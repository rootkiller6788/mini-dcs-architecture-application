/**
 * @file state_sync.c
 * @brief State Synchronization Protocols Implementation
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * Knowledge Coverage:
 *   L3 - Engineering structures: memory region tracking, delta encoding
 *   L5 - Algorithms: CRC-32, version vectors, delta compression
 *   L2 - State consistency, eventual consistency between controllers
 *
 * Reference:
 *   Schneider, "Implementing Fault-Tolerant Services Using the State
 *     Machine Approach" (1990), ACM Computing Surveys
 *   ABB 800xA Redundancy Technical Reference (synchronization methods)
 *   Emerson DeltaV Redundancy — peer-to-peer state sync
 */

#include "state_sync.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * L3: State Sync Manager Initialization
 * ============================================================================
 * Knowledge: State synchronization ensures that the secondary
 * (backup) controller has an up-to-date copy of the primary's
 * state. This is essential for bumpless failover.
 *
 * Methods:
 *   - Full sync: Transfer all state (simplest, highest bandwidth)
 *   - Incremental: Transfer only dirty (modified) regions
 *   - Checksum: Compare checksums, transfer only mismatched regions
 *   - Delta: Transfer only changed bytes (highest compression)
 *   - Versioned: Use version vectors to detect conflicts
 */

int state_sync_init(state_sync_manager_t *ssm,
                    sync_method_t method,
                    uint32_t sync_interval_ms)
{
    if (!ssm) return -1;
    if (sync_interval_ms < 1) return -1;

    memset(ssm, 0, sizeof(*ssm));
    ssm->method = method;
    ssm->sync_interval_ms = sync_interval_ms;
    ssm->state = SYNC_STATE_IDLE;
    ssm->consistency_verified = false;
    ssm->delta_threshold_bytes = 256;
    ssm->compression_ratio = 1.0;

    state_sync_version_vector_init(&ssm->vclock, 2);  /* Primary + Secondary */
    return 0;
}

/* ============================================================================
 * L3: Memory Region Registration
 * ============================================================================
 * Knowledge: A redundant controller's state is divided into memory
 * regions, each tracking:
 *   - base_address: starting offset in the state data buffer
 *   - size_bytes: size of this region
 *   - sequence_number: incremented on each modification
 *   - checksum: CRC-32 of the region content
 *   - dirty: flag set when the region is modified
 *
 * Regions typically include: PID block data, alarm states,
 * I/O mapping, configuration parameters, sequence states.
 */

int state_sync_register_region(state_sync_manager_t *ssm,
                               uint32_t region_id,
                               uint32_t base_address,
                               uint32_t size_bytes)
{
    if (!ssm) return -1;
    if (ssm->n_regions >= STATE_SYNC_MAX_REGIONS) return -1;
    if (size_bytes == 0 || size_bytes > STATE_SYNC_MAX_BLOCK_SIZE) return -1;

    sync_region_t *r = &ssm->regions[ssm->n_regions];
    r->region_id = region_id;
    r->base_address = base_address;
    r->size_bytes = size_bytes;
    r->sequence_number = 0;
    r->checksum = 0;
    r->dirty = false;

    ssm->n_regions++;
    return 0;
}

int state_sync_mark_dirty(state_sync_manager_t *ssm, uint32_t region_id)
{
    if (!ssm) return -1;

    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        if (ssm->regions[i].region_id == region_id) {
            ssm->regions[i].dirty = true;
            ssm->regions[i].sequence_number++;
            return 0;
        }
    }
    return -1;  /* Region not found */
}

uint32_t state_sync_dirty_bytes(const state_sync_manager_t *ssm)
{
    if (!ssm) return 0;
    uint32_t total = 0;
    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        if (ssm->regions[i].dirty) {
            total += ssm->regions[i].size_bytes;
        }
    }
    return total;
}

/* ============================================================================
 * L5: CRC-32 Checksum
 * ============================================================================
 * Knowledge: CRC-32 (Cyclic Redundancy Check) is used to detect
 * accidental changes to raw data. The polynomial 0xEDB88320 is the
 * reversed form of 0x04C11DB7 (IEEE 802.3 / Ethernet CRC-32).
 *
 * CRC is widely used in industrial protocols (Modbus RTU uses CRC-16,
 * Ethernet uses CRC-32) and in memory integrity checking for
 * safety-critical systems (IEC 61508-2 Table A.2).
 */

uint32_t state_sync_compute_checksum(const uint8_t *data, size_t size_bytes)
{
    if (!data || size_bytes == 0) return 0;

    uint32_t crc = 0xFFFFFFFF;
    const uint32_t polynomial = 0xEDB88320;

    for (size_t i = 0; i < size_bytes; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

/* ============================================================================
 * L5: Full State Transfer
 * ============================================================================
 * Knowledge: Full state transfer copies the entire state buffer from
 * the primary to the secondary. This is the simplest approach and is
 * used during initial synchronization or after a controller reboot.
 *
 * Bandwidth requirement: B = state_size / sync_interval
 * For a typical DCS controller with 256KB state and 100ms sync
 * interval: B = 256 * 1024 / 0.1 ≈ 2.6 MB/s (well within backplane
 * capacity of 100+ MB/s).
 */

int state_sync_full_transfer(state_sync_manager_t *ssm,
                             const uint8_t *src_data, size_t src_size,
                             uint8_t *dst_data, size_t dst_size)
{
    if (!ssm || !src_data || !dst_data) return -1;

    /* Compute total size of all registered regions */
    uint32_t total = 0;
    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        total += ssm->regions[i].size_bytes;
    }

    if (src_size < total || dst_size < total) return -1;

    ssm->state = SYNC_STATE_TRANSFERRING;

    /* Copy each registered region */
    uint32_t offset = 0;
    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        sync_region_t *r = &ssm->regions[i];
        if (r->base_address + r->size_bytes > src_size
            || r->base_address + r->size_bytes > dst_size) {
            ssm->state = SYNC_STATE_FAILED;
            return -1;
        }
        memcpy(dst_data + r->base_address,
               src_data + r->base_address,
               r->size_bytes);

        /* Update checksum and clear dirty */
        r->checksum = state_sync_compute_checksum(
            src_data + r->base_address, r->size_bytes);
        r->dirty = false;
        offset += r->size_bytes;
    }

    ssm->total_bytes_synced += total;
    ssm->last_sync_time_ms = ssm->total_bytes_synced;  /* Using as timestamp proxy */
    ssm->state = SYNC_STATE_COMPLETE;
    ssm->compression_ratio = 1.0;  /* No compression in full sync */

    return 0;
}

/* ============================================================================
 * L5: Incremental State Transfer
 * ============================================================================
 * Knowledge: Incremental sync transfers only the memory regions that
 * have been marked as dirty (modified since the last sync). This
 * dramatically reduces bandwidth when only a small fraction of the
 * state changes per cycle.
 *
 * Typical DCS: ~5-10% of state changes per scan cycle
 * Bandwidth saving: 90-95% vs full sync
 */

int state_sync_incremental_transfer(state_sync_manager_t *ssm,
                                    const uint8_t *src_data, size_t src_size,
                                    uint8_t *dst_data, size_t dst_size)
{
    if (!ssm || !src_data || !dst_data) return -1;

    ssm->state = SYNC_STATE_TRANSFERRING;
    uint32_t dirty_total = 0;

    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        sync_region_t *r = &ssm->regions[i];
        if (!r->dirty) continue;

        if (r->base_address + r->size_bytes > src_size
            || r->base_address + r->size_bytes > dst_size) {
            ssm->state = SYNC_STATE_FAILED;
            return -1;
        }

        memcpy(dst_data + r->base_address,
               src_data + r->base_address,
               r->size_bytes);

        r->checksum = state_sync_compute_checksum(
            src_data + r->base_address, r->size_bytes);
        r->dirty = false;
        dirty_total += r->size_bytes;
    }

    ssm->total_bytes_synced += dirty_total;
    ssm->state = SYNC_STATE_COMPLETE;
    ssm->compression_ratio = 0.0;

    return 0;
}

/* ============================================================================
 * L5: Checksum-Based Sync
 * ============================================================================
 * Knowledge: Checksum-based sync compares CRC-32 hashes of each region
 * between primary and secondary. Only regions with mismatched checksums
 * are transferred. This avoids the need for explicit dirty tracking,
 * at the cost of computing checksums on both sides.
 */

int state_sync_checksum_transfer(state_sync_manager_t *ssm,
                                 const uint8_t *src_data, size_t src_size,
                                 uint8_t *dst_data, size_t dst_size)
{
    if (!ssm || !src_data || !dst_data) return -1;

    ssm->state = SYNC_STATE_TRANSFERRING;
    uint32_t transferred = 0;

    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        sync_region_t *r = &ssm->regions[i];

        if (r->base_address + r->size_bytes > src_size
            || r->base_address + r->size_bytes > dst_size) {
            ssm->state = SYNC_STATE_FAILED;
            return -1;
        }

        /* Compute checksums for both sides */
        uint32_t src_crc = state_sync_compute_checksum(
            src_data + r->base_address, r->size_bytes);
        uint32_t dst_crc = state_sync_compute_checksum(
            dst_data + r->base_address, r->size_bytes);

        /* Transfer if checksums differ */
        if (src_crc != dst_crc) {
            memcpy(dst_data + r->base_address,
                   src_data + r->base_address,
                   r->size_bytes);
            transferred += r->size_bytes;
        }

        /* Update source checksum */
        r->checksum = src_crc;
        r->dirty = false;
    }

    ssm->total_bytes_synced += transferred;
    ssm->state = SYNC_STATE_COMPLETE;
    ssm->compression_ratio = (transferred > 0) ? 1.0 : 0.0;

    return 0;
}

/* ============================================================================
 * L5: Delta Sync — Byte-Level Differencing
 * ============================================================================
 * Knowledge: Delta synchronization computes the byte-level differences
 * between source and destination buffers and transmits only the changed
 * bytes with their offsets.
 *
 * Delta encoding format: [offset(16bit)][length(16bit)][data bytes]...
 * Each chunk header is 4 bytes. For small changes (a few bytes here
 * and there), this is more efficient than transferring entire regions.
 *
 * Compression ratio = delta_bytes / total_region_bytes
 */

int state_sync_delta_transfer(state_sync_manager_t *ssm,
                              const uint8_t *src_data, size_t src_size,
                              uint8_t *dst_data, size_t dst_size)
{
    if (!ssm || !src_data || !dst_data) return -1;

    ssm->state = SYNC_STATE_TRANSFERRING;
    uint32_t total_region_bytes = 0;
    uint32_t delta_bytes = 0;

    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        sync_region_t *r = &ssm->regions[i];
        uint32_t base = r->base_address;
        uint32_t size = r->size_bytes;

        if (base + size > src_size || base + size > dst_size) {
            ssm->state = SYNC_STATE_FAILED;
            return -1;
        }

        total_region_bytes += size;

        /* Find changed byte ranges */
        uint32_t chunk_start = 0;
        bool in_chunk = false;

        for (uint32_t j = 0; j < size; j++) {
            bool differs = (src_data[base + j] != dst_data[base + j]);

            if (differs && !in_chunk) {
                chunk_start = j;
                in_chunk = true;
            } else if (!differs && in_chunk) {
                /* End of chunk: copy from src to dst */
                uint32_t chunk_len = j - chunk_start;
                memcpy(dst_data + base + chunk_start,
                       src_data + base + chunk_start,
                       chunk_len);
                delta_bytes += chunk_len;
                in_chunk = false;
            }
        }

        /* Handle open chunk at end of region */
        if (in_chunk) {
            uint32_t chunk_len = size - chunk_start;
            memcpy(dst_data + base + chunk_start,
                   src_data + base + chunk_start,
                   chunk_len);
            delta_bytes += chunk_len;
        }

        /* Update checksum */
        r->checksum = state_sync_compute_checksum(
            src_data + base, r->size_bytes);
        r->dirty = false;
    }

    ssm->total_bytes_synced += delta_bytes;
    ssm->state = SYNC_STATE_COMPLETE;
    ssm->compression_ratio = (total_region_bytes > 0)
        ? ((double)delta_bytes / (double)total_region_bytes) : 0.0;

    return (int)delta_bytes;
}

/* ============================================================================
 * L2: Consistency Verification
 * ============================================================================
 * Knowledge: After synchronization, the primary and secondary must
 * verify that their states are identical. This is done by comparing
 * checksums of all registered regions. A mismatch indicates a
 * synchronization fault requiring a retransfer.
 */

bool state_sync_verify_consistency(const state_sync_manager_t *ssm,
                                   const uint8_t *data_a, size_t size_a,
                                   const uint8_t *data_b, size_t size_b)
{
    if (!ssm || !data_a || !data_b) return false;

    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        const sync_region_t *r = &ssm->regions[i];
        uint32_t base = r->base_address;
        uint32_t size = r->size_bytes;

        if (base + size > size_a || base + size > size_b) return false;

        uint32_t crc_a = state_sync_compute_checksum(data_a + base, size);
        uint32_t crc_b = state_sync_compute_checksum(data_b + base, size);

        if (crc_a != crc_b) return false;
    }

    return true;
}

/* ============================================================================
 * L5: Version Vectors
 * ============================================================================
 * Knowledge: Version vectors track causal relationships between
 * updates in distributed systems. Each node maintains a vector
 * where element [i] is the number of updates seen from node i.
 *
 * Comparison rules (Parker et al., 1983):
 *   a < b iff all(a_i <= b_i) AND exists(a_j < b_j)     [a happened-before b]
 *   a || b iff NOT(a <= b) AND NOT(b <= a)              [concurrent]
 *   a == b iff all(a_i == b_i)
 *
 * Merging: take element-wise maximum (last-writer-wins for concurrent)
 */

void state_sync_version_vector_init(version_vector_t *vv, uint8_t n_nodes)
{
    if (!vv) return;
    memset(vv->version, 0, sizeof(vv->version));
    vv->n_nodes = n_nodes;
}

void state_sync_version_vector_increment(version_vector_t *vv, uint8_t node_id)
{
    if (!vv || node_id >= vv->n_nodes
        || node_id >= STATE_SYNC_VERSION_VECTOR_SIZE) return;
    vv->version[node_id]++;
}

int state_sync_version_vector_compare(const version_vector_t *a,
                                      const version_vector_t *b)
{
    if (!a || !b) return 0;
    if (a->n_nodes != b->n_nodes) return 0;  /* Incomparable */

    bool a_leq_b = true;
    bool b_leq_a = true;
    bool any_strict_a = false;
    bool any_strict_b = false;

    for (uint8_t i = 0; i < a->n_nodes; i++) {
        if (a->version[i] > b->version[i]) {
            a_leq_b = false;
            any_strict_a = true;
        }
        if (b->version[i] > a->version[i]) {
            b_leq_a = false;
            any_strict_b = true;
        }
    }

    if (a_leq_b && any_strict_b) return -1;  /* a < b */
    if (b_leq_a && any_strict_a) return 1;   /* a > b */
    if (a_leq_b && b_leq_a) return 0;        /* a == b */
    return 0;  /* Concurrent */
}

void state_sync_version_vector_merge(version_vector_t *dst,
                                     const version_vector_t *src)
{
    if (!dst || !src) return;
    uint8_t n = (dst->n_nodes < src->n_nodes) ? dst->n_nodes : src->n_nodes;

    for (uint8_t i = 0; i < n && i < STATE_SYNC_VERSION_VECTOR_SIZE; i++) {
        if (src->version[i] > dst->version[i]) {
            dst->version[i] = src->version[i];
        }
    }
}

/* ============================================================================
 * L5: Diagnostic Helpers
 * ============================================================================
 */

double state_sync_compression_ratio(const state_sync_manager_t *ssm)
{
    if (!ssm) return 1.0;
    return ssm->compression_ratio;
}

uint32_t state_sync_estimate_transfer_time(const state_sync_manager_t *ssm,
                                           double bandwidth_bps)
{
    if (!ssm || bandwidth_bps <= 0.0) return 0;

    uint32_t total = 0;
    for (uint8_t i = 0; i < ssm->n_regions; i++) {
        total += ssm->regions[i].size_bytes;
    }

    /* Transfer time in ms:
     *   t_ms = total_bytes / (bandwidth_bps / 8) * 1000
     */
    return (uint32_t)((double)total / (bandwidth_bps / 8.0) * 1000.0);
}

int state_sync_seq_compare(uint32_t a, uint32_t b)
{
    /* RFC 1982: Serial Number Arithmetic */
    int32_t diff = (int32_t)(a - b);
    if (diff > 0) return 1;
    if (diff < 0) return -1;
    return 0;
}
