/**
 * @file dcs_redundancy.c
 * @brief DCS redundancy management implementation.
 *
 * Covers hot standby synchronization, switchover logic,
 * bumpless transfer, and redundancy availability analysis.
 *
 * Knowledge Levels: L2, L3, L5, L6
 */

#include "dcs_redundancy.h"
#include <math.h>
#include <string.h>

/*===========================================================================
 * L3: Redundancy Initialization
 *===========================================================================*/

int dcs_redundancy_init(dcs_redundant_pair_t *pair,
                         uint32_t primary_id,
                         uint32_t standby_id,
                         dcs_redundancy_mode_t mode,
                         dcs_redundancy_arch_t arch)
{
    if (pair == NULL) return 0;
    if (primary_id == standby_id) return 0; /* Must be distinct nodes */

    memset(pair, 0, sizeof(dcs_redundant_pair_t));

    pair->pair_id              = primary_id; /* Pair ID = primary node ID */
    pair->primary_node_id      = primary_id;
    pair->standby_node_id      = standby_id;
    pair->mode                 = mode;
    pair->architecture         = arch;
    pair->primary_active       = 1;
    pair->standby_ready        = (mode == DCS_REDUNDANCY_MODE_HOT_STANDBY) ? 1 : 0;
    pair->sync_completeness_pct = (mode == DCS_REDUNDANCY_MODE_HOT_STANDBY) ? 100.0 : 0.0;
    pair->last_switchover_time_s = 0.0;
    pair->last_switchover_timestamp = 0;
    pair->switchover_count     = 0.0;
    pair->data_sync_latency_ms = 0.0;
    pair->switchover_in_progress = 0;

    return 1;
}

/*===========================================================================
 * L3: Standby Readiness Check
 *===========================================================================*/

int dcs_redundancy_standby_ready(const dcs_redundant_pair_t *pair,
                                  double sync_threshold)
{
    if (pair == NULL) return 0;

    /* Standby must exist */
    if (pair->standby_node_id == 0) return 0;

    /* Synchronization must meet threshold */
    if (pair->sync_completeness_pct < sync_threshold) return 0;

    /* No switchover already in progress */
    if (pair->switchover_in_progress) return 0;

    /* For hot standby: sync latency must be acceptable (< 50 ms) */
    if (pair->mode == DCS_REDUNDANCY_MODE_HOT_STANDBY) {
        if (pair->data_sync_latency_ms > 50.0) return 0;
    }

    return 1;
}

/*===========================================================================
 * L3: Switchover Execution
 *===========================================================================*/

int dcs_redundancy_switchover(dcs_redundant_pair_t *pair,
                               dcs_switchover_trigger_t trigger,
                               const dcs_node_health_t *health_primary)
{
    if (pair == NULL) return 0;

    /* Already switching */
    if (pair->switchover_in_progress) return 0;

    /* Standby must be ready for graceful switchover */
    if (trigger == DCS_SWITCHOVER_MANUAL
        || trigger == DCS_SWITCHOVER_SCHEDULED) {
        if (!dcs_redundancy_standby_ready(pair, 95.0)) {
            return 0;
        }
    }

    /* For fault-triggered switchover, check primary health */
    if (trigger == DCS_SWITCHOVER_FAULT
        || trigger == DCS_SWITCHOVER_POWER_FAIL
        || trigger == DCS_SWITCHOVER_WATCHDOG) {
        if (health_primary != NULL && health_primary->overall_healthy) {
            /* Primary says it's healthy — spurious trigger? */
            return 0;
        }
    }

    /* Execute switchover:
     *   1. Mark in-progress
     *   2. Swap roles: old standby → primary, old primary → standby
     *   3. Record metrics
     */
    pair->switchover_in_progress = 1;

    /* Swap node IDs */
    uint32_t old_primary = pair->primary_node_id;
    pair->primary_node_id = pair->standby_node_id;
    pair->standby_node_id = old_primary;

    /* New primary becomes active */
    pair->primary_active = 1;
    pair->standby_ready = 0; /* Old primary must resync as standby */

    /* Record switchover metrics */
    pair->switchover_count += 1.0;

    /* Estimate switchover time based on mode */
    double scan_period_est = 250.0; /* Default 250 ms scan */
    pair->last_switchover_time_s =
        dcs_calculate_switchover_time(pair->mode, scan_period_est) / 1000.0;

    /* Clear in-progress flag (actual DCS would do this async) */
    pair->switchover_in_progress = 0;

    return 1;
}

/*===========================================================================
 * L3: Data Synchronization
 *===========================================================================*/

double dcs_redundancy_synchronize(dcs_redundant_pair_t *pair,
                                   uint32_t *tags_synced)
{
    if (pair == NULL) return 0.0;
    if (tags_synced != NULL) *tags_synced = 0;

    /* Only synchronize if primary is active */
    if (!pair->primary_active) return 0.0;

    /*
     * Synchronization model:
     *
     * In hot standby, the primary continuously sends database updates
     * to the standby. The synchronization rate depends on:
     *   - Available bandwidth between controllers
     *   - Number of changed tags per scan
     *   - Overhead of sync protocol
     *
     * Typical sync bandwidth: 10-100 Mbps dedicated link
     * Tag record size: ~64 bytes (ID + value + quality + timestamp)
     * Changed tags per scan: ~10% of total on average
     *
     * sync_completeness = previously_synced + newly_synced / total
     */

    /* For this implementation, we simulate sync progress */
    uint32_t total_tags = 1000; /* Typical controller has ~1000 tags */
    uint32_t changed_per_scan = total_tags / 10; /* ~10% change per scan */

    /* Simulate synchronization based on current completeness */
    double remaining = 100.0 - pair->sync_completeness_pct;
    if (remaining > 0.0) {
        /*
         * Sync rate: each call synchronizes ~5% of remaining tags
         * This approximates the convergence behavior:
         *   sync(t) = 100 * (1 - e^(-t/tau))
         *   where tau depends on bandwidth and tag count
         */
        double sync_step = remaining * 0.15; /* 15% of remaining per call */
        if (sync_step < 0.1) sync_step = 0.1; /* Minimum progress */
        pair->sync_completeness_pct += sync_step;
        if (pair->sync_completeness_pct > 100.0) {
            pair->sync_completeness_pct = 100.0;
        }

        uint32_t newly_synced = (uint32_t)(changed_per_scan * (sync_step / 100.0));
        if (tags_synced != NULL) {
            *tags_synced = newly_synced;
        }
    }

    /* Update standby readiness */
    if (pair->sync_completeness_pct >= 95.0) {
        pair->standby_ready = 1;
    }

    /* Update sync latency estimate */
    double sync_bw_mbps = 50.0; /* 50 Mbps dedicated sync link */
    double total_data_kb = (double)changed_per_scan * 64.0 / 1024.0; /* KB */
    pair->data_sync_latency_ms = total_data_kb * 8.0 / sync_bw_mbps; /* ms */

    return pair->sync_completeness_pct;
}

/*===========================================================================
 * L3: Switchover Time Calculation
 *===========================================================================*/

double dcs_calculate_switchover_time(dcs_redundancy_mode_t mode,
                                      double scan_period_ms)
{
    if (scan_period_ms <= 0.0) scan_period_ms = 250.0;

    switch (mode) {
        case DCS_REDUNDANCY_MODE_HOT_STANDBY:
            /*
             * Hot standby: standby is synchronized and executing in parallel.
             * Switchover time = detection time + role transition
             *   Detection: 1-2 scan periods (watchdog or heartbeat)
             *   Transition: < 10 ms (flag swap)
             * Total: ~0.5-1 scan period
             */
            return scan_period_ms * 0.5;

        case DCS_REDUNDANCY_MODE_WARM_STANDBY:
            /*
             * Warm standby: standby has database but not executing.
             * Switchover time = detection + database reload + startup
             *   Detection: 1-2 scan periods
             *   Database reload: 500-2000 ms
             *   Startup: 1 scan period
             * Total: 3-5 scan periods
             */
            return scan_period_ms * 4.0;

        case DCS_REDUNDANCY_MODE_COLD_STANDBY:
            /*
             * Cold standby: standby powered off or uninitialized.
             * Switchover time = boot + load + start
             *   Boot: 10-30 seconds
             *   Load configuration: 5-15 seconds
             *   Synchronize: 2-10 seconds
             * Total: 20-60 seconds
             */
            return 30000.0; /* 30 seconds average */

        case DCS_REDUNDANCY_MODE_ACTIVE_ACTIVE:
            /*
             * Active-active: both controllers process I/O.
             * No switchover needed — load sharing.
             * If one fails, the other absorbs its load instantly.
             */
            return scan_period_ms * 0.1; /* Essentially immediate */

        default:
            return scan_period_ms;
    }
}

/*===========================================================================
 * L5: Bumpless Transfer Algorithm
 *===========================================================================*/

int dcs_bumpless_transfer_step(double current_output,
                                double target_output,
                                double output_span,
                                double dt_s,
                                const dcs_bumpless_config_t *config,
                                double *new_output)
{
    if (config == NULL || new_output == NULL) return 0;
    if (output_span <= 0.0) output_span = 100.0; /* Default 0-100% */
    if (dt_s <= 0.0) dt_s = 0.1;

    double diff = target_output - current_output;
    double abs_diff = (diff < 0.0) ? -diff : diff;

    /* If already at target, transfer complete */
    if (abs_diff < output_span * 0.001) { /* Within 0.1% of span */
        *new_output = target_output;
        return 1;
    }

    /*
     * Ramp towards target at a controlled rate.
     *
     * Max step per iteration = max_step_change_pct / 100 * span * dt / ramp_time
     *
     * This ensures the transition is smooth and does not upset the process.
     *
     * Reference: Bumpless transfer with velocity limiting
     * (Astrom & Hagglund, 1995, Section 4.2)
     */
    double max_step_pct_per_sec;
    if (config->ramp_time_s > 0.0) {
        max_step_pct_per_sec = config->max_step_change_pct / config->ramp_time_s;
    } else {
        max_step_pct_per_sec = config->max_step_change_pct; /* Instant if ramp=0 */
    }

    double max_step = max_step_pct_per_sec / 100.0 * output_span * dt_s;

    double step;
    if (diff > 0.0) {
        step = (diff < max_step) ? diff : max_step;
    } else {
        step = (diff > -max_step) ? diff : -max_step;
    }

    *new_output = current_output + step;

    /* Check if we'll reach target in next step */
    if ((step > 0.0 && *new_output >= target_output)
        || (step < 0.0 && *new_output <= target_output)) {
        return 1; /* Transfer complete */
    }

    return 0; /* Still transitioning */
}

/*===========================================================================
 * L6: Redundancy Availability Analysis
 *===========================================================================*/

double dcs_analyze_redundancy_availability(double component_availability,
                                            dcs_redundancy_arch_t arch)
{
    if (component_availability < 0.0) component_availability = 0.0;
    if (component_availability > 1.0) component_availability = 1.0;

    double a = component_availability;

    /*
     * Reliability block diagram analysis per IEC 61508-6.
     *
     * Formulas:
     *   1oo1:  A = a
     *   1oo2:  A = 1 - (1 - a)^2  = 2a - a^2  (either works)
     *   2oo2:  A = a^2             (both must work — LOWER availability)
     *   2oo3:  A = a^3 + 3*a^2*(1 - a)  (any 2 of 3 must work)
     *          = 3a^2 - 2a^3
     *   1oo2D: A = 1 - (1 - a)^2  = 1oo2 (for availability purposes)
     */
    switch (arch) {
        case DCS_REDUNDANCY_1OO1:
            return a;

        case DCS_REDUNDANCY_1OO2:
        case DCS_REDUNDANCY_1OO2D:
            return 2.0 * a - a * a;

        case DCS_REDUNDANCY_2OO2:
            return a * a;

        case DCS_REDUNDANCY_2OO3:
            return 3.0 * a * a - 2.0 * a * a * a;

        default:
            return a;
    }
}

double dcs_calculate_effective_mttr(double single_mttr_hours,
                                     dcs_redundancy_arch_t arch)
{
    if (single_mttr_hours <= 0.0) return 0.0;

    /*
     * Effective MTTR considering redundancy.
     *
     * For architectures where failure is tolerated (1oo2, 2oo3):
     *   Effective MTTR = single MTTR (can repair while running)
     *   BUT the system doesn't fail, so MTTR for system failure → ∞
     *
     * For architectures where failure causes system trip (1oo1, 2oo2):
     *   Effective MTTR = single MTTR (system is down until repair)
     *
     * We return the "system failure MTTR" — the expected downtime
     * when the system actually fails (not when a redundant component fails).
     */
    switch (arch) {
        case DCS_REDUNDANCY_1OO2:
        case DCS_REDUNDANCY_1OO2D:
            /*
             * Single failure doesn't bring system down.
             * System fails only if second channel fails before repair of first.
             * Probability of this is very low, so effective MTTR is much higher.
             *
             * Approximate: MTTReff = single_mttr / (1 - a_pair)
             * where a_pair is the redundant pair availability.
             * For typical a = 0.9995, a_pair_1oo2 = 0.99999975
             * MTTReff ≈ single_mttr / 2.5e-7 ≈ 4,000,000 * single_mttr
             * But we cap for practical purposes.
             */
            return single_mttr_hours * 1000.0; /* Much better than simplex */

        case DCS_REDUNDANCY_2OO3:
            /*
             * Can tolerate single failure. System fails on 2nd failure.
             * Slightly worse than 1oo2 but much better than simplex.
             */
            return single_mttr_hours * 500.0;

        case DCS_REDUNDANCY_1OO1:
        case DCS_REDUNDANCY_2OO2:
            /*
             * Any failure causes system trip (1oo1: single failure;
             * 2oo2: any single failure, because both must work).
             */
            return single_mttr_hours;

        default:
            return single_mttr_hours;
    }
}
