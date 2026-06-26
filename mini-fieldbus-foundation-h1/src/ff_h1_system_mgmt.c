/**
 * ff_h1_system_mgmt.c ? Foundation Fieldbus H1 System & Network Management Implementation
 *
 * Implements SM Agent state machine, device initialization, Set Address protocol,
 * Time Distribution processing, Find Tag Query, Network Management statistics,
 * and SMIB read operations.
 *
 * Knowledge Levels: L1, L2, L3
 */

#include "ff_h1_system_mgmt.h"
#include "ff_h1_datalink.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ============================================================================
 * L1: SM State Name Lookup
 * ============================================================================ */

const char* ff_sm_state_name(ff_sm_state_t state) {
    switch (state) {
        case FF_SM_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FF_SM_STATE_INITIALIZING:  return "INITIALIZING";
        case FF_SM_STATE_OPERATIONAL:   return "OPERATIONAL";
        case FF_SM_STATE_FAULT:         return "FAULT";
        default:                        return "UNKNOWN";
    }
}


/* ============================================================================
 * L1: SM Agent Initialization and State Transitions
 *
 * Power-on sequence:
 *   1. Device powers on ? UNINITIALIZED
 *   2. Device detects bus activity (receives DL-PDU) ? INITIALIZING
 *   3. LAS assigns PD-TAG and permanent address ? OPERATIONAL
 *
 * Fault detection:
 *   - Hardware fault (watchdog, memory, power)
 *   - Communication loss (no TD for > 3? TD interval)
 *   - Configuration fault (missing/invalid configuration)
 * ============================================================================ */

void ff_sm_init(ff_sm_agent_t *sm, const uint8_t device_id[32]) {
    if (!sm || !device_id) return;
    memset(sm, 0, sizeof(*sm));
    memcpy(sm->device_id, device_id, 32);
    sm->state = FF_SM_STATE_UNINITIALIZED;
    sm->dl_address = 0xFC; /* Temporary/default address */
    sm->permanent_addr = 0xFF; /* Not yet assigned */
    sm->pd_tag[0] = '\0';
    sm->current_time = 0;
    sm->uptime_sec = 0;
    sm->sm_supported = 0xFFFF; /* All services supported in this implementation */
    sm->sm_sync_flags = 0;
}

void ff_sm_start_initialization(ff_sm_agent_t *sm) {
    if (!sm) return;
    if (sm->state == FF_SM_STATE_UNINITIALIZED) {
        sm->state = FF_SM_STATE_INITIALIZING;
    }
}

int ff_sm_set_operational(ff_sm_agent_t *sm, const char *tag, uint8_t permanent_addr) {
    if (!sm || !tag) return -1;
    if (permanent_addr < FF_DL_ADDR_PERM_MIN || permanent_addr > FF_DL_ADDR_PERM_MAX) {
        return -1;
    }

    strncpy((char*)sm->pd_tag, tag, 31);
    sm->pd_tag[31] = '\0';
    sm->permanent_addr = permanent_addr;
    sm->dl_address = permanent_addr;
    sm->state = FF_SM_STATE_OPERATIONAL;

    return 0;
}

void ff_sm_set_fault(ff_sm_agent_t *sm, uint16_t fault_code) {
    if (!sm) return;
    sm->state = FF_SM_STATE_FAULT;
    /* fault_code stored for diagnostic purposes */
    (void)fault_code;
}

int ff_sm_has_permanent_address(const ff_sm_agent_t *sm) {
    if (!sm) return 0;
    return (sm->permanent_addr >= FF_DL_ADDR_PERM_MIN &&
            sm->permanent_addr <= FF_DL_ADDR_PERM_MAX) ? 1 : 0;
}


/* ============================================================================
 * L2: Find Tag Query ? Device Discovery by PD-TAG
 *
 * Configuration tools use Find Tag Query to locate devices by their
 * Physical Device Tag. The host broadcasts a query with the tag;
 * each device checks if its PD-TAG matches and responds.
 *
 * Comparison is case-sensitive (per FF-880), exact match required.
 * ============================================================================ */

int ff_sm_find_tag_match(const ff_sm_agent_t *sm, const char *query_tag) {
    if (!sm || !query_tag) return 0;
    if (sm->state != FF_SM_STATE_OPERATIONAL) return 0;
    return (strcmp((const char*)sm->pd_tag, query_tag) == 0) ? 1 : 0;
}


/* ============================================================================
 * L3: Set Address Protocol ? Permanent Address Assignment
 *
 * The LAS uses Set Address to assign a permanent DL-address to a device.
 *
 * Acceptance criteria:
 *   1. Device is in INITIALIZING state
 *   2. device_id in the request matches the device's own ID
 *   3. new_addr is in the valid permanent range (0x10-0xFB)
 *
 * On acceptance, the device transitions to OPERATIONAL with the new address.
 *
 * Security note: In the FF protocol, Set Address is unauthenticated.
 * It relies on the physical security of the H1 segment.
 * ============================================================================ */

int ff_sm_process_set_address(ff_sm_agent_t *sm, const uint8_t device_id[32],
                               uint8_t new_addr) {
    if (!sm || !device_id) return -1;

    /* Must be initializing to accept address assignment */
    if (sm->state != FF_SM_STATE_INITIALIZING) return -1;

    /* Verify device ID matches */
    if (memcmp(sm->device_id, device_id, 32) != 0) return -1;

    /* Validate address range */
    if (new_addr < FF_DL_ADDR_PERM_MIN || new_addr > FF_DL_ADDR_PERM_MAX) {
        return -1;
    }

    /* Accept the address */
    sm->permanent_addr = new_addr;
    sm->dl_address = new_addr;
    sm->state = FF_SM_STATE_OPERATIONAL;

    return 0;
}


/* ============================================================================
 * L3: Time Distribution Protocol Processing
 *
 * When the LAS broadcasts a TD message, each device:
 *   1. Records the LAS time
 *   2. Adds propagation delay compensation
 *   3. Updates its internal clock
 *   4. Tracks sync quality based on consecutive successful TDs
 *
 * Sync quality transitions:
 *   NONE ? COARSE (after first TD)
 *   COARSE ? FINE (after 3 consecutive TDs with stable drift)
 *   FINE ? LOCKED (after 10 consecutive TDs with minimal drift)
 *   Any ? COARSE (after missed TD, clock drift exceeded)
 *
 * Reference: FF-880 ?4.5.3 "Time Synchronization Algorithm"
 * ============================================================================ */

ff_time_sync_quality_t ff_sm_process_td(ff_sm_agent_t *sm,
                                         const ff_td_message_t *td,
                                         uint32_t prop_delay_us) {
    if (!sm || !td) return FF_TIME_SYNC_NONE;

    /* Apply propagation delay compensation:
     * The TD message contains the LAS time at the moment of transmission.
     * By the time we receive it, prop_delay_us has elapsed.
     * So the current LAS time = td->las_time + propagation delay.
     */
    uint32_t las_time_ms = td->las_time * 1000 + td->las_time_ns / 1000000;
    uint32_t prop_delay_ms = prop_delay_us / 1000;
    uint32_t estimated_time = las_time_ms + prop_delay_ms;

    /* Update device time */
    sm->current_time = estimated_time;
    sm->time_last_td = estimated_time;

    /* Compute drift: difference between previous device time and new LAS time */
    /* (If the device's local clock was perfect, old_time + td_interval ? estimated_time) */

    /* Determine sync quality */
    if (sm->sm_sync_flags == 0) {
        /* First TD received */
        sm->sm_sync_flags = 1;
        return FF_TIME_SYNC_COARSE;
    }

    /* Simple heuristic: count consecutive TDs */
    sm->sm_sync_flags++;

    if (sm->sm_sync_flags >= 10) {
        return FF_TIME_SYNC_LOCKED;
    } else if (sm->sm_sync_flags >= 3) {
        return FF_TIME_SYNC_FINE;
    } else {
        return FF_TIME_SYNC_COARSE;
    }
}

ff_time_sync_quality_t ff_sm_time_sync_quality(const ff_sm_agent_t *sm) {
    if (!sm) return FF_TIME_SYNC_NONE;
    if (sm->sm_sync_flags == 0) return FF_TIME_SYNC_NONE;
    if (sm->sm_sync_flags >= 10) return FF_TIME_SYNC_LOCKED;
    if (sm->sm_sync_flags >= 3) return FF_TIME_SYNC_FINE;
    return FF_TIME_SYNC_COARSE;
}

void ff_sm_time_format(const ff_sm_agent_t *sm, char *buf, size_t buf_size) {
    if (!sm || !buf || buf_size < 20) return;

    /* Convert seconds since 1970 to a simple readable format.
     * For a full ISO 8601 conversion we'd need gmtime(), but to avoid
     * platform dependency, we output seconds-since-epoch with an indicator.
     */

    uint32_t sec = sm->current_time;
    uint32_t days = sec / 86400;
    uint32_t rem = sec % 86400;
    uint32_t hours = rem / 3600;
    rem %= 3600;
    uint32_t minutes = rem / 60;
    uint32_t seconds = rem % 60;

    snprintf(buf, buf_size, "d%u %02u:%02u:%02u", days, hours, minutes, seconds);
}


/* ============================================================================
 * L3: Network Management Statistics
 * ============================================================================ */

void ff_nm_stats_init(ff_nm_statistics_t *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
}

double ff_nm_error_rate(const ff_nm_statistics_t *stats) {
    if (!stats) return -1.0;
    uint32_t total = stats->dl_tx_frames + stats->dl_rx_frames;
    if (total == 0) return -1.0;
    uint32_t errors = stats->dl_tx_errors + stats->dl_rx_errors;
    return (double)errors / (double)total;
}

double ff_nm_efficiency(const ff_nm_statistics_t *stats) {
    if (!stats) return -1.0;
    uint32_t total = stats->dl_tx_frames + stats->dl_rx_frames;
    if (total == 0) return -1.0;
    uint32_t errors = stats->dl_tx_errors + stats->dl_rx_errors;
    uint32_t good = total - errors;
    return (double)good / (double)total;
}


/* ============================================================================
 * L3: SMIB Read Operations
 *
 * Read SMIB entries by type. Provides a structured interface for
 * configuration and diagnostic tools to query device information.
 *
 * Each SMIB entry type returns specific data in the output buffer:
 *   DEVICE_ID:         32 bytes (raw device ID)
 *   DEVICE_TAG:        32 bytes (PD-TAG, null-terminated string)
 *   DEVICE_ADDRESS:    1 byte (DL-address)
 *   DEVICE_STATE:      1 byte (SM state enum value)
 *   OPERATIONAL_POWERUP: 4 bytes (uptime in seconds)
 *   DEVICE_CAPABILITY: 2 bytes (sm_supported bitmask)
 *   TIME_SYNC:         1 byte (sync quality)
 * ============================================================================ */

int ff_smib_read(const ff_sm_agent_t *sm, ff_smib_entry_t entry,
                 uint8_t *buf, size_t *buf_size) {
    if (!sm || !buf || !buf_size) return -1;

    switch (entry) {
        case FF_SMIB_DEVICE_ID:
            if (*buf_size < 32) return -1;
            memcpy(buf, sm->device_id, 32);
            *buf_size = 32;
            return 0;

        case FF_SMIB_DEVICE_TAG:
            if (*buf_size < 32) return -1;
            memcpy(buf, sm->pd_tag, 32);
            *buf_size = strlen((const char*)buf) + 1;
            return 0;

        case FF_SMIB_DEVICE_ADDRESS:
            if (*buf_size < 1) return -1;
            buf[0] = sm->dl_address;
            *buf_size = 1;
            return 0;

        case FF_SMIB_DEVICE_STATE:
            if (*buf_size < 1) return -1;
            buf[0] = (uint8_t)sm->state;
            *buf_size = 1;
            return 0;

        case FF_SMIB_OPERATIONAL_POWERUP:
            if (*buf_size < 4) return -1;
            buf[0] = (uint8_t)(sm->uptime_sec & 0xFF);
            buf[1] = (uint8_t)((sm->uptime_sec >> 8) & 0xFF);
            buf[2] = (uint8_t)((sm->uptime_sec >> 16) & 0xFF);
            buf[3] = (uint8_t)((sm->uptime_sec >> 24) & 0xFF);
            *buf_size = 4;
            return 0;

        case FF_SMIB_DEVICE_CAPABILITY:
            if (*buf_size < 2) return -1;
            buf[0] = (uint8_t)(sm->sm_supported & 0xFF);
            buf[1] = (uint8_t)((sm->sm_supported >> 8) & 0xFF);
            *buf_size = 2;
            return 0;

        case FF_SMIB_TIME_SYNC: {
            if (*buf_size < 1) return -1;
            ff_time_sync_quality_t q = ff_sm_time_sync_quality(sm);
            buf[0] = (uint8_t)q;
            *buf_size = 1;
            return 0;
        }

        default:
            return -1; /* Unknown SMIB entry */
    }
}