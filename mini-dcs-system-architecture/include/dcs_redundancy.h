/**
 * @file dcs_redundancy.h
 * @brief DCS controller and network redundancy management.
 *
 * Knowledge Level: L2 Core Concepts + L3 Engineering Structures
 *
 * References:
 *   - Honeywell Experion PKS Redundancy Technical Guide (EP-DCS-RED-GDE)
 *   - IEC 61508:2010 Part 6, Annex B — Architectural constraints
 *   - Yokogawa CENTUM VP Pair & Spare Redundancy
 *
 * Covers hot standby synchronization, switchover logic,
 * bumpless transfer, and redundancy health monitoring.
 */

#ifndef DCS_REDUNDANCY_H
#define DCS_REDUNDANCY_H

#include "dcs_types.h"

/*===========================================================================
 * L2: Controller Redundancy Pair Definition
 *===========================================================================*/

/**
 * @brief Controller redundancy pair state.
 *
 * Represents a pair of redundant controllers (Primary + Standby).
 * The primary executes control logic; the standby synchronizes
 * data and is ready to take over.
 *
 * Synchronization completeness is the fraction of the primary's
 * database that has been successfully replicated to the standby:
 *   sync_pct = N_synced / N_total_tags
 */
typedef struct {
    uint32_t            pair_id;
    uint32_t            primary_node_id;
    uint32_t            standby_node_id;
    dcs_redundancy_mode_t mode;
    dcs_redundancy_arch_t architecture;
    int                 primary_active;       /* 1 = primary is running */
    int                 standby_ready;        /* 1 = standby synchronized */
    double              sync_completeness_pct;
    double              last_switchover_time_s;
    uint64_t            last_switchover_timestamp;
    double              switchover_count;
    double              data_sync_latency_ms;  /* Replication latency */
    int                 switchover_in_progress;
} dcs_redundant_pair_t;

/**
 * @brief Switchover trigger type.
 */
typedef enum {
    DCS_SWITCHOVER_MANUAL      = 0,  /* Operator initiated */
    DCS_SWITCHOVER_FAULT       = 1,  /* Primary hardware fault detected */
    DCS_SWITCHOVER_POWER_FAIL  = 2,  /* Primary power supply failure */
    DCS_SWITCHOVER_NETWORK_FAIL = 3, /* Primary network link down */
    DCS_SWITCHOVER_SOFTWARE    = 4,  /* Primary software exception */
    DCS_SWITCHOVER_SCHEDULED   = 5,  /* Scheduled maintenance */
    DCS_SWITCHOVER_WATCHDOG    = 6   /* Watchdog timer expiry */
} dcs_switchover_trigger_t;

/**
 * @brief Controller health status.
 */
typedef struct {
    uint32_t node_id;
    int      power_ok;
    int      processor_ok;
    int      memory_ok;
    int      network_link_ok;
    int      io_communication_ok;
    double   cpu_temperature_c;
    double   cpu_load_pct;
    double   memory_used_pct;
    double   uptime_hours;
    int      watchdog_enabled;
    int      overall_healthy;
} dcs_node_health_t;

/*===========================================================================
 * L3: Redundancy Core Functions
 *===========================================================================*/

/**
 * @brief Initialize a redundancy pair.
 *
 * Configures two controllers as a primary/standby pair.
 * Both must be of the same type and with compatible firmware.
 * The initialization assigns which node starts as primary based on
 * election criteria (lowest node_id or pre-configuration).
 *
 * @param pair            Output: initialized redundancy pair.
 * @param primary_id      Node ID of the primary controller.
 * @param standby_id      Node ID of the standby controller.
 * @param mode            Redundancy mode (hot/warm/cold standby).
 * @param arch            Redundancy architecture (1oo2, 2oo3, etc.).
 * @return                1 on success, 0 on invalid parameters.
 */
int dcs_redundancy_init(dcs_redundant_pair_t *pair,
                         uint32_t primary_id,
                         uint32_t standby_id,
                         dcs_redundancy_mode_t mode,
                         dcs_redundancy_arch_t arch);

/**
 * @brief Check if the standby is ready to take over.
 *
 * A standby is "ready" when:
 *   - Synchronization completeness ≥ 95% (or configurable threshold)
 *   - No fault conditions on standby
 *   - Standby network link is active
 *   - Data sync latency is within acceptable range
 *
 * @param pair             The redundancy pair to check.
 * @param sync_threshold   Minimum sync percentage required (e.g., 95.0).
 * @return                 1 if standby is ready, 0 otherwise.
 */
int dcs_redundancy_standby_ready(const dcs_redundant_pair_t *pair,
                                  double sync_threshold);

/**
 * @brief Execute a switchover from primary to standby.
 *
 * Hot standby switchover:
 *   1. Verify standby is ready (sync ≥ threshold)
 *   2. Freeze primary output (hold last value)
 *   3. Transfer active flag to standby
 *   4. Standby begins execution from synchronized state
 *   5. Old primary transitions to standby role
 *
 * Target switchover time for hot standby: < 1 scan cycle.
 *
 * @param pair     The redundancy pair to switch.
 * @param trigger  What triggered the switchover.
 * @param health_primary Health status of primary before switch.
 * @return         1 on successful switchover, 0 on failure.
 */
int dcs_redundancy_switchover(dcs_redundant_pair_t *pair,
                               dcs_switchover_trigger_t trigger,
                               const dcs_node_health_t *health_primary);

/**
 * @brief Synchronize data from primary to standby controller.
 *
 * Essential for hot standby: the standby must mirror the primary's
 * real-time database, control variables, and state machine positions.
 *
 * Key data synchronised:
 *   - Process tag values (PV, SP, OP)
 *   - PID loop states (integrator, derivative history)
 *   - Sequence states (step number, timers)
 *   - Alarm states (active, acknowledged)
 *
 * @param pair              The redundancy pair.
 * @param tags_synced       Output: number of tags successfully synchronized.
 * @return                  Sync completeness percentage (0-100).
 */
double dcs_redundancy_synchronize(dcs_redundant_pair_t *pair,
                                   uint32_t *tags_synced);

/**
 * @brief Calculate expected switchover time based on redundancy mode.
 *
 * Hot standby:     < 1 scan cycle (typically 10-50 ms)
 * Warm standby:    1-5 scan cycles (reload and reinitialize)
 * Cold standby:    10-60 seconds (full boot cycle)
 *
 * @param mode            Redundancy mode.
 * @param scan_period_ms  Controller scan period.
 * @return                Estimated switchover time in milliseconds.
 */
double dcs_calculate_switchover_time(dcs_redundancy_mode_t mode,
                                      double scan_period_ms);

/*===========================================================================
 * L5: Algorithm — Bumpless Transfer During Switchover
 *===========================================================================*/

/**
 * @brief Parameter structure for bumpless transfer algorithm.
 */
typedef struct {
    double ramp_time_s;           /* Ramp duration for smooth transition */
    double tracking_enabled;      /* Enable output tracking on standby */
    double max_step_change_pct;   /* Max allowed step in output (% of span) */
} dcs_bumpless_config_t;

/**
 * @brief Execute bumpless transfer during redundancy switchover.
 *
 * The bumpless algorithm ensures that when the standby takes over,
 * the output does not jump. It uses output tracking:
 *
 *   OP_standby(t0)  = OP_primary(t0-1)  (initial catch)
 *   OP_standby(t)   = OP_standby(t-1) + step * sign(target - current)
 *                     where step < max_step_change_pct * span / ramp_time
 *
 * Reference: Astrom & Hagglund (1995), Ch. 4, "Bumpless Transfer"
 *
 * @param current_output   Output value before switchover.
 * @param target_output    Desired output after transition.
 * @param output_span      Full span of output (max - min).
 * @param dt_s             Time step in seconds.
 * @param config           Bumpless transfer configuration.
 * @param new_output       Output: computed bumpless output value.
 * @return                 1 if transfer is complete, 0 if still ramping.
 */
int dcs_bumpless_transfer_step(double current_output,
                                double target_output,
                                double output_span,
                                double dt_s,
                                const dcs_bumpless_config_t *config,
                                double *new_output);

/*===========================================================================
 * L6: Classic Problem — Redundancy Failover Analysis
 *===========================================================================*/

/**
 * @brief Analyze the availability improvement from redundancy.
 *
 * For a single component with availability A:
 *   Simplex: A_simplex = A
 *   1oo2:    A_1oo2 = 1 - (1 - A)^2  (either can work)
 *   2oo2:    A_2oo2 = A^2             (both must work → lower availability
 *                                      but higher safety)
 *   2oo3:    A_2oo3 = A^3 + 3*A^2*(1-A)
 *
 * @param component_availability  Single component availability (0-1).
 * @param arch                    Redundancy architecture.
 * @return                        Combined availability.
 */
double dcs_analyze_redundancy_availability(double component_availability,
                                            dcs_redundancy_arch_t arch);

/**
 * @brief Calculate Mean Time To Repair (MTTR) based on redundancy architecture.
 *
 * Effective MTTR considers whether a redundant component can be repaired
 * while the system continues operating.
 *
 * @param single_mttr_hours  MTTR for a single component.
 * @param arch               Redundancy architecture.
 * @return                   Effective system MTTR in hours.
 */
double dcs_calculate_effective_mttr(double single_mttr_hours,
                                     dcs_redundancy_arch_t arch);

#endif /* DCS_REDUNDANCY_H */
