/**
 * @file    ecs700_redundancy.h
 * @brief   SUPCON ECS-700 Redundancy and Failover Management
 *
 * ECS-700 implements multiple layers of redundancy:
 *   - Control Station: 1:1 hot standby (primary/secondary pair)
 *   - SCnet: Dual redundant Ethernet ring (Port A / Port B)
 *   - SBUS: Redundant fieldbus (Port A / Port B)
 *   - Power Supply: Dual redundant PSU (N+1)
 *   - I/O Modules: Optional 1:1 redundancy for critical loops
 *
 * Key Performance Metrics:
 *   - Failover time: < 20 ms (target per IEC 62439-3 HSR)
 *   - Data sync interval: 1 scan period (50-200 ms typical)
 *   - Bumpless transfer during failover: guaranteed
 *   - Hot swap support: I/O modules and power supplies
 *
 * Redundancy Protocol:
 *   - Heartbeat: Periodic (50 ms) UDP messages between primary/secondary
 *   - Missing heartbeats threshold: 3 (150 ms fault detection)
 *   - Data synchronization: Complete CS state (PID, SFC, interlock)
 *   - Arbitration: Node ID priority + health score tiebreaker
 *
 * Knowledge Coverage:
 *   L1: Redundancy definitions, failover types
 *   L2: Hot standby concept, heartbeat mechanism
 *   L3: Redundancy state machine, sync protocol
 *   L4: IEC 62439-3 HSR/PRP, IEC 61508 high-demand redundancy
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#ifndef ECS700_REDUNDANCY_H
#define ECS700_REDUNDANCY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecs700_system_core.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * L1: Core Definitions — Redundancy Parameters
 * ============================================================================
 */

/** Maximum heartbeat miss count before declaring partner failure */
#define ECS700_HEARTBEAT_MISS_MAX         3

/** Heartbeat message interval in microseconds */
#define ECS700_HEARTBEAT_INTERVAL_US      50000UL  /* 50 ms */

/** Data synchronization interval in microseconds */
#define ECS700_SYNC_INTERVAL_US           100000UL /* 100 ms */

/** Maximum failover retry attempts */
#define ECS700_FAILOVER_RETRY_MAX         3

/** Failover grace period (prevent oscillation) in microseconds */
#define ECS700_FAILOVER_GRACE_US          5000000UL /* 5 s */

/** Redundancy network path health thresholds */
#define ECS700_PATH_HEALTH_GOOD           90.0    /**< % packets delivered */
#define ECS700_PATH_HEALTH_DEGRADED       70.0    /**< % Warning level */
#define ECS700_PATH_HEALTH_FAILED         30.0    /**< % Declare dead */

/** Maximum redundancy pair ID */
#define ECS700_MAX_REDUNDANCY_PAIRS       (ECS700_MAX_CONTROL_STATIONS / 2)

/* ============================================================================
 * L1: Core Data Structures — Redundancy Control
 * ============================================================================
 */

/**
 * @brief Controller health score components
 *
 * Comprehensive health assessment used for redundancy arbitration.
 * The healthier controller becomes primary after failover.
 */
typedef struct {
    double    cpu_load;                   /**< CPU utilization percent */
    double    memory_available_mb;        /**< Available RAM in MB */
    uint32_t  scnet_a_errors;             /**< Port A cumulative errors */
    uint32_t  scnet_b_errors;             /**< Port B cumulative errors */
    uint32_t  sbus_a_errors;              /**< SBUS A cumulative errors */
    uint32_t  sbus_b_errors;              /**< SBUS B cumulative errors */
    uint32_t  watchdog_timeouts;          /**< Watchdog timeout count */
    uint32_t  memory_ecc_errors;          /**< ECC memory error count */
    double    temperature_c;              /**< CPU temperature in Celsius */
    double    power_supply_v;             /**< Power supply voltage */
    uint32_t  uptime_ms;                  /**< Controller uptime */
    uint32_t  io_module_faults;           /**< Failed I/O module count */
    double    system_time_drift_us;        /**< Clock drift from time server */
} ecs700_health_score_t;

/**
 * @brief Redundancy pair state — manages two controllers in 1:1 hot standby
 *
 * The redundancy controller monitors partner health, synchronizes
 * control state, and executes failover when the primary fails.
 *
 * Knowledge Point: 1:1 Hot Standby — the core DCS redundancy architecture.
 * The secondary controller runs the same control logic in shadow mode,
 * receiving synchronized data each scan. On failover, the secondary
 * transitions to primary in < 20 ms with fully initialized control state.
 */
typedef struct {
    uint16_t  pair_id;                    /**< Redundancy pair identifier */
    uint16_t  primary_node_id;            /**< Current primary node ID */
    uint16_t  secondary_node_id;          /**< Current secondary node ID */
    ecs700_redundancy_mode_t mode;        /**< Redundancy mode */
    bool      sync_in_progress;           /**< Full state sync active */
    uint64_t  last_sync_time;             /**< Last sync timestamp μs */
    uint32_t  sync_sequence;              /**< Incremental sync counter */
    uint64_t  last_heartbeat_primary;     /**< Last HB received from primary */
    uint64_t  last_heartbeat_secondary;   /**< Last HB received from secondary */
    uint8_t   heartbeat_miss_count;       /**< Consecutive missed heartbeats */
    uint8_t   failover_count;             /**< Cumulative failover events */
    uint64_t  last_failover_time;         /**< Last failover timestamp */
    bool      failover_in_progress;       /**< Failover currently executing */
    bool      partner_healthy;            /**< Is the partner controller healthy */
    ecs700_health_score_t local_health;   /**< Local controller health */
    ecs700_health_score_t partner_health; /**< Partner controller health */
    uint64_t  grace_period_end;          /**< Failover grace period end time */
    uint32_t  bytes_synced;              /**< Total data bytes synchronized */
    uint32_t  sync_errors;               /**< Cumulative sync failures */
} ecs700_redundancy_pair_t;

/**
 * @brief Network path health statistics
 *
 * Monitors individual communication paths for redundancy management.
 * Both SCnet and SBUS paths are monitored independently.
 */
typedef struct {
    uint16_t  path_id;                    /**< Path identifier */
    char      path_name[32];              /**< Path description */
    uint32_t  packets_sent;               /**< Total packets transmitted */
    uint32_t  packets_received;           /**< Total packets received */
    uint32_t  packets_lost;               /**< Total packets lost */
    uint32_t  packet_errors;              /**< CRC/checksum errors */
    double    packet_loss_rate;           /**< % packet loss */
    double    average_latency_us;         /**< Average round-trip latency */
    double    max_latency_us;             /**< Maximum observed latency */
    double    jitter_us;                  /**< Latency jitter (std deviation) */
    bool      link_up;                    /**< Physical link status */
    bool      degraded;                   /**< Degraded but operational */
    uint64_t  last_packet_time;           /**< Timestamp of last received packet */
} ecs700_path_health_t;

/* ============================================================================
 * L2: Core Concepts — Redundancy Operations
 * ============================================================================
 */

/**
 * @brief Initialize a redundancy pair
 *
 * Configures the 1:1 hot standby relationship between two controllers.
 * Sets heartbeat parameters, initializes health monitoring, and starts
 * the partner discovery protocol.
 *
 * Knowledge Point: Redundancy Pair Initialization — the bootstrap
 * process that establishes master-slave relationship in DCS.
 * Typically the lower node ID becomes primary initially.
 *
 * @param pair             Redundancy pair to initialize
 * @param pair_id          Unique pair ID
 * @param primary_node_id  Primary controller node ID
 * @param secondary_node_id Secondary controller node ID
 */
void ecs700_redundancy_pair_init(ecs700_redundancy_pair_t *pair,
                                  uint16_t pair_id,
                                  uint16_t primary_node_id,
                                  uint16_t secondary_node_id);

/**
 * @brief Process heartbeat message from partner controller
 *
 * Updates partner health data and resets heartbeat miss counter.
 * Heartbeat messages carry: controller health summary, sync sequence
 * number, and current CS state.
 *
 * Knowledge Point: DCS Heartbeat Protocol — the watchdog mechanism
 * that detects partner failure. Industry standard: 3 missed heartbeats
 * at 50 ms intervals = 150 ms fault detection time.
 *
 * @param pair            Redundancy pair
 * @param partner_health  Partner health data from heartbeat
 * @param current_time_us System time in microseconds
 */
void ecs700_redundancy_heartbeat(ecs700_redundancy_pair_t *pair,
                                  const ecs700_health_score_t *partner_health,
                                  uint64_t current_time_us);

/**
 * @brief Execute failover from primary to secondary controller
 *
 * Failover sequence:
 *   1. Detect partner failure (heartbeat timeout)
 *   2. Verify local controller health is adequate
 *   3. If not in grace period, initiate failover
 *   4. Switch output ownership to local controller
 *   5. Resume control from last synchronized state
 *   6. Begin partner recovery monitoring
 *
 * Knowledge Point: DCS Failover — the switchover process that maintains
 * process control during controller failure. Target: < 20 ms from fault
 * detection to full control resumption. Outputs are held at last value
 * during the transition (typically 1-2 scan periods).
 *
 * @param pair            Redundancy pair
 * @param current_time_us System time
 * @return 0 if failover successful or not needed, non-zero on failure
 */
int ecs700_redundancy_failover(ecs700_redundancy_pair_t *pair,
                                uint64_t current_time_us);

/**
 * @brief Synchronize control state from primary to secondary
 *
 * Transfers the complete control state including all PID blocks,
 * SFC states, interlock status, alarm states, and I/O output values.
 * Uses incremental sync (changed data only) to minimize network load.
 *
 * Knowledge Point: DCS Data Synchronization — the core mechanism that
 * enables bumpless failover. Complete state sync ensures the secondary
 * controller can take over with zero control disturbance.
 *
 * @param pair            Redundancy pair
 * @param state_data      Serialized control state
 * @param state_size      Size of state data in bytes
 * @param current_time_us System time
 * @return 0 on success, non-zero on sync failure
 */
int ecs700_redundancy_sync_state(ecs700_redundancy_pair_t *pair,
                                  const uint8_t *state_data,
                                  uint32_t state_size,
                                  uint64_t current_time_us);

/**
 * @brief Compute controller health score (0.0 = fatal, 100.0 = perfect)
 *
 * Weighted health assessment combining:
 *   - CPU load (25%)
 *   - Memory availability (20%)
 *   - Network path health (20%)
 *   - I/O subsystem health (15%)
 *   - Power supply health (10%)
 *   - Temperature (10%)
 *
 * Knowledge Point: Health Scoring — quantitative controller health
 * used for redundancy arbitration. Prevents failover to a
 * partially-failed controller.
 *
 * @param health  Controller health metrics
 * @return Health score 0.0–100.0
 */
double ecs700_compute_health_score(const ecs700_health_score_t *health);

/**
 * @brief Initialize network path health monitor
 *
 * @param path      Path health structure
 * @param path_id   Path identifier
 * @param name      Path description
 */
void ecs700_path_health_init(ecs700_path_health_t *path,
                              uint16_t path_id, const char *name);

/**
 * @brief Update path health statistics with packet result
 *
 * @param path         Path health structure
 * @param sent         Packets sent this interval
 * @param received     Packets received this interval
 * @param errors       Packet errors this interval
 * @param latency_us   Average latency in microseconds
 */
void ecs700_path_health_update(ecs700_path_health_t *path,
                                uint32_t sent, uint32_t received,
                                uint32_t errors, double latency_us);

/* ============================================================================
 * L3: Engineering Structures — Redundancy State Machine
 * ============================================================================
 */

/**
 * @brief Redundancy event types for logging and diagnostics
 */
typedef enum {
    ECS700_REDUN_EVENT_HEARTBEAT_OK     = 0,
    ECS700_REDUN_EVENT_HEARTBEAT_MISS   = 1,
    ECS700_REDUN_EVENT_FAILOVER_START   = 2,
    ECS700_REDUN_EVENT_FAILOVER_COMPLETE = 3,
    ECS700_REDUN_EVENT_FAILOVER_FAILED  = 4,
    ECS700_REDUN_EVENT_SYNC_START       = 5,
    ECS700_REDUN_EVENT_SYNC_COMPLETE    = 6,
    ECS700_REDUN_EVENT_SYNC_FAILED      = 7,
    ECS700_REDUN_EVENT_PARTNER_RECOVERED = 8,
    ECS700_REDUN_EVENT_GRACE_PERIOD_START = 9,
    ECS700_REDUN_EVENT_GRACE_PERIOD_END = 10
} ecs700_redundancy_event_type_t;

/**
 * @brief Redundancy event log entry
 */
typedef struct {
    uint64_t timestamp;                    /**< Event timestamp */
    ecs700_redundancy_event_type_t type;   /**< Event type */
    uint16_t pair_id;                      /**< Affected pair */
    uint16_t source_node_id;               /**< Node that generated event */
    char     reason[128];                  /**< Human-readable event reason */
} ecs700_redundancy_event_t;

/**
 * @brief Log a redundancy event
 *
 * @param pair      Redundancy pair
 * @param type      Event type
 * @param reason    Event description
 */
void ecs700_redundancy_log_event(ecs700_redundancy_pair_t *pair,
                                  ecs700_redundancy_event_type_t type,
                                  const char *reason);

/* ============================================================================
 * L4: Engineering Laws — Reliability Calculations
 * ============================================================================
 */

/**
 * @brief Compute system availability with 1:1 redundancy
 *
 * For a 1:1 hot standby system with independent failure:
 *   A_system = 1 - (1 - A_single)^2
 *
 * This assumes: independent failure modes, perfect failover detection,
 * no common-cause failures.
 *
 * Knowledge Point: Reliability/Availability — the mathematics of
 * redundant systems. DCS availability target: 99.999% (5-nines).
 * With MTBF(CS) = 100,000 hours and MTTR = 4 hours:
 *   A_single = 100000 / (100000 + 4) = 0.99996
 *   A_redundant = 1 - (1 - 0.99996)^2 = 0.9999999984
 *
 * @param mtbf_hours     Mean Time Between Failures (hours)
 * @param mttr_hours     Mean Time To Repair (hours)
 * @return System availability (0.0 – 1.0)
 */
double ecs700_compute_availability(double mtbf_hours, double mttr_hours);

/**
 * @brief Compute Probability of Failure on Demand (PFD) for redundant system
 *
 * For 1oo2 (1-out-of-2) architecture:
 *   PFD_avg = (λ_D * T1)^2 / 3
 * where λ_D = dangerous failure rate, T1 = proof test interval.
 *
 * Knowledge Point: PFD Calculation — required by IEC 61508 for SIL
 * verification. For SIL 2: PFD_avg must be < 1E-2.
 *
 * @param lambda_d    Dangerous failure rate per hour
 * @param t1_hours    Proof test interval in hours
 * @return PFD_avg value
 */
double ecs700_compute_pfd_avg(double lambda_d, double t1_hours);

#ifdef __cplusplus
}
#endif

#endif /* ECS700_REDUNDANCY_H */
