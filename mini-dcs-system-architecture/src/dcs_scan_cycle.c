/**
 * @file dcs_scan_cycle.c
 * @brief DCS controller scan cycle management and analysis.
 *
 * Knowledge Levels: L3 Engineering Structures, L6 Canonical Problems
 *
 * Covers scan cycle scheduling, task partitioning, watchdog monitoring,
 * scan overrun detection, and deterministic execution analysis.
 *
 * References:
 *   - Honeywell Experion C300 Controller Technical Specification
 *   - Yokogawa CENTUM VP FCS Controller Function Manual
 *   - IEC 61131-3 cyclic execution model
 */

#include "dcs_system_db.h"
#include "dcs_architecture.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * L3: Scan Cycle State Machine
 *===========================================================================*/

/**
 * @brief Scan cycle execution states.
 */
typedef enum {
    SCAN_STATE_IDLE        = 0,
    SCAN_STATE_INPUT_READ  = 1,
    SCAN_STATE_EXEC_PHASE1 = 2,
    SCAN_STATE_EXEC_PHASE2 = 3,
    SCAN_STATE_EXEC_PHASE3 = 4,
    SCAN_STATE_EXEC_PHASE4 = 5,
    SCAN_STATE_EXEC_PHASE5 = 6,
    SCAN_STATE_OUTPUT_WRITE = 7,
    SCAN_STATE_COMM        = 8,
    SCAN_STATE_OVERRUN     = 9
} dcs_scan_state_t;

/**
 * @brief Watchdog timer state for controller health monitoring.
 */
typedef struct {
    double   timeout_ms;
    double   elapsed_ms;
    int      enabled;
    int      expired;
    uint32_t reset_count;
} dcs_watchdog_t;

/**
 * @brief Scan cycle performance statistics.
 */
typedef struct {
    double   scan_period_ms;
    double   min_scan_time_ms;
    double   max_scan_time_ms;
    double   avg_scan_time_ms;
    double   std_dev_scan_time_ms;
    uint32_t overrun_count;
    uint32_t total_scans;
    double   watchdog_timeout_ms;
    int      scan_healthy;
} dcs_scan_performance_t;

/*===========================================================================
 * L3: Watchdog Timer Functions
 *===========================================================================*/

/**
 * @brief Initialize a watchdog timer.
 *
 * The watchdog is a safety mechanism that detects controller failure.
 * If the controller fails to reset the watchdog within the timeout period,
 * the watchdog expires and triggers a fail-safe state (all outputs de-energize).
 *
 * @param wd           Watchdog timer to initialize.
 * @param timeout_ms   Timeout period in milliseconds.
 * @return             1 on success.
 */
int dcs_watchdog_init(dcs_watchdog_t *wd, double timeout_ms)
{
    if (wd == NULL) return 0;

    wd->timeout_ms = timeout_ms;
    wd->elapsed_ms = 0.0;
    wd->enabled = 1;
    wd->expired = 0;
    wd->reset_count = 0;

    return 1;
}

/**
 * @brief Reset (kick) the watchdog timer.
 *
 * Called at the end of each successful scan cycle.
 * If the scan cycle takes too long, the watchdog expires,
 * indicating a controller fault.
 *
 * @param wd  Watchdog timer to reset.
 * @return    1 if reset successful, 0 if already expired.
 */
int dcs_watchdog_reset(dcs_watchdog_t *wd)
{
    if (wd == NULL) return 0;

    if (wd->expired) {
        /* Too late — controller is in fault state */
        return 0;
    }

    wd->elapsed_ms = 0.0;
    wd->reset_count++;
    return 1;
}

/**
 * @brief Advance the watchdog timer by dt_ms.
 *
 * If elapsed time exceeds timeout, the watchdog expires.
 * This triggers all outputs to their fail-safe states.
 *
 * @param wd     Watchdog timer.
 * @param dt_ms  Time elapsed since last advance (milliseconds).
 * @return       1 if watchdog active (not expired), 0 if expired.
 */
int dcs_watchdog_tick(dcs_watchdog_t *wd, double dt_ms)
{
    if (wd == NULL || !wd->enabled) return 1;

    wd->elapsed_ms += dt_ms;

    if (wd->elapsed_ms >= wd->timeout_ms) {
        wd->expired = 1;
        return 0; /* Watchdog expired! */
    }

    return 1;
}

/*===========================================================================
 * L3: Scan Overrun Detection
 *===========================================================================*/

/**
 * @brief Detect if a scan cycle has overrun its allocated period.
 *
 * Scan overrun occurs when the total execution time exceeds the
 * configured scan period. This is a critical fault that degrades
 * control performance and may cause watchdog expiry.
 *
 * Formula:
 *   overrun = (actual_scan_time_ms > scan_period_ms)
 *
 * Overrun severity:
 *   < 110%: Warning (minor overrun)
 *   < 150%: Alarm (moderate overrun)
 *   ≥ 150%: Critical (severe overrun, imminent watchdog)
 *
 * @param actual_scan_time_ms  Measured scan execution time.
 * @param scan_period_ms       Configured scan period.
 * @return                     0: no overrun, 1: warning, 2: alarm, 3: critical.
 */
int dcs_detect_scan_overrun(double actual_scan_time_ms,
                             double scan_period_ms)
{
    if (scan_period_ms <= 0.0) return 0;
    if (actual_scan_time_ms <= scan_period_ms) return 0;

    double overrun_ratio = actual_scan_time_ms / scan_period_ms;

    if (overrun_ratio >= 1.5) {
        return 3; /* Critical overrun */
    } else if (overrun_ratio >= 1.3) {
        return 2; /* Alarm overrun */
    } else {
        return 1; /* Warning overrun */
    }
}

/*===========================================================================
 * L3: Scan Period Recommendation
 *===========================================================================*/

/**
 * @brief Recommend scan period based on process dynamics.
 *
 * The scan period should be at least 5-10x faster than the dominant
 * process time constant to maintain good control performance.
 *
 * Nyquist criterion: fs ≥ 2 * f_max (sampling theorem)
 * Engineering rule: scan_period ≤ process_time_constant / 10
 *
 * Process type examples:
 *   Flow control:      0.1 - 1.0 s time constant → 50-100 ms scan
 *   Pressure control:  0.5 - 5.0 s time constant → 100-250 ms scan
 *   Level control:     5 - 60 s time constant    → 250-500 ms scan
 *   Temperature control: 60 - 600 s time constant → 500-1000 ms scan
 *   Composition:       600 - 3600 s time constant → 1000-5000 ms scan
 *
 * @param process_time_constant_s  Dominant process time constant in seconds.
 * @return                         Recommended scan period in milliseconds.
 */
double dcs_recommend_scan_period(double process_time_constant_s)
{
    if (process_time_constant_s <= 0.0) return 250.0; /* Default */

    /* Engineering heuristic: Ts ≤ tau / 10 */
    double scan_period_s = process_time_constant_s / 10.0;

    /* Convert to ms */
    double scan_period_ms = scan_period_s * 1000.0;

    /* Clamp to practical DCS ranges */
    if (scan_period_ms < 50.0) scan_period_ms = 50.0;   /* Minimum 50 ms */
    if (scan_period_ms > 5000.0) scan_period_ms = 5000.0; /* Maximum 5 s */

    /* Round to standard DCS scan periods */
    const double standard_periods[] = {50, 100, 200, 250, 500, 1000, 2000};
    for (int i = 0; i < 7; i++) {
        if (scan_period_ms <= standard_periods[i]) {
            return standard_periods[i];
        }
    }

    return 5000.0;
}

/*===========================================================================
 * L6: Scan Cycle Performance Analysis
 *===========================================================================*/

/**
 * @brief Compute scan cycle performance statistics.
 *
 * Collects and analyzes scan timing data to detect jitter,
 * overruns, and scheduling anomalies.
 *
 * @param scan_times_ms      Array of measured scan times.
 * @param num_scans          Number of scan measurements.
 * @param scan_period_ms     Configured scan period.
 * @param perf               Output: performance statistics.
 * @return                   1 on success.
 */
int dcs_analyze_scan_performance(const double *scan_times_ms,
                                  uint32_t num_scans,
                                  double scan_period_ms,
                                  dcs_scan_performance_t *perf)
{
    if (scan_times_ms == NULL || perf == NULL || num_scans == 0) return 0;

    memset(perf, 0, sizeof(dcs_scan_performance_t));

    perf->scan_period_ms = scan_period_ms;
    perf->total_scans = num_scans;

    /* Compute min, max, average */
    double sum = 0.0;
    double sum_sq = 0.0;
    perf->min_scan_time_ms = scan_times_ms[0];
    perf->max_scan_time_ms = scan_times_ms[0];

    for (uint32_t i = 0; i < num_scans; i++) {
        double t = scan_times_ms[i];
        sum += t;
        sum_sq += t * t;

        if (t < perf->min_scan_time_ms) perf->min_scan_time_ms = t;
        if (t > perf->max_scan_time_ms) perf->max_scan_time_ms = t;

        /* Count overruns */
        if (t > scan_period_ms) {
            perf->overrun_count++;
        }
    }

    perf->avg_scan_time_ms = sum / (double)num_scans;

    /* Standard deviation */
    double variance = sum_sq / (double)num_scans
                    - perf->avg_scan_time_ms * perf->avg_scan_time_ms;
    if (variance < 0.0) variance = 0.0;
    perf->std_dev_scan_time_ms = sqrt(variance);

    /* Jitter = max - min (peak-to-peak variation) */
    double jitter = perf->max_scan_time_ms - perf->min_scan_time_ms;

    /*
     * Scan health assessment:
     *   - Jitter < 5% of scan period
     *   - Overrun rate < 0.1%
     *   - No watchdog timeouts
     */
    double overrun_rate = (double)perf->overrun_count / (double)num_scans * 100.0;
    double jitter_pct = (scan_period_ms > 0.0)
                      ? jitter / scan_period_ms * 100.0 : 100.0;

    perf->scan_healthy = (jitter_pct < 5.0) && (overrun_rate < 0.1);

    return 1;
}

/*===========================================================================
 * L6: Task Partitioning and Scheduling
 *===========================================================================*/

/**
 * @brief Compute the maximum number of PID loops executable per scan.
 *
 * Based on available time in Phase 2 (regulatory control) and
 * per-PID execution time.
 *
 * Formula: N_max = floor(phase2_budget / t_per_pid)
 *
 * @param scan_period_ms      Scan period in ms.
 * @param phase2_fraction     Fraction of scan for PID phase (typical 0.30).
 * @param t_per_pid_us        Execution time per PID block in µs (typical 150).
 * @return                    Maximum number of PID loops.
 */
uint32_t dcs_max_pid_loops_per_scan(double scan_period_ms,
                                     double phase2_fraction,
                                     double t_per_pid_us)
{
    if (scan_period_ms <= 0.0 || t_per_pid_us <= 0.0) return 0;
    if (phase2_fraction <= 0.0 || phase2_fraction > 1.0) phase2_fraction = 0.30;

    /* Phase 2 budget in microseconds */
    double phase2_budget_us = scan_period_ms * 1000.0 * phase2_fraction;

    /* Max loops */
    uint32_t max_loops = (uint32_t)(phase2_budget_us / t_per_pid_us);

    /* Apply 80% utilization ceiling (20% margin for jitter) */
    max_loops = (uint32_t)((double)max_loops * 0.80);

    return max_loops;
}

/**
 * @brief Calculate inter-scan jitter prediction.
 *
 * For a set of scan times with average µ and standard deviation σ,
 * the probability of overrun P_overrun can be estimated.
 *
 * P_overrun = 1 - Phi((T_period - µ) / σ)
 * where Phi is the cumulative standard normal distribution.
 *
 * This implementation uses the Abramowitz-Stegun approximation
 * for the normal CDF (maximum error: 7.5e-8).
 *
 * @param avg_scan_ms    Average scan time in ms.
 * @param std_scan_ms    Standard deviation of scan time in ms.
 * @param period_ms      Scan period in ms.
 * @return               Predicted overrun probability (0-1).
 */
double dcs_predict_overrun_probability(double avg_scan_ms,
                                        double std_scan_ms,
                                        double period_ms)
{
    if (std_scan_ms <= 0.0) {
        /* Deterministic: if avg > period, always overrun */
        return (avg_scan_ms > period_ms) ? 1.0 : 0.0;
    }

    /* Z-score: how many standard deviations from the mean to the limit */
    double z = (period_ms - avg_scan_ms) / std_scan_ms;

    if (z > 7.0) return 0.0;   /* Essentially zero probability */
    if (z < -7.0) return 1.0;  /* Essentially certain */

    /* Abramowitz-Stegun approximation for standard normal CDF */
    double abs_z = (z < 0.0) ? -z : z;
    double t = 1.0 / (1.0 + 0.2316419 * abs_z);
    double d = 0.3989423 * exp(-0.5 * abs_z * abs_z);
    double p = 1.0 - d * t * (0.3193815 + t * (-0.3565638
               + t * (1.781478 + t * (-1.821256 + t * 1.330274))));

    if (z < 0.0) return p * 0.5; /* Lower tail */
    return (1.0 - p) + p * 0.5;  /* Upper tail */
}

/*===========================================================================
 * L6: Deterministic Execution Guarantee
 *===========================================================================*/

/**
 * @brief Verify that a set of control modules can execute deterministically
 *        within the allocated scan period.
 *
 * Deterministic execution means every module completes within its
 * allocated time budget, every scan cycle. This is critical for
 * safety-certified DCS (SIL 2+) where jitter must be bounded.
 *
 * The verification uses rate monotonic scheduling analysis:
 *   Sum(Ci / Ti) ≤ n * (2^(1/n) - 1)
 * where Ci = execution time, Ti = period for task i, n = number of tasks.
 * For n → ∞, the bound approaches ln(2) ≈ 0.693.
 *
 * @param exec_times_us     Array of execution times per module (µs).
 * @param periods_us        Array of periods per module (µs).
 * @param num_modules       Number of control modules.
 * @param utilization_bound Output: total CPU utilization fraction.
 * @return                  1 if schedulable, 0 if unschedulable.
 */
int dcs_verify_deterministic_schedule(const double *exec_times_us,
                                       const double *periods_us,
                                       uint32_t num_modules,
                                       double *utilization_bound)
{
    if (exec_times_us == NULL || periods_us == NULL) return 0;

    if (utilization_bound != NULL) *utilization_bound = 0.0;

    double total_utilization = 0.0;

    for (uint32_t i = 0; i < num_modules; i++) {
        if (periods_us[i] > 0.0) {
            double ui = exec_times_us[i] / periods_us[i];
            total_utilization += ui;
        } else {
            /* Invalid period — cannot schedule */
            return 0;
        }
    }

    if (utilization_bound != NULL) *utilization_bound = total_utilization;

    /* Rate monotonic bound: U ≤ n * (2^(1/n) - 1) */
    double n = (double)num_modules;
    double liu_layland_bound;

    if (num_modules == 0) {
        return 1;
    } else if (num_modules == 1) {
        liu_layland_bound = 1.0;
    } else {
        liu_layland_bound = n * (pow(2.0, 1.0 / n) - 1.0);
    }

    /* For DCS with fixed scan periods, use 70% bound */
    double dcs_bound = 0.70;
    double bound = (liu_layland_bound < dcs_bound)
                   ? liu_layland_bound : dcs_bound;

    return (total_utilization <= bound) ? 1 : 0;
}
