/**
 * ff_h1_system_mgmt.h ? Foundation Fieldbus H1 System & Network Management
 *
 * Implements System Management (SM) agent state machine, Network Management (NM)
 * VCR state machines, device address assignment (Set Address), Find Tag Query,
 * time synchronization, and SMIB (System Management Information Base).
 *
 * Course Mapping:
 *   CMU 24-677  ? Distributed system management, state machines
 *   Berkeley EECS C128 ? Mechatronics system management
 *   ISA/IEC   ? IEC 61158-4 (SM/NM), FF-880 System Management Specification
 *
 * Knowledge Levels: L1 (Definitions), L2 (Core Concepts), L3 (Structures)
 */

#ifndef FF_H1_SYSTEM_MGMT_H
#define FF_H1_SYSTEM_MGMT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: System Management (SM) ? Device Operational States
 *
 * Foundation Fieldbus SM controls the lifecycle of a device on the H1 segment.
 * Key responsibilities:
 *   - Device tag assignment and management
 *   - Address assignment (Set Address protocol)
 *   - Find Tag Query service
 *   - Time distribution to application
 *   - Device identification
 *
 * SM Agent State Machine (IEC 61158-4, FF-880):
 *   UNINITIALIZED  INITIALIZING  OPERATIONAL  FAULT
 * ============================================================================ */

typedef enum {
    FF_SM_STATE_UNINITIALIZED = 0, /**< No tag, no permanent address, just powered on */
    FF_SM_STATE_INITIALIZING  = 1, /**< Obtaining tag and permanent address */
    FF_SM_STATE_OPERATIONAL   = 2, /**< Fully operational on the bus */
    FF_SM_STATE_FAULT         = 3  /**< Hardware or configuration fault */
} ff_sm_state_t;

/** Get SM state name string */
const char* ff_sm_state_name(ff_sm_state_t state);

/** SM Agent structure */
typedef struct {
    ff_sm_state_t  state;
    uint8_t        device_id[32];  /**< Unique 32-byte device ID (MAC-address-like) */
    uint8_t        pd_tag[32];     /**< Physical Device Tag (PD-TAG), null-terminated */
    uint8_t        dl_address;     /**< Current DL-address */
    uint8_t        permanent_addr; /**< Permanent address assigned by LAS (0xFF = none) */
    uint32_t       current_time;   /**< System time (seconds since 1970-01-01) */
    uint32_t       time_last_td;   /**< Last Time Distribution received timestamp */
    uint32_t       uptime_sec;     /**< Device uptime counter */
    uint16_t       sm_supported;   /**< Bit mask of supported SM services */
    uint16_t       sm_sync_flags;  /**< Time synchronization flags */
} ff_sm_agent_t;

/**
 * Initialize SM Agent at power-on.
 *
 * State: SM_STATE_UNINITIALIZED
 * DL-address: assigned random temporary address (0xFC-0xFF)
 */
void ff_sm_init(ff_sm_agent_t *sm, const uint8_t device_id[32]);

/**
 * Advance SM state from UNINITIALIZED to INITIALIZING.
 * Called when device receives first DL-PDU on the bus.
 */
void ff_sm_start_initialization(ff_sm_agent_t *sm);

/**
 * Complete initialization: tag and permanent address assigned.
 * Advance to OPERATIONAL state.
 *
 * @param sm              SM agent
 * @param tag             assigned PD-TAG (max 32 chars including null)
 * @param permanent_addr  assigned permanent DL-address (0x10-0xFB)
 * @return 0 on success, -1 if invalid address
 */
int ff_sm_set_operational(ff_sm_agent_t *sm, const char *tag, uint8_t permanent_addr);

/**
 * Transition to FAULT state (hardware fault, watchdog timeout, etc.)
 */
void ff_sm_set_fault(ff_sm_agent_t *sm, uint16_t fault_code);

/**
 * Check if SM agent has a valid permanent address assignment.
 */
int ff_sm_has_permanent_address(const ff_sm_agent_t *sm);


/* ============================================================================
 * L2: Find Tag Query Service
 *
 * The Find Tag Query is used by configuration tools to locate a device
 * by its PD-TAG. The query is broadcast; the device matching the tag responds
 * with its current DL-address and device ID.
 *
 * Query flow:
 *   1. Host sends FIND_TAG_QUERY(tag) as broadcast
 *   2. Device with matching PD-TAG responds with FIND_TAG_REPLY(address, device_id)
 *   3. Host now knows the device's DL-address
 *
 * Reference: FF-880 4.3 "Find Tag Query/Reply"
 * ============================================================================ */

/**
 * Check if this device's PD-TAG matches a query tag.
 *
 * @param sm         SM agent of this device
 * @param query_tag  tag being searched for
 * @return 1 if match, 0 if no match
 */
int ff_sm_find_tag_match(const ff_sm_agent_t *sm, const char *query_tag);


/* ============================================================================
 * L3: Set Address Protocol
 *
 * The Set Address protocol is used by the LAS to assign a permanent
 * DL-address to a device that has joined with a temporary address.
 *
 * Sequence:
 *   1. LAS detects new device at temporary address (via PN/PR)
 *   2. LAS reads device ID from new device via FMS Read
 *   3. LAS assigns unused permanent address
 *   4. LAS sends SET_ADDRESS(device_id, new_address)
 *   5. Device accepts new address and responds with SET_ADDRESS_ACK
 *   6. Device reconfigures its DL-address to the new permanent value
 *
 * Reference: IEC 61158-4 9.3 "DL-address assignment"
 * ============================================================================ */

/**
 * Process a Set Address request from LAS.
 * If device_id matches and state is INITIALIZING, accept the address.
 *
 * @param sm        SM agent
 * @param device_id device ID in the request
 * @param new_addr  proposed new permanent address
 * @return 0 if accepted, -1 if rejected (ID mismatch, invalid addr, wrong state)
 */
int ff_sm_process_set_address(ff_sm_agent_t *sm, const uint8_t device_id[32],
                               uint8_t new_addr);


/* ============================================================================
 * L3: Time Distribution Protocol
 *
 * The LAS periodically broadcasts Time Distribution (TD) frames containing
 * the current system time. All devices synchronize their internal clocks.
 *
 * Time accuracy: ?1 ms on H1 (adequate for sequence-of-events)
 *
 * TD frame: LAS time + propagation delay compensation
 * Each device adds its local clock drift correction.
 *
 * Reference: FF-880 4.5 "Time Synchronization"
 * ============================================================================ */

/** Time synchronization quality */
typedef enum {
    FF_TIME_SYNC_NONE    = 0, /**< No time sync received yet */
    FF_TIME_SYNC_COARSE  = 1, /**< Coarse sync (?10 ms), receiving TD */
    FF_TIME_SYNC_FINE    = 2, /**< Fine sync (?1 ms), TD + local correction */
    FF_TIME_SYNC_LOCKED  = 3  /**< Locked: stable over multiple TD intervals */
} ff_time_sync_quality_t;

/** Time Distribution message */
typedef struct {
    uint32_t   las_time;         /**< LAS current time (seconds since epoch) */
    uint32_t   las_time_ns;      /**< Fractional nanoseconds */
    uint16_t   td_sequence;      /**< Monotonic sequence number */
    uint32_t   macrocycle_count; /**< LAS macrocycle count at TD issuance */
} ff_td_message_t;

/**
 * Process a received Time Distribution message.
 * Updates SM agent current_time with propagation delay compensation.
 *
 * @param sm       SM agent
 * @param td       received TD message
 * @param prop_delay_us  estimated one-way propagation delay (microseconds)
 * @return new time sync quality level
 */
ff_time_sync_quality_t ff_sm_process_td(ff_sm_agent_t *sm,
                                         const ff_td_message_t *td,
                                         uint32_t prop_delay_us);

/**
 * Get current time sync quality.
 */
ff_time_sync_quality_t ff_sm_time_sync_quality(const ff_sm_agent_t *sm);

/**
 * Convert SM time to human-readable format (ISO 8601-like).
 * Output buffer must be at least 20 bytes.
 */
void ff_sm_time_format(const ff_sm_agent_t *sm, char *buf, size_t buf_size);


/* ============================================================================
 * L3: Network Management (NM)
 *
 * NM manages the communication capabilities of individual VCRs.
 * Each VCR has its own state machine:
 *   NONEXISTENT  CONFIGURED  ESTABLISHING  OPERATIONAL  ABORTING
 *
 * NM also tracks the overall communication stack health:
 *   - Number of operational VCRs
 *   - DL-error counters
 *   - Retransmission statistics
 * ============================================================================ */

/** NM VCR state machine states */
typedef enum {
    FF_NM_VCR_NONEXISTENT   = 0, /**< VCR not defined */
    FF_NM_VCR_CONFIGURED    = 1, /**< VCR defined but not established */
    FF_NM_VCR_ESTABLISHING  = 2, /**< Connection establishment in progress */
    FF_NM_VCR_OPERATIONAL   = 3, /**< VCR fully operational */
    FF_NM_VCR_ABORTING      = 4  /**< Disconnection in progress */
} ff_nm_vcr_state_t;

/** Network Management statistics */
typedef struct {
    uint32_t dl_tx_frames;       /**< DL frames transmitted */
    uint32_t dl_rx_frames;       /**< DL frames received */
    uint32_t dl_tx_errors;       /**< Transmission errors */
    uint32_t dl_rx_errors;       /**< Reception errors (CRC failures, etc.) */
    uint32_t dl_timeouts;        /**< Response timeouts */
    uint32_t dl_retransmissions; /**< Retransmission count */
    uint32_t pt_received;        /**< Pass Tokens received */
    uint32_t cd_executed;        /**< Compel Data executions */
    uint32_t td_received;        /**< Time Distribution frames received */
    uint32_t vcr_established;    /**< Number of VCRs in OPERATIONAL state */
    uint32_t vcr_failed;         /**< Number of VCRs that failed/aborted */
} ff_nm_statistics_t;

/** Initialize NM statistics to zero */
void ff_nm_stats_init(ff_nm_statistics_t *stats);

/**
 * Compute the DL error rate: tx_errors + rx_errors / total frames.
 *
 * @return error rate [0.0, 1.0], or -1.0 if no frames
 */
double ff_nm_error_rate(const ff_nm_statistics_t *stats);

/**
 * Compute communication efficiency: successful frames / total frames.
 *
 * @return efficiency ratio [0.0, 1.0]
 */
double ff_nm_efficiency(const ff_nm_statistics_t *stats);


/* ============================================================================
 * L3: SMIB ? System Management Information Base
 *
 * The SMIB is a structured database of SM/NM operational parameters.
 * It is accessed by configuration and diagnostic tools via FMS.
 *
 * Key SMIB entries:
 *   - Device ID, Device Tag
 *   - Operational state
 *   - DL-address (permanent and temporary)
 *   - Time synchronization parameters
 *   - Device capabilities (SM, FBAP supported features)
 *
 * Reference: FF-880 Annex A "SMIB Structure"
 * ============================================================================ */

/** SMIB entry types */
typedef enum {
    FF_SMIB_DEVICE_ID        = 1,
    FF_SMIB_DEVICE_TAG       = 2,
    FF_SMIB_DEVICE_ADDRESS   = 3,
    FF_SMIB_DEVICE_STATE     = 4,
    FF_SMIB_OPERATIONAL_POWERUP = 5,
    FF_SMIB_DEVICE_CAPABILITY  = 6,
    FF_SMIB_TIME_SYNC          = 7,
    FF_SMIB_VFD_REF_LIST       = 8,  /**< Virtual Field Device reference list */
    FF_SMIB_DLME_BASIC_INFO    = 9   /**< DLME = Data Link Management Entity */
} ff_smib_entry_t;

/** SMIB read function: get a SMIB value by entry type */
int ff_smib_read(const ff_sm_agent_t *sm, ff_smib_entry_t entry,
                 uint8_t *buf, size_t *buf_size);


#ifdef __cplusplus
}
#endif

#endif /* FF_H1_SYSTEM_MGMT_H */