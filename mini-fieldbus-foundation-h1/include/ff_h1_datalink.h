/**
 * ff_h1_datalink.h ? Foundation Fieldbus H1 Data Link Layer (IEC 61158-4)
 *
 * Implements the H1 Data Link Layer: Link Active Scheduler (LAS) operations,
 * token passing, scheduled/unscheduled communication, device addressing,
 * Live List management, and Link Master redundancy.
 *
 * Course Mapping:
 *   MIT 6.302  ? Deterministic scheduling in feedback systems
 *   Berkeley ME233 ? Real-time network scheduling
 *   ISA/IEC  ? IEC 61158-4: Data Link Layer Protocol Specification
 *   RWTH Aachen ? Echtzeitkommunikation in der Automatisierungstechnik
 *
 * Knowledge Levels: L1 (Definitions), L2 (Core Concepts), L3 (Structures), L5 (Algorithms)
 */

#ifndef FF_H1_DATALINK_H
#define FF_H1_DATALINK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: DL-Address Definitions
 *
 * H1 addressing (IEC 61158-4):
 *   0x00        ? Reserved (null address)
 *   0x01?0x0F   ? Reserved for future use
 *   0x10?0xFB   ? Permanent device addresses (236 available)
 *   0xFC?0xFF   ? Temporary/visitor/default addresses (4 available)
 *
 * Default address 0xFC?0xFF is assigned when a device first joins the segment;
 * the LAS then assigns a permanent address in range 0x10?0xFB.
 * ============================================================================ */

#define FF_DL_ADDR_NULL          0x00
#define FF_DL_ADDR_RESERVED_MIN  0x01
#define FF_DL_ADDR_RESERVED_MAX  0x0F
#define FF_DL_ADDR_PERM_MIN      0x10
#define FF_DL_ADDR_PERM_MAX      0xFB
#define FF_DL_ADDR_TEMP_MIN      0xFC
#define FF_DL_ADDR_TEMP_MAX      0xFF
#define FF_DL_ADDR_VISITOR_FIRST 0xFC
#define FF_DL_ADDR_VISITOR_LAST  0xFF

/** Maximum number of permanent addresses */
#define FF_DL_MAX_PERMANENT_DEVICES (FF_DL_ADDR_PERM_MAX - FF_DL_ADDR_PERM_MIN + 1)

/** Check if an address is in the permanent range */
#define FF_DL_IS_PERMANENT_ADDR(a) ((a) >= FF_DL_ADDR_PERM_MIN && (a) <= FF_DL_ADDR_PERM_MAX)


/* ============================================================================
 * L1: DL-PDU Frame Control Field Definitions
 *
 * Frame Control (FC) octet: the first octet of every DL-PDU.
 *   Bits 7-5: Frame Type
 *     b000 = Token / idle
 *     b001 = CD (Compel Data) ? scheduled request
 *     b010 = Data/Status Transfer (DT)
 *     b011 = Time Distribution (TD)
 *     b100 = Probe Node (PN) / Probe Response (PR)
 *     b101 = Claim LAS (CL) / Transfer LAS (TL)
 *   Bits 4-0: Sub-type / flags (type-dependent)
 * ============================================================================ */

typedef enum {
    FF_FRAME_TYPE_TOKEN_IDLE   = 0x00, /**< Token pass or idle indication */
    FF_FRAME_TYPE_CD           = 0x20, /**< Compel Data: scheduled publish request */
    FF_FRAME_TYPE_DT           = 0x40, /**< Data/Status Transfer */
    FF_FRAME_TYPE_TD           = 0x60, /**< Time Distribution */
    FF_FRAME_TYPE_PN_PR        = 0x80, /**< Probe Node / Probe Response */
    FF_FRAME_TYPE_CL_TL        = 0xA0  /**< Claim LAS / Transfer LAS */
} ff_frame_type_t;

/** Extract frame type from FC octet */
#define FF_FC_GET_TYPE(fc)  ((fc) & 0xE0)

/** DL-PDU structure */
typedef struct {
    uint8_t  fc;               /**< Frame Control octet */
    uint8_t  dl_dest;          /**< Destination DL-address */
    uint8_t  dl_src;           /**< Source DL-address */
    uint8_t  user_data[255];   /**< User data / FMS PDU */
    size_t   user_data_len;    /**< User data length */
    uint16_t fcs;              /**< Frame Check Sequence */
} ff_dl_pdu_t;


/* ============================================================================
 * L1: Link Active Scheduler (LAS) ? Core Definitions
 *
 * The LAS is the bus arbiter. Exactly one device on each H1 segment acts as the
 * LAS at any given time. The LAS:
 *   1. Maintains the Live List of operational devices
 *   2. Executes the CD Schedule (deterministic, time-triggered)
 *   3. Issues Pass Token (PT) for unscheduled communication
 *   4. Probes for new devices (PN/PR)
 *   5. Distributes time (TD)
 *   6. Manages LAS transfer and redundancy
 *
 * IEC 61158-4 8.1: LAS functions
 * ============================================================================ */

/** LAS operational states */
typedef enum {
    FF_LAS_STATE_INACTIVE = 0,  /**< Not the active LAS */
    FF_LAS_STATE_STARTUP  = 1,  /**< Assuming LAS role, building Live List */
    FF_LAS_STATE_ACTIVE   = 2,  /**< Fully operational LAS */
    FF_LAS_STATE_SHUTDOWN = 3   /**< Gracefully transferring LAS role */
} ff_las_state_t;


/* ============================================================================
 * L1: CD Schedule Entry
 *
 * A Compel Data schedule entry defines:
 *   - Which device (address) must publish a specific buffer
 *   - At what macrocycle offset this must happen
 *   - The maximum time allowed for the response
 *
 * The CD schedule is periodic with period = macrocycle.
 * ============================================================================ */

typedef struct {
    uint8_t  publisher_addr;   /**< DL-address of publishing device */
    uint8_t  buffer_index;     /**< Which buffer in the publisher to read */
    uint32_t offset_us;        /**< Offset from macrocycle start in microseconds */
    uint32_t max_response_us;  /**< Maximum response time window */
} ff_cd_entry_t;

/** CD Schedule: ordered array of CD entries executed each macrocycle */
typedef struct {
    ff_cd_entry_t *entries;
    size_t         count;
    size_t         capacity;
    uint32_t       macrocycle_us; /**< Macrocycle duration in microseconds */
} ff_cd_schedule_t;


/* ============================================================================
 * L2: Live List Management
 *
 * The Live List records every operational device on the segment.
 * Devices are discovered via Probe Node (PN) / Probe Response (PR) exchanges.
 * ============================================================================ */

/** Device entry in the Live List */
typedef struct {
    uint8_t  dl_address;   /**< Device DL-address */
    uint8_t  device_class; /**< 1=Basic, 2=Link Master, 3=Bridge */
    uint8_t  is_operational; /**< 1 if confirmed operational */
    uint32_t last_seen_us; /**< Timestamp of last communication */
} ff_live_list_entry_t;

/** Live List */
typedef struct {
    ff_live_list_entry_t *entries;
    size_t                count;
    size_t                capacity;
} ff_live_list_t;


/* ============================================================================
 * L2: Token Passing
 *
 * Unscheduled communication is managed via token passing. The LAS passes a
 * Pass Token (PT) to each device in the Live List during idle time. The
 * device holding the token may send one unscheduled message before returning
 * the token.
 *
 * Delegated Token: a Link Master device can be delegated a subset of addresses
 * to which it may pass tokens. This reduces LAS burden for large segments.
 * ============================================================================ */

/** Delegated token range */
typedef struct {
    uint8_t  delegator_addr;  /**< LAS or Link Master that delegates */
    uint8_t  delegatee_addr;  /**< LM receiving delegation */
    uint8_t  range_start;     /**< First address in delegated range */
    uint8_t  range_end;       /**< Last address in delegated range */
    uint32_t token_hold_us;   /**< Maximum token hold time (microseconds) */
} ff_delegated_token_t;


/* ============================================================================
 * L5: LAS Scheduling Algorithm
 *
 * The LAS must interleave scheduled (CD) and unscheduled (PT) traffic within
 * each macrocycle. The core algorithm:
 *
 *   macrocycle_start:
 *     for each cd_entry in schedule (ordered by offset_us):
 *       wait until offset_us from start
 *       issue CD(cd_entry.publisher_addr, cd_entry.buffer_index)
 *       wait for response or timeout (cd_entry.max_response_us)
 *     remaining_time = macrocycle_us - last_cd_end
 *     while remaining_time > token_pass_overhead:
 *       next_device = token_round_robin_next()
 *       issue PT(next_device)
 *       remaining_time -= (pt_time + device_response_time)
 * ============================================================================ */

/**
 * LAS scheduler context ? maintains all state needed for LAS operation
 * across macrocycles.
 */
typedef struct {
    ff_las_state_t    state;
    ff_live_list_t    live_list;
    ff_cd_schedule_t  cd_schedule;
    uint32_t          macrocycle_start_us;
    uint32_t          current_time_us;
    size_t            next_cd_index;
    size_t            next_pt_index;
    uint8_t           las_address;
    uint32_t          token_pass_overhead_us;
    uint32_t          idle_time_accumulated_us;
    uint32_t          cd_overruns;          /**< Count of CD schedule overruns */
    uint32_t          macrocycle_count;     /**< Total macrocycles executed */
} ff_las_context_t;

/**
 * Initialize LAS context with empty live list and schedule.
 *
 * @param ctx           LAS context to initialize
 * @param las_address   DL-address of this LAS
 */
void ff_las_init(ff_las_context_t *ctx, uint8_t las_address);

/**
 * Add a CD schedule entry. Entries must be added in ascending offset_us order.
 *
 * @return 0 on success, -1 if schedule full or offset not monotonic
 */
int ff_las_cd_add(ff_las_context_t *ctx, const ff_cd_entry_t *entry);

/**
 * Run one macrocycle of the LAS schedule.
 * Executes all CD entries in order and fills remaining time with token passes.
 *
 * @param ctx LAS context with populated schedule
 * @return number of CD entries successfully executed this cycle
 *
 * Complexity: O(n) in number of CD + PT operations per macrocycle
 * Reference: IEC 61158-4 8.3.2 "Scheduled and unscheduled traffic"
 */
int ff_las_run_macrocycle(ff_las_context_t *ctx);

/**
 * Compute schedule utilization: fraction of macrocycle consumed by CD traffic.
 *
 * @return utilization ratio [0.0, 1.0], or -1.0 on error
 */
double ff_las_cd_utilization(const ff_las_context_t *ctx);

/**
 * Determine if there is enough idle time for unscheduled communication
 * after all CD traffic in the current macrocycle.
 *
 * @param min_idle_us  minimum idle time required (microseconds)
 * @return 1 if sufficient idle time exists, 0 otherwise
 */
int ff_las_has_idle_time(const ff_las_context_t *ctx, uint32_t min_idle_us);


/* ============================================================================
 * L5: Link Master Election Algorithm
 *
 * When the LAS fails, Link Master devices use a priority-based election
 * (Claim LAS protocol). Each LM sends a Claim LAS frame with its priority.
 * The LM with the highest priority (lowest numerical priority value) wins.
 *
 * Election timeouts and back-off prevent collisions:
 *   1. LM detects LAS failure (no TD for 3x T_timeout)
 *   2. LM waits T_holdoff (proportional to its priority)
 *   3. LM sends Claim LAS
 *   4. If higher-priority claim heard, withdraw
 *   5. If no higher claim, this LM becomes LAS
 * ============================================================================ */

/** Link Master priority: lower value = higher priority */
#define FF_LM_PRIORITY_HIGHEST  0x01
#define FF_LM_PRIORITY_LOWEST   0xFF
#define FF_LM_PRIORITY_DEFAULT  0x80

/** Timeouts for LAS election in milliseconds */
#define FF_LAS_TIMEOUT_MS        100   /**< No TD for this long = LAS failure suspected */
#define FF_LAS_HOLDOFF_BASE_MS   4     /**< Base hold-off per priority level */
#define FF_LAS_CLAIM_TIMEOUT_MS  250   /**< Time to wait for competing claims */

/**
 * Compute the hold-off time for a given Link Master priority.
 *
 * holdoff_ms = (priority - 1) * FF_LAS_HOLDOFF_BASE_MS
 *
 * Lower priority (higher value) devices wait longer, giving higher-priority
 * devices the chance to claim first.
 *
 * @param priority  LM priority (0x01 = highest, 0xFF = lowest)
 * @return hold-off time in milliseconds
 */
uint32_t ff_lm_holdoff_ms(uint8_t priority);

/**
 * Determine the winner of a LAS election between two priorities.
 *
 * @param priority_a  first candidate priority
 * @param priority_b  second candidate priority
 * @return 1 if A wins (lower value), -1 if B wins, 0 if equal
 */
int ff_lm_election_compare(uint8_t priority_a, uint8_t priority_b);


/* ============================================================================
 * L2: VCR ? Virtual Communication Relationship Types
 *
 * Foundation Fieldbus defines three VCR types for data exchange:
 *
 *   BNU (Publisher/Subscriber):  Buffered, Network-scheduled, Unidirectional.
 *       Used for cyclic I/O data (AI, AO). One publisher, multiple subscribers.
 *
 *   QUB (Client/Server): Queued, User-triggered, Bidirectional.
 *       Used for acyclic parameter access, configuration, diagnostics.
 *
 *   QUB (Report Distribution): Queued, User-triggered, Bidirectional.
 *       Used for alarm/event notification from device to host.
 *
 * Reference: IEC 61158-4 Annex B
 * ============================================================================ */

typedef enum {
    FF_VCR_TYPE_BNU = 0,  /**< Publisher/Subscriber (buffered, scheduled, unidirectional) */
    FF_VCR_TYPE_QUB = 1,  /**< Client/Server (queued, unscheduled, bidirectional) */
    FF_VCR_TYPE_QUU = 2   /**< Report Distribution (queued, unscheduled, unidirectional) */
} ff_vcr_type_t;

/** VCR descriptor */
typedef struct {
    ff_vcr_type_t type;
    uint8_t       local_addr;      /**< Local DL-address */
    uint8_t       remote_addr;     /**< Remote DL-address */
    uint8_t       vcr_id;          /**< Locally unique VCR identifier */
    uint16_t      max_data_size;   /**< Maximum PDU size for this VCR */
    uint8_t       is_active;       /**< 1 if VCR is operational */
} ff_vcr_descriptor_t;


/* ============================================================================
 * L5: Live List Operations
 * ============================================================================ */

void ff_live_list_init(ff_live_list_t *list);
int ff_live_list_add(ff_live_list_t *list, uint8_t address, uint8_t device_class);
int ff_live_list_remove(ff_live_list_t *list, uint8_t address);
int ff_live_list_find(const ff_live_list_t *list, uint8_t address);
int ff_live_list_count_operational(const ff_live_list_t *list);
void ff_live_list_mark_seen(ff_live_list_t *list, uint8_t address, uint32_t timestamp);
int ff_live_list_is_full(const ff_live_list_t *list);

/**
 * Find the next device in round-robin order for token passing.
 * Wraps around the operational devices list.
 *
 * @return address of next device, or 0 if list empty
 */
uint8_t ff_live_list_next_token(const ff_live_list_t *list, size_t *current_index);


#ifdef __cplusplus
}
#endif

#endif /* FF_H1_DATALINK_H */