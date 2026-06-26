#ifndef DCS_REDUNDANCY_H
#define DCS_REDUNDANCY_H

#include "experion_system.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Redundancy role enumeration. */
typedef enum {
    RED_ROLE_UNKNOWN   = 0,
    RED_ROLE_PRIMARY   = 1,
    RED_ROLE_BACKUP    = 2,
    RED_ROLE_SOLO      = 3,
    RED_ROLE_SYNCING   = 4,
    RED_ROLE_OFFLINE   = 5
} RedundancyRole;

/** Redundancy pair health. */
typedef enum {
    RED_PAIR_HEALTHY      = 0,
    RED_PAIR_DEGRADED     = 1,
    RED_PAIR_SYNCING      = 2,
    RED_PAIR_FAILOVER     = 3,
    RED_PAIR_PRIMARY_ONLY = 4,
    RED_PAIR_BACKUP_ONLY  = 5,
    RED_PAIR_FAILED       = 6
} RedundancyPairHealth;

/** Redundancy module types. */
typedef enum {
    RED_MOD_ESVT       = 0x01,
    RED_MOD_C300       = 0x02,
    RED_MOD_SAFETY_MGR = 0x04,
    RED_MOD_FTE_SWITCH = 0x08,
    RED_MOD_POWER      = 0x10,
    RED_MOD_IOLINK     = 0x20
} RedundancyModuleType;

#define RED_MAX_MISSED_HEARTBEATS 5
#define RED_HEARTBEAT_INTERVAL_MS 100

/** Heartbeat message exchanged between primary and backup. */
typedef struct {
    uint32_t        sequence_number;
    uint64_t        timestamp_ns;
    RedundancyRole  sender_role;
    uint32_t        partner_id;
    uint32_t        data_sequence;
    bool            request_sync;
    bool            partner_alive;
    uint32_t        uptime_ms;
    uint8_t         health_status;
} RedundancyHeartbeat;

/** Data synchronization categories. */
typedef enum {
    SYNC_NONE           = 0x00,
    SYNC_CONFIG         = 0x01,
    SYNC_CONTROL_BLOCKS = 0x02,
    SYNC_IO_IMAGE       = 0x04,
    SYNC_ALARM_STATE    = 0x08,
    SYNC_TREND_BUFFER   = 0x10,
    SYNC_SEQUENCE_STATE = 0x20,
    SYNC_OPERATOR_LOCKS = 0x40,
    SYNC_ALL            = 0x7F
} DataSyncCategory;

/** Synchronization progress tracker. */
typedef struct {
    DataSyncCategory categories_pending;
    DataSyncCategory categories_complete;
    uint32_t        bytes_total;
    uint32_t        bytes_transferred;
    uint32_t        start_time_ms;
    uint32_t        elapsed_ms;
    bool            in_progress;
    bool            success;
} SyncProgress;

/** Failover trigger types. */
typedef enum {
    FAILOVER_NONE             = 0,
    FAILOVER_PRIMARY_FAILURE  = 1,
    FAILOVER_HEARTBEAT_LOST   = 2,
    FAILOVER_MANUAL           = 3,
    FAILOVER_SCHEDULED        = 4,
    FAILOVER_POWER_LOSS       = 5,
    FAILOVER_NETWORK_ISOLATE  = 6,
    FAILOVER_WATCHDOG_TIMEOUT = 7
} FailoverTrigger;

/** Failover event record. */
typedef struct {
    uint32_t        event_id;
    time_t          timestamp;
    FailoverTrigger trigger;
    RedundancyRole  old_primary_role;
    RedundancyRole  new_primary_role;
    uint32_t        switchover_time_ms;
    uint32_t        data_loss_bytes;
    bool            bumpless;
    char            description[128];
} FailoverEvent;

#define RED_MAX_FAILOVER_HISTORY 256

/** Redundancy manager for a single redundant pair. */
typedef struct {
    uint32_t            pair_id;
    RedundancyModuleType module_type;
    uint32_t            primary_id;
    uint32_t            backup_id;
    RedundancyRole      current_role;
    RedundancyPairHealth pair_health;
    RedundancyHeartbeat last_heartbeat_tx;
    RedundancyHeartbeat last_heartbeat_rx;
    uint32_t            missed_heartbeats;
    uint32_t            heartbeat_interval_ms;
    SyncProgress        sync_progress;
    bool                sync_on_startup;
    bool                auto_failback;
    uint32_t            failover_count;
    FailoverEvent       failover_history[256];
    uint32_t            last_failover_index;
    time_t              last_role_change;
    uint64_t            total_uptime_ms;
    bool                diagnostic_mode;
} RedundancyManager;

/** FTE network paths. */
typedef enum {
    FTE_PATH_A = 0,
    FTE_PATH_B = 1,
    FTE_PATH_C = 2,
    FTE_PATH_D = 3,
    FTE_PATH_COUNT = 4
} FTEPath;

/** FTE path status. */
typedef struct {
    FTEPath         path;
    bool            link_up;
    bool            logical_up;
    uint32_t        tx_packets;
    uint32_t        rx_packets;
    uint32_t        tx_errors;
    uint32_t        rx_errors;
    uint32_t        tx_dropped;
    uint32_t        link_speed_mbps;
    double          utilization_pct;
    uint32_t        avg_latency_us;
} FTEPathStatus;

/** FTE network aggregate status. */
typedef struct {
    FTEPathStatus   paths[4];
    bool            redundancy_healthy;
    int             active_paths;
    uint32_t        total_bandwidth_mbps;
    double          aggregate_utilization;
    bool            failover_capable;
} FTENetworkStatus;

/** Bumpless transfer state. */
typedef struct {
    double      transfer_time_sec;
    double      current_ramp;
    double      tracked_op;
    double      computed_op;
    double      transition_op;
    bool        in_transition;
    uint32_t    transition_start_ms;
} BumplessTransfer;

/** SIL level enumeration. */
typedef enum {
    SIL_NONE = 0,
    SIL_1    = 1,
    SIL_2    = 2,
    SIL_3    = 3,
    SIL_4    = 4
} SafetyIntegrityLevel;

/** SIL compliance tracking. */
typedef struct {
    SafetyIntegrityLevel target_sil;
    int                  required_hft;
    double               pfdavg_target;
    double               pfdavg_achieved;
    double               proof_test_interval_hours;
    double               mitr_hours;
    double               mttf_hours;
    double               common_cause_beta;
    bool                 compliant;
} SILComplianceStatus;

/* API */
int  redundancy_init(RedundancyManager *rm, uint32_t pair_id, RedundancyModuleType type,
                     uint32_t primary_id, uint32_t backup_id);
int  redundancy_set_role(RedundancyManager *rm, RedundancyRole new_role);
int  redundancy_send_heartbeat(RedundancyManager *rm, RedundancyHeartbeat *hb);
int  redundancy_receive_heartbeat(RedundancyManager *rm, const RedundancyHeartbeat *hb);
int  redundancy_check_health(RedundancyManager *rm, RedundancyPairHealth *health);
int  redundancy_trigger_failover(RedundancyManager *rm, FailoverTrigger trigger);
int  redundancy_start_sync(RedundancyManager *rm, DataSyncCategory categories);
int  redundancy_sync_progress(const RedundancyManager *rm, SyncProgress *progress);
int  redundancy_sync_complete(RedundancyManager *rm);
int  bumpless_transfer_init(BumplessTransfer *bt, double transfer_time_sec);
int  bumpless_transfer_start(BumplessTransfer *bt, double tracked_op, double computed_op);
double bumpless_transfer_update(BumplessTransfer *bt, double computed_op, double dt_sec);
int  fte_status_update(FTENetworkStatus *fte, FTEPath path, bool link_up,
                       uint32_t tx_pkts, uint32_t rx_pkts);
int  fte_check_redundancy(const FTENetworkStatus *fte, bool *redundant);
int  fte_best_path(const FTENetworkStatus *fte, FTEPath *best);
int  sil_compliance_check(const SILComplianceStatus *sil, bool *compliant);
int  sil_calculate_pfdavg(SILComplianceStatus *sil, double mttf, double mitr,
                          double proof_interval, double beta);
int  redundancy_log_failover(RedundancyManager *rm, FailoverTrigger trigger,
                             uint32_t switchover_ms, bool bumpless);
int  redundancy_get_last_failover(const RedundancyManager *rm, FailoverEvent *event);

#ifdef __cplusplus
}
#endif

#endif