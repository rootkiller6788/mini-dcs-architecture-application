/**
 * @file state_sync.h
 * @brief State Synchronization Protocols for Redundant Controllers
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover (7. mini-dcs-architecture-application)
 *
 * Knowledge Coverage:
 *   L3 - Engineering structures: delta sync, full sync, checksum sync
 *   L5 - Algorithms: sequence number tracking, version vectors, delta compression
 *   L2 - Core concepts: state consistency, eventual consistency
 *
 * Reference:
 *   - Schneider, "Implementing Fault-Tolerant Services Using the State
 *     Machine Approach" (1990), ACM Computing Surveys
 *   - ABB 800xA Redundancy Technical Reference
 */

#ifndef STATE_SYNC_H
#define STATE_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define STATE_SYNC_MAX_BLOCK_SIZE 4096
#define STATE_SYNC_MAX_REGIONS 64
#define STATE_SYNC_CHECKSUM_LEN 4
#define STATE_SYNC_VERSION_VECTOR_SIZE 8

typedef enum {
    SYNC_METHOD_FULL        = 0,
    SYNC_METHOD_INCREMENTAL = 1,
    SYNC_METHOD_CHECKSUM    = 2,
    SYNC_METHOD_DELTA       = 3,
    SYNC_METHOD_VERSIONED   = 4
} sync_method_t;

typedef enum {
    SYNC_STATE_IDLE         = 0,
    SYNC_STATE_REQUESTING   = 1,
    SYNC_STATE_TRANSFERRING = 2,
    SYNC_STATE_VERIFYING    = 3,
    SYNC_STATE_COMPLETE     = 4,
    SYNC_STATE_FAILED       = 5
} sync_state_t;

typedef struct {
    uint32_t region_id;
    uint32_t base_address;
    uint32_t size_bytes;
    uint32_t sequence_number;
    uint32_t checksum;
    bool     dirty;
} sync_region_t;

typedef struct {
    uint32_t version[STATE_SYNC_VERSION_VECTOR_SIZE];
    uint8_t  n_nodes;
} version_vector_t;

typedef struct {
    sync_region_t regions[STATE_SYNC_MAX_REGIONS];
    uint8_t       n_regions;
    sync_method_t method;
    sync_state_t  state;
    uint32_t      total_bytes_synced;
    uint32_t      sync_interval_ms;
    uint32_t      last_sync_time_ms;
    bool          consistency_verified;
    version_vector_t vclock;
    uint32_t      delta_threshold_bytes;
    double        compression_ratio;
} state_sync_manager_t;

/**
 * Initialize the state synchronization manager.
 * @param ssm             Sync manager
 * @param method          Synchronization method
 * @param sync_interval_ms Periodic sync interval
 * @return                0 on success, -1 on error
 * Complexity: O(1)
 */
int state_sync_init(state_sync_manager_t *ssm,
                    sync_method_t method,
                    uint32_t sync_interval_ms);

/**
 * Register a memory region for synchronization tracking.
 * @param ssm         Sync manager
 * @param region_id   Unique region identifier
 * @param base_address Starting address in controller memory
 * @param size_bytes  Size of the region in bytes
 * @return            0 on success, -1 on error
 * Complexity: O(1)
 */
int state_sync_register_region(state_sync_manager_t *ssm,
                               uint32_t region_id,
                               uint32_t base_address,
                               uint32_t size_bytes);

/**
 * Mark a memory region as dirty (modified) requiring synchronization.
 * @param ssm       Sync manager
 * @param region_id Region to mark dirty
 * @return          0 on success, -1 if region not found
 * Complexity: O(R) where R = n_regions
 */
int state_sync_mark_dirty(state_sync_manager_t *ssm, uint32_t region_id);

/**
 * Compute the total number of dirty bytes needing synchronization.
 * @param ssm Sync manager
 * @return     Total dirty bytes
 * Complexity: O(R)
 */
uint32_t state_sync_dirty_bytes(const state_sync_manager_t *ssm);

/**
 * Compute a CRC-32 checksum for a region to detect changes.
 * @param data       Data buffer
 * @param size_bytes Size in bytes
 * @return           CRC-32 checksum
 * Complexity: O(N) where N = size_bytes
 */
uint32_t state_sync_compute_checksum(const uint8_t *data, size_t size_bytes);

/**
 * Perform a full state transfer: copy all registered regions.
 * @param ssm        Sync manager
 * @param src_data   Source data buffer (from primary)
 * @param src_size   Source data size
 * @param dst_data   Destination data buffer (to secondary)
 * @param dst_size   Destination buffer size
 * @return           0 on success, -1 on error
 * Complexity: O(N) where N = total registered bytes
 */
int state_sync_full_transfer(state_sync_manager_t *ssm,
                             const uint8_t *src_data, size_t src_size,
                             uint8_t *dst_data, size_t dst_size);

/**
 * Perform an incremental sync: only transfer dirty regions.
 * The sequence number is used to determine what has changed.
 * @param ssm        Sync manager
 * @param src_data   Source data buffer
 * @param src_size   Source data size
 * @param dst_data   Destination data buffer
 * @param dst_size   Destination buffer size
 * @return           0 on success, -1 on error
 * Complexity: O(D) where D = total dirty bytes
 */
int state_sync_incremental_transfer(state_sync_manager_t *ssm,
                                    const uint8_t *src_data, size_t src_size,
                                    uint8_t *dst_data, size_t dst_size);

/**
 * Perform checksum-based sync: compare region checksums and transfer
 * only regions where checksums differ.
 * @param ssm        Sync manager
 * @param src_data   Source data buffer
 * @param src_size   Source data size
 * @param dst_data   Destination data buffer
 * @param dst_size   Destination buffer size
 * @return           0 on success, -1 on error
 * Complexity: O(R + D) where R = regions, D = differing bytes
 */
int state_sync_checksum_transfer(state_sync_manager_t *ssm,
                                 const uint8_t *src_data, size_t src_size,
                                 uint8_t *dst_data, size_t dst_size);

/**
 * Perform delta sync: compute byte-level differences and transfer
 * only the changed bytes using a simple delta encoding.
 *
 * Delta encoding: [offset_16bit][length_16bit][data_bytes]...
 * @param ssm        Sync manager
 * @param src_data   Source data buffer
 * @param src_size   Source data size
 * @param dst_data   Destination data buffer
 * @param dst_size   Destination buffer size
 * @return           Number of delta bytes transferred, -1 on error
 * Complexity: O(N) where N = src_size
 */
int state_sync_delta_transfer(state_sync_manager_t *ssm,
                              const uint8_t *src_data, size_t src_size,
                              uint8_t *dst_data, size_t dst_size);

/**
 * Verify consistency between source and destination data.
 * Compares checksums of all registered regions.
 * @param ssm       Sync manager
 * @param data_a    First data buffer
 * @param size_a    First buffer size
 * @param data_b    Second data buffer
 * @param size_b    Second buffer size
 * @return          true if all regions match
 * Complexity: O(N) where N = total registered bytes
 */
bool state_sync_verify_consistency(const state_sync_manager_t *ssm,
                                   const uint8_t *data_a, size_t size_a,
                                   const uint8_t *data_b, size_t size_b);

/**
 * Initialize a version vector for tracking causal ordering.
 * Version vectors detect conflicts in distributed state.
 * @param vv      Version vector
 * @param n_nodes Number of participating nodes
 * Complexity: O(V) where V = vector size
 */
void state_sync_version_vector_init(version_vector_t *vv, uint8_t n_nodes);

/**
 * Increment the version for a specific node.
 * @param vv      Version vector
 * @param node_id Node to increment
 * Complexity: O(1)
 */
void state_sync_version_vector_increment(version_vector_t *vv, uint8_t node_id);

/**
 * Compare two version vectors for ordering.
 * @param a First version vector
 * @param b Second version vector
 * @return  <0 if a < b, 0 if concurrent or equal, >0 if a > b
 * Complexity: O(V)
 */
int state_sync_version_vector_compare(const version_vector_t *a,
                                      const version_vector_t *b);

/**
 * Merge two version vectors (element-wise max).
 * @param dst     Destination (will be updated)
 * @param src     Source to merge from
 * Complexity: O(V)
 */
void state_sync_version_vector_merge(version_vector_t *dst,
                                     const version_vector_t *src);

/**
 * Compute the compression ratio achieved by delta sync vs full transfer.
 * @param ssm Sync manager
 * @return     Compression ratio (0..1, where 1 = no compression)
 * Complexity: O(1)
 */
double state_sync_compression_ratio(const state_sync_manager_t *ssm);

/**
 * Estimate the time required for a full state transfer.
 * @param ssm         Sync manager
 * @param bandwidth_bps Network bandwidth in bytes per second
 * @return             Estimated transfer time in milliseconds
 * Complexity: O(R)
 */
uint32_t state_sync_estimate_transfer_time(const state_sync_manager_t *ssm,
                                           double bandwidth_bps);

/**
 * Sequence number comparison for wrap-around detection.
 * Uses the standard technique: if |a-b| < 2^31, compare directly.
 * Otherwise, the smaller (with wrap) is considered newer.
 * @param a First sequence number
 * @param b Second sequence number
 * @return  <0 if a < b, 0 if equal, >0 if a > b
 * Complexity: O(1)
 */
int state_sync_seq_compare(uint32_t a, uint32_t b);

#endif /* STATE_SYNC_H */
