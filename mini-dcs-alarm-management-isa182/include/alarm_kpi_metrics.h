/**
 * @file alarm_kpi_metrics.h
 * @brief ISA-18.2 Alarm System Performance Metrics — KPI Calculation,
 *        Reporting, and Analysis (L3, L5)
 *
 * Knowledge Points:
 *   isa18_kpi_init                      — Initialize KPI counter structure (L3)
 *   isa18_kpi_calc_alarms_per_day       — Daily alarm rate per operator (L5)
 *   isa18_kpi_calc_peak_rate            — Peak alarm rate in 10-min window (L5)
 *   isa18_kpi_update_with_event         — Incrementally update KPIs on new event (L5)
 *   isa18_kpi_detect_chattering         — Chattering detection algorithm (L5)
 *   isa18_kpi_calc_priority_distribution — Priority distribution percentage (L3)
 *   isa18_kpi_detect_nuisance_alarms    — Nuisance alarm identification (L5)
 *   isa18_kpi_top_n_frequent            — Top-N most frequent alarms (L5)
 *   isa18_kpi_assess_eemua_benchmark    — Compare against EEMUA 191 benchmarks (L4)
 *   isa18_kpi_overall_health_score      — Composite alarm health score (L5)
 *   isa18_kpi_generate_report           — Generate KPI report string (L3)
 *   isa18_kpi_rolling_window_update     — Maintain rolling 24h event window (L5)
 *   isa18_kpi_calc_avg_response_time    — Average operator response time (L5)
 *   isa18_kpi_chattering_reset          — Reset chattering detection (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §16 (Monitoring and Assessment)
 *   - ISA-TR18.2.5-2012 (Alarm System Monitoring)
 *   - EEMUA Publication 191 §8 (Measuring Performance)
 *   - ASM Consortium Guidelines (Abnormal Situation Management)
 */

#ifndef ALARM_KPI_METRICS_H
#define ALARM_KPI_METRICS_H

#include "alarm_management_types.h"

/*============================================================================
 * L3 — Initialize KPI Counters
 *
 * Sets all KPI counters to zero and records the period start time.
 *============================================================================*/
void isa18_kpi_init(
    isa18_kpi_counts_t *kpi,
    time_t period_start,
    uint32_t number_of_operators);

/*============================================================================
 * L5 — Calculate Alarms Per Day Per Operator
 *
 * Primary KPI from EEMUA 191 Table 8-1:
 *   Average alarms per day per operator position
 *
 * Formula: alarms_per_day = total_alarms_in_period × (86400 / period_duration_sec) / operators
 *
 * Benchmarks:
 *   ≤ 150 / day  — Acceptable (steady state)
 *   ≤ 300 / day  — Manageable
 *   > 300 / day  — Demanding (action required)
 *   > 600 / day  — Unacceptable
 *
 * Returns: alarms per day per operator
 *============================================================================*/
double isa18_kpi_calc_alarms_per_day(
    const isa18_kpi_counts_t *kpi,
    time_t current_time);

/*============================================================================
 * L5 — Calculate Peak Alarm Rate
 *
 * ISA-18.2 §16.4.2: The peak number of alarm activations in any
 * 10-minute sliding window over the reporting period.
 *
 * Benchmarks per EEMUA 191:
 *   ≤ 10 / 10 min  — Acceptable
 *   11-30 / 10 min  — Manageable
 *   > 30 / 10 min   — Over-manageable (remedial action required)
 *
 * Returns: peak alarm count in a 10-minute window
 *============================================================================*/
uint32_t isa18_kpi_calc_peak_rate(
    const isa18_history_entry_t *history,
    uint32_t history_count,
    time_t current_time,
    uint32_t window_sec);

/*============================================================================
 * L5 — Incremental KPI Update on Alarm Event
 *
 * Updates all relevant KPI counters when a new alarm event occurs.
 * Called from the alarm engine scan cycle.
 *
 * Updates:
 *   - Priority-specific counters (if state → ACTIVE_UNACK)
 *   - Active alarm count
 *   - Peak rate trackers
 *   - Response time statistics (on acknowledge)
 *============================================================================*/
void isa18_kpi_update_with_event(
    isa18_kpi_counts_t *kpi,
    const isa18_alarm_event_t *event,
    const isa18_alarm_config_t *alarm);

/*============================================================================
 * L5 — Chattering Alarm Detection (ISA-18.2 §16.6)
 *
 * A chattering alarm is one that repeatedly transitions between
 * active and normal states in a short time window.
 *
 * Detection algorithm:
 *   1. Record each transition timestamp
 *   2. Count transitions in the last CHATTER_WINDOW_SEC
 *   3. If count >= CHATTER_THRESHOLD_COUNT, flag as chattering
 *
 * Chattering is typically caused by:
 *   - Process variable oscillating near the setpoint
 *   - Insufficient deadband
 *   - Noisy measurements
 *   - Process instability
 *
 * Returns: true if chattering is now detected
 *============================================================================*/
bool isa18_kpi_detect_chattering(
    isa18_chattering_detector_t *detector,
    time_t transition_time);

/*============================================================================
 * L5 — Reset Chattering Detection
 *
 * Called when an alarm is acknowledged or the deadband is adjusted.
 *============================================================================*/
void isa18_kpi_chattering_reset(
    isa18_chattering_detector_t *detector);

/*============================================================================
 * L3 — Calculate Priority Distribution
 *
 * Computes the percentage breakdown of alarms by priority level.
 *
 * ISA-18.2 §16.4.4: The distribution should match rationalization targets.
 * A shift toward higher priorities indicates deteriorating process health.
 *============================================================================*/
void isa18_kpi_calc_priority_distribution(
    const isa18_kpi_counts_t *kpi,
    double *out_critical_pct,
    double *out_high_pct,
    double *out_medium_pct,
    double *out_low_pct);

/*============================================================================
 * L5 — Nuisance Alarm Detection (ISA-18.2 §16.7)
 *
 * A nuisance alarm is one that:
 *   - Does not require operator action (but was configured as an alarm)
 *   - Is consistently acknowledged without corrective action
 *   - Is consistently shelved by operators
 *
 * Detection heuristic: if an alarm's average time from activation to
 * acknowledgment is very short (< 5 seconds) and no operator action
 * is recorded, it's likely a nuisance.
 *
 * Returns: true if the alarm is likely a nuisance
 *============================================================================*/
bool isa18_kpi_detect_nuisance_alarm(
    double avg_ack_time_sec,
    uint32_t activation_count,
    uint32_t shelve_count);

/*============================================================================
 * L5 — Top-N Most Frequent Alarms (ISA-18.2 §16.4.1)
 *
 * Identifies the alarms that activate most frequently.
 * ISA-18.2 requires that "bad actor" alarms be identified
 * and subjected to re-rationalization.
 *
 * This uses a frequency counting approach: O(m × n) where
 * m = history_count, n = top_n.
 *
 * Returns: number of entries populated in result array
 *============================================================================*/
uint32_t isa18_kpi_top_n_frequent(
    const isa18_history_entry_t *history,
    uint32_t history_count,
    isa18_frequent_alarm_entry_t *result,
    uint32_t top_n);

/*============================================================================
 * L4 — Assess Against EEMUA 191 Benchmarks
 *
 * Compares current KPI values against EEMUA 191 recommended targets.
 *
 * Returns a score for each metric:
 *   0 = Unacceptable (action required)
 *   1 = Demanding (improvement needed)
 *   2 = Manageable (monitor)
 *   3 = Acceptable (no action needed)
 *   4 = Excellent (industry-leading)
 *
 * Metrics assessed:
 *   - Alarms per day per operator
 *   - Peak alarm rate per 10 minutes
 *   - Percentage of hours with >30 alarms
 *   - Percentage of alarms at each priority
 *   - Stale alarm count (active > 24h)
 *   - Chattering alarm count
 *============================================================================*/
void isa18_kpi_assess_eemua_benchmark(
    const isa18_kpi_counts_t *kpi,
    time_t current_time,
    int scores[7]);

/*============================================================================
 * L5 — Composite Alarm System Health Score
 *
 * ISA-18.2 §16.8: Combines multiple KPI metrics into a single
 * 0-100 health score for management reporting.
 *
 * Formula:
 *   health_score = w1×alarm_rate_score + w2×peak_rate_score
 *                + w3×stale_score + w4×response_score
 *                + w5×chatter_score + w6×coverage_score
 *
 * Where each sub-score is 0-100 and weights sum to 1.0.
 *
 * Returns: composite health score [0, 100]
 *============================================================================*/
double isa18_kpi_overall_health_score(
    const isa18_kpi_counts_t *kpi,
    time_t current_time);

/*============================================================================
 * L3 — Generate KPI Report String
 *
 * Produces a human-readable KPI summary for shift reports.
 * Format includes all EEMUA 191 key metrics.
 *
 * Returns: number of characters written to buffer
 *============================================================================*/
uint32_t isa18_kpi_generate_report(
    const isa18_kpi_counts_t *kpi,
    time_t current_time,
    char *report_buffer,
    uint32_t buffer_size);

/*============================================================================
 * L5 — Rolling 24-Hour Window Update
 *
 * Maintains a sliding window of alarm events for the last 24 hours.
 * Used for calculating daily KPIs. Older entries are aged out.
 *
 * Parameters:
 *   history — the ring buffer of history entries
 *   history_count — pointer to current count (updated)
 *   max_entries — capacity of the history buffer
 *   new_event — the new event to add
 *   current_time — current wall clock time
 *
 * Returns: number of entries aged out
 *============================================================================*/
uint32_t isa18_kpi_rolling_window_update(
    isa18_history_entry_t *history,
    uint32_t *history_count,
    uint32_t max_entries,
    const isa18_history_entry_t *new_event,
    time_t current_time);

/*============================================================================
 * L5 — Calculate Average Operator Response Time
 *
 * Computes the mean time from alarm activation to operator
 * acknowledgement over a set of completed alarm cycles.
 *
 * ISA-18.2 §16.4.5: Response time should be consistent with
 * the time-to-respond specified in rationalization.
 *
 * Formula: avg_rt = sum(ack_time_i - activation_time_i) / n
 *
 * Returns: average response time in seconds, or -1.0 if no data
 *============================================================================*/
double isa18_kpi_calc_avg_response_time(
    const isa18_operator_response_t *responses,
    uint32_t response_count);

/*============================================================================
 * L3 — Calculate Fleeting Alarm Percentage
 *
 * Fleeting alarms (ISA-18.2 §16.7.2) are alarms that activate and
 * return to normal automatically within 30 seconds, without operator
 * action. A high fleeting percentage indicates poorly configured alarms
 * or rapid process oscillations.
 *
 * Returns: percentage [0, 100]
 *============================================================================*/
double isa18_kpi_calc_fleeting_percentage(
    const isa18_kpi_counts_t *kpi,
    time_t current_time);

/*============================================================================
 * L3 — Standing Alarm Detection
 *
 * A standing alarm (ISA-18.2 §16.4.3) is one that has been continuously
 * active for an extended period (typically > 24 hours). Standing alarms
 * indicate either:
 *   - A process condition that cannot be corrected
 *   - An alarm that should be reclassified as an alert
 *   - An alarm that needs a higher priority for resolution
 *
 * Returns: count of standing alarms
 *============================================================================*/
uint32_t isa18_kpi_count_standing_alarms(
    const isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    time_t current_time,
    uint32_t standing_threshold_sec);

#endif /* ALARM_KPI_METRICS_H */