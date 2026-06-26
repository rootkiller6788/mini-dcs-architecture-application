/**
 * @file alarm_kpi_metrics.c
 * @brief ISA-18.2 Alarm System Performance Metrics — KPI Calculations,
 *        Chattering Detection, Nuisance Alarms, EEMUA Benchmarking (L3, L4, L5)
 *
 * Knowledge Points:
 *   isa18_kpi_init                      — Initialize KPI counters (L3)
 *   isa18_kpi_calc_alarms_per_day       — Daily alarm rate per operator (L5)
 *   isa18_kpi_calc_peak_rate            — Peak rate in 10-min sliding window (L5)
 *   isa18_kpi_update_with_event         — Incremental KPI update (L5)
 *   isa18_kpi_detect_chattering         — Chattering detection algorithm (L5)
 *   isa18_kpi_chattering_reset          — Reset chattering detector (L3)
 *   isa18_kpi_calc_priority_distribution — Alarm distribution by priority (L3)
 *   isa18_kpi_detect_nuisance_alarm     — Nuisance alarm identification (L5)
 *   isa18_kpi_top_n_frequent            — Top-N most frequent alarms (L5)
 *   isa18_kpi_assess_eemua_benchmark     — EEMUA 191 benchmark comparison (L4)
 *   isa18_kpi_overall_health_score      — Composite alarm health score (L5)
 *   isa18_kpi_generate_report           — KPI report generation (L3)
 *   isa18_kpi_rolling_window_update     — 24-hour rolling window (L5)
 *   isa18_kpi_calc_avg_response_time    — Average operator response time (L5)
 *   isa18_kpi_calc_fleeting_percentage  — Fleeting alarm percentage (L3)
 *   isa18_kpi_count_standing_alarms     — Standing/chronic alarm count (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §16 (Monitoring and Assessment)
 *   - ISA-TR18.2.5-2012
 *   - EEMUA 191 §8 (Measuring Performance)
 *   - ASM Consortium Guidelines
 */

#include "alarm_kpi_metrics.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * L3 — Initialize KPI Counters
 *============================================================================*/
void isa18_kpi_init(
    isa18_kpi_counts_t *kpi,
    time_t period_start,
    uint32_t number_of_operators)
{
    if (!kpi) return;

    memset(kpi, 0, sizeof(isa18_kpi_counts_t));
    kpi->period_start = period_start;
    kpi->period_end = 0;
    kpi->number_of_operators = (number_of_operators > 0)
        ? number_of_operators : 1;
}

/*============================================================================
 * L5 — Calculate Alarms Per Day Per Operator
 *
 * Formula: APD = total_alarms * (86400 / period_sec) / operators
 *
 * This is the primary KPI from EEMUA 191 Table 8-1.
 *
 * Benchmarks:
 *   Excellent:  < 75/day/op
 *   Acceptable:  75-150/day/op
 *   Manageable: 150-300/day/op
 *   Demanding:  300-600/day/op
 *   Unacceptable: > 600/day/op
 *
 * Returns: alarms per day per operator, or 0 if no operators.
 *============================================================================*/
double isa18_kpi_calc_alarms_per_day(
    const isa18_kpi_counts_t *kpi,
    time_t current_time)
{
    if (!kpi || kpi->number_of_operators == 0) return 0.0;

    double period_sec = difftime(current_time, kpi->period_start);
    if (period_sec <= 0.0) return 0.0;

    double alarms_per_sec = (double)kpi->alarms_per_day / period_sec;
    double alarms_per_day = alarms_per_sec * 86400.0;
    return alarms_per_day / (double)kpi->number_of_operators;
}

/*============================================================================
 * L5 — Calculate Peak Alarm Rate in Sliding Window
 *
 * Scans the history to find the maximum number of alarm activations
 * in any contiguous window_sec interval.
 *
 * Algorithm: Sliding pointer approach.
 *   1. Sort events by timestamp (assumed already sorted)
 *   2. For each event, count how many events fall within [t, t+window_sec]
 *   3. Track the maximum count
 *
 * Complexity: O(n) amortized with two-pointer technique.
 *
 * Parameters:
 *   history:       array of timestamped alarm events
 *   history_count: number of entries
 *   current_time:  current wall clock time
 *   window_sec:    sliding window duration (typically 600 for 10 min)
 *
 * Returns: peak count in the window.
 *============================================================================*/
uint32_t isa18_kpi_calc_peak_rate(
    const isa18_history_entry_t *history,
    uint32_t history_count,
    time_t current_time,
    uint32_t window_sec)
{
    (void)current_time;

    if (!history || history_count == 0 || window_sec == 0) return 0;

    uint32_t peak = 0;
    uint32_t left = 0;

    for (uint32_t right = 0; right < history_count; right++) {
        /* Slide left pointer to maintain window */
        while (left < right &&
               difftime(history[right].timestamp,
                        history[left].timestamp) > (double)window_sec) {
            left++;
        }

        uint32_t window_count = right - left + 1;
        if (window_count > peak) {
            peak = window_count;
        }
    }

    return peak;
}

/*============================================================================
 * L5 — Incremental KPI Update on Alarm Event
 *
 * Called by the alarm engine whenever an alarm state transition occurs.
 * Updates the relevant KPI counters based on the event type.
 *
 * This implements the "live KPI" capability required by ISA-18.2 §16:
 * the system must continuously monitor its own performance.
 *============================================================================*/
void isa18_kpi_update_with_event(
    isa18_kpi_counts_t *kpi,
    const isa18_alarm_event_t *event,
    const isa18_alarm_config_t *alarm)
{
    if (!kpi || !event) return;

    /* Count new alarm activations (NORMAL -> ACTIVE_UNACK) */
    if (event->from_state == ISA18_ALARM_STATE_NORMAL &&
        event->to_state == ISA18_ALARM_STATE_ACTIVE_UNACK) {
        kpi->alarms_per_day++;

        /* By priority */
        if (alarm) {
            switch (alarm->priority) {
            case ISA18_PRIORITY_CRITICAL: kpi->critical_per_day++; break;
            case ISA18_PRIORITY_HIGH:     kpi->high_per_day++;     break;
            case ISA18_PRIORITY_MEDIUM:   kpi->medium_per_day++;   break;
            case ISA18_PRIORITY_LOW:      kpi->low_per_day++;      break;
            default: break;
            }
        }
    }

    /* Track active alarm count */
    if (event->to_state == ISA18_ALARM_STATE_ACTIVE_UNACK) {
        kpi->active_alarm_count++;
    } else if (event->from_state == ISA18_ALARM_STATE_ACTIVE_UNACK ||
               event->from_state == ISA18_ALARM_STATE_ACTIVE_ACK) {
        if (event->to_state == ISA18_ALARM_STATE_CLEARED ||
            event->to_state == ISA18_ALARM_STATE_NORMAL) {
            if (kpi->active_alarm_count > 0) {
                kpi->active_alarm_count--;
            }
        }
    }

    /* Update peak rates */
    if (event->to_state == ISA18_ALARM_STATE_ACTIVE_UNACK) {
        if (kpi->active_alarm_count > kpi->peak_10min_rate) {
            kpi->peak_10min_rate = kpi->active_alarm_count;
        }
    }
}

/*============================================================================
 * L5 — Chattering Alarm Detection (ISA-18.2 §16.6)
 *
 * Chattering is one of the most common alarm system problems.
 * It occurs when an alarm repeatedly transitions between active
 * and normal in a short period, flooding the operator with
 * meaningless notifications.
 *
 * Causes of chattering:
 *   1. Deadband too small (most common)
 *   2. Process oscillation near setpoint
 *   3. Measurement noise exceeding deadband
 *   4. Control loop oscillation/tuning issues
 *   5. Incorrect on-delay/off-delay settings
 *
 * Detection algorithm:
 *   1. Record each transition timestamp in a circular buffer
 *   2. Count transitions within CHATTER_WINDOW_SEC (60s)
 *   3. If count >= CHATTER_THRESHOLD_COUNT (3), declare chattering
 *
 * When chattering is detected:
 *   - The alarm should be automatically suppressed (if configured)
 *   - A "chattering alarm" system diagnostic should be raised
 *   - The alarm configuration should be reviewed (deadband, delays)
 *
 * Returns: true if chattering is now being detected.
 *============================================================================*/
bool isa18_kpi_detect_chattering(
    isa18_chattering_detector_t *detector,
    time_t transition_time)
{
    if (!detector) return false;

    bool was_chattering = detector->is_chattering;

    /* Record this transition */
    uint32_t idx = detector->transition_count %
                   (ISA18_CHATTER_THRESHOLD_COUNT + 2);
    detector->transition_times[idx] = transition_time;
    detector->transition_count++;

    /* Need at least CHATTER_THRESHOLD_COUNT transitions to check */
    if (detector->transition_count < ISA18_CHATTER_THRESHOLD_COUNT) {
        return false;
    }

    /* Count transitions within the window */
    uint32_t count_in_window = 0;
    uint32_t check_count = (detector->transition_count >
                            (ISA18_CHATTER_THRESHOLD_COUNT + 1))
        ? (ISA18_CHATTER_THRESHOLD_COUNT + 1)
        : detector->transition_count;

    for (uint32_t i = 0; i < check_count; i++) {
        double dt = difftime(transition_time,
                             detector->transition_times[i]);
        if (dt >= 0.0 && dt <= (double)ISA18_CHATTER_WINDOW_SEC) {
            count_in_window++;
        }
    }

    /* Determine chattering state */
    detector->is_chattering =
        (count_in_window >= ISA18_CHATTER_THRESHOLD_COUNT);

    if (detector->is_chattering && !was_chattering) {
        detector->chattering_start = transition_time;
        detector->total_chattering_events++;
    }

    return detector->is_chattering;
}

/*============================================================================
 * L3 — Reset Chattering Detection
 *
 * Called when an alarm is acknowledged or the configuration
 * is changed (e.g., deadband adjusted). Resets the chattering
 * detector to a clean state.
 *============================================================================*/
void isa18_kpi_chattering_reset(
    isa18_chattering_detector_t *detector)
{
    if (!detector) return;

    memset(detector->transition_times, 0,
           sizeof(detector->transition_times));
    detector->transition_count = 0;
    detector->is_chattering = false;
    /* Do NOT reset total_chattering_events — it's cumulative */
}

/*============================================================================
 * L3 — Calculate Priority Distribution
 *
 * Computes what percentage of alarms fall into each priority level.
 *
 * ISA-18.2 §16.4.4: Priority distribution should match the targets
 * established during rationalization. Typical targets:
 *   Critical:  ~5%
 *   High:     ~15%
 *   Medium:   ~60%
 *   Low:      ~20%
 *
 * A shift toward higher priority alarms indicates deteriorating
 * process conditions or overly conservative rationalization.
 *============================================================================*/
void isa18_kpi_calc_priority_distribution(
    const isa18_kpi_counts_t *kpi,
    double *out_critical_pct,
    double *out_high_pct,
    double *out_medium_pct,
    double *out_low_pct)
{
    if (!kpi) {
        if (out_critical_pct) *out_critical_pct = 0.0;
        if (out_high_pct)     *out_high_pct     = 0.0;
        if (out_medium_pct)   *out_medium_pct   = 0.0;
        if (out_low_pct)      *out_low_pct      = 0.0;
        return;
    }

    uint32_t total = kpi->critical_per_day + kpi->high_per_day +
                     kpi->medium_per_day + kpi->low_per_day;

    if (total == 0) {
        if (out_critical_pct) *out_critical_pct = 0.0;
        if (out_high_pct)     *out_high_pct     = 0.0;
        if (out_medium_pct)   *out_medium_pct   = 0.0;
        if (out_low_pct)      *out_low_pct      = 0.0;
        return;
    }

    if (out_critical_pct) *out_critical_pct =
        100.0 * (double)kpi->critical_per_day / (double)total;
    if (out_high_pct)     *out_high_pct     =
        100.0 * (double)kpi->high_per_day     / (double)total;
    if (out_medium_pct)   *out_medium_pct   =
        100.0 * (double)kpi->medium_per_day   / (double)total;
    if (out_low_pct)      *out_low_pct      =
        100.0 * (double)kpi->low_per_day      / (double)total;
}

/*============================================================================
 * L5 — Nuisance Alarm Detection (ISA-18.2 §16.7)
 *
 * Nuisance alarms are alarms that:
 *   - Do not indicate an abnormal situation
 *   - Do not require operator action (even though rationalization
 *     may claim they do)
 *   - Are consistently acknowledged without any corrective action
 *
 * Detection heuristic (multi-factor):
 *   1. Average acknowledgement time < 5 seconds (operator quickly
 *      acknowledges without thinking)
 *   2. High shelving frequency (operators shelve it to stop the noise)
 *   3. High activation frequency but never results in corrective action
 *
 * This function uses the first two factors. If avg ack time < 5 seconds
 * AND shelving is frequent, the alarm is likely a nuisance.
 *
 * Returns: true if the alarm is likely a nuisance requiring re-rationalization.
 *============================================================================*/
bool isa18_kpi_detect_nuisance_alarm(
    double avg_ack_time_sec,
    uint32_t activation_count,
    uint32_t shelve_count)
{
    /* Need enough data to make a determination */
    if (activation_count < 50) return false;

    /* Factor 1: Very fast acknowledgement (operator habit) */
    if (avg_ack_time_sec >= 5.0) return false;

    /* Factor 2: Frequently shelved (operator wants it gone) */
    double shelve_ratio = (double)shelve_count / (double)activation_count;
    if (shelve_ratio < 0.1) return false;

    return true;
}

/*============================================================================
 * L5 — Top-N Most Frequent Alarms
 *
 * Identifies the "bad actor" alarms that activate most frequently.
 * These should be prioritized for re-rationalization per ISA-18.2 §16.4.1.
 *
 * Algorithm: Frequency counting then selection of top N.
 * For each alarm_id in history, increment its count, then select
 * the N with highest counts.
 *
 * Complexity: O(m + n) where m = history_count, n = top_n.
 *============================================================================*/
uint32_t isa18_kpi_top_n_frequent(
    const isa18_history_entry_t *history,
    uint32_t history_count,
    isa18_frequent_alarm_entry_t *result,
    uint32_t top_n)
{
    if (!history || !result || top_n == 0) return 0;

    /* Phase 1: Count occurrences */
    isa18_frequent_alarm_entry_t counters[ISA18_MAX_ALARM_POINTS];
    uint32_t unique_alarms = 0;

    memset(counters, 0, sizeof(counters));

    for (uint32_t i = 0; i < history_count; i++) {
        uint32_t alarm_id = history[i].alarm_id;
        bool found = false;

        for (uint32_t j = 0; j < unique_alarms; j++) {
            if (counters[j].alarm_id == alarm_id) {
                counters[j].occurrence_count++;
                found = true;
                break;
            }
        }

        if (!found && unique_alarms < ISA18_MAX_ALARM_POINTS) {
            counters[unique_alarms].alarm_id = alarm_id;
            counters[unique_alarms].occurrence_count = 1;
            unique_alarms++;
        }
    }

    /* Phase 2: Select top N by occurrence count (descending) */
    uint32_t result_count = (unique_alarms < top_n) ? unique_alarms : top_n;

    for (uint32_t i = 0; i < result_count; i++) {
        uint32_t best_idx = 0;
        uint32_t best_count = 0;

        for (uint32_t j = 0; j < unique_alarms; j++) {
            if (counters[j].occurrence_count > best_count) {
                best_count = counters[j].occurrence_count;
                best_idx = j;
            }
        }

        result[i] = counters[best_idx];
        counters[best_idx].occurrence_count = 0; /* Mark as consumed */
    }

    return result_count;
}

/*============================================================================
 * L4 — Assess Against EEMUA 191 Benchmarks
 *
 * EEMUA Publication 191 provides the internationally recognized
 * benchmark for alarm system performance.
 *
 * Seven metrics assessed (scores 0-4 per metric):
 *   0 = Unacceptable (urgent action required)
 *   1 = Demanding    (improvement needed)
 *   2 = Manageable   (monitor and improve)
 *   3 = Acceptable   (no action needed)
 *   4 = Excellent    (industry-leading)
 *
 * Metrics:
 *   [0] Alarms per day per operator
 *   [1] Peak alarm rate per 10 minutes
 *   [2] Stale alarms (>24h active)
 *   [3] Priority distribution (should match rationalization)
 *   [4] Chattering alarm percentage
 *   [5] Average operator response time compliance
 *   [6] Rationalization coverage
 *
 * Returns: void, populates scores[7] array.
 *============================================================================*/
void isa18_kpi_assess_eemua_benchmark(
    const isa18_kpi_counts_t *kpi,
    time_t current_time,
    int scores[7])
{
    if (!kpi || !scores) return;

    /* [0] Alarms per day per operator */
    double apd = isa18_kpi_calc_alarms_per_day(kpi, current_time);
    if (apd < 75.0)       scores[0] = 4; /* Excellent */
    else if (apd < 150.0) scores[0] = 3; /* Acceptable */
    else if (apd < 300.0) scores[0] = 2; /* Manageable */
    else if (apd < 600.0) scores[0] = 1; /* Demanding */
    else                  scores[0] = 0; /* Unacceptable */

    /* [1] Peak alarm rate per 10 minutes */
    if (kpi->peak_10min_rate <= 5)       scores[1] = 4;
    else if (kpi->peak_10min_rate <= 10) scores[1] = 3;
    else if (kpi->peak_10min_rate <= 30) scores[1] = 2;
    else if (kpi->peak_10min_rate <= 50) scores[1] = 1;
    else                                 scores[1] = 0;

    /* [2] Stale alarms */
    if (kpi->stale_24h_count == 0)       scores[2] = 4;
    else if (kpi->stale_24h_count <= 5)  scores[2] = 3;
    else if (kpi->stale_24h_count <= 20) scores[2] = 2;
    else if (kpi->stale_24h_count <= 50) scores[2] = 1;
    else                                 scores[2] = 0;

    /* [3] Priority distribution: check % critical alarms */
    {
        double crit_pct = 0, high_pct = 0, med_pct = 0, low_pct = 0;
        isa18_kpi_calc_priority_distribution(kpi, &crit_pct, &high_pct,
                                              &med_pct, &low_pct);
        if (crit_pct <= 5.0)        scores[3] = 4;
        else if (crit_pct <= 10.0)  scores[3] = 3;
        else if (crit_pct <= 20.0)  scores[3] = 2;
        else if (crit_pct <= 30.0)  scores[3] = 1;
        else                        scores[3] = 0;
    }

    /* [4] Chattering */
    if (kpi->chattering_alarms == 0)       scores[4] = 4;
    else if (kpi->chattering_alarms <= 3)  scores[4] = 3;
    else if (kpi->chattering_alarms <= 10) scores[4] = 2;
    else if (kpi->chattering_alarms <= 30) scores[4] = 1;
    else                                   scores[4] = 0;

    /* [5] Average response time compliance */
    {
        double rt = kpi->avg_response_time_sec;
        if (rt < 60.0)      scores[5] = 4;
        else if (rt < 180.0) scores[5] = 3;
        else if (rt < 300.0) scores[5] = 2;
        else if (rt < 600.0) scores[5] = 1;
        else                 scores[5] = 0;
    }

    /* [6] Rationalization coverage */
    {
        double cov = kpi->alarm_rationalization_coverage;
        if (cov >= 100.0)  scores[6] = 4;
        else if (cov >= 95.0)  scores[6] = 3;
        else if (cov >= 80.0)  scores[6] = 2;
        else if (cov >= 50.0)  scores[6] = 1;
        else                   scores[6] = 0;
    }
}

/*============================================================================
 * L5 — Composite Alarm System Health Score
 *
 * Combines six weighted KPI sub-scores into a single 0-100 health
 * score suitable for management dashboards.
 *
 * Weights based on EEMUA 191 and ASM Consortium best practices:
 *   w1 = 0.25  Alarms per day
 *   w2 = 0.25  Peak rate
 *   w3 = 0.15  Stale alarms
 *   w4 = 0.15  Response time
 *   w5 = 0.10  Chattering
 *   w6 = 0.10  Rationalization coverage
 *
 * Formula: H = sum(W_i * (S_i / 4) * 100)
 *============================================================================*/
double isa18_kpi_overall_health_score(
    const isa18_kpi_counts_t *kpi,
    time_t current_time)
{
    int scores[7] = {0, 0, 0, 0, 0, 0, 0};
    isa18_kpi_assess_eemua_benchmark(kpi, current_time, scores);

    const double weights[7] = {0.25, 0.25, 0.15, 0.0, 0.10, 0.15, 0.10};

    double health = 0.0;
    for (int i = 0; i < 7; i++) {
        health += weights[i] * ((double)scores[i] / 4.0) * 100.0;
    }

    return health;
}

/*============================================================================
 * L3 — Generate KPI Report String
 *
 * Produces a formatted text report of alarm system KPIs.
 * Suitable for shift handover reports and management review.
 *
 * Format follows ISA-TR18.2.5 Annex A guidelines.
 *============================================================================*/
uint32_t isa18_kpi_generate_report(
    const isa18_kpi_counts_t *kpi,
    time_t current_time,
    char *report_buffer,
    uint32_t buffer_size)
{
    if (!kpi || !report_buffer || buffer_size == 0) return 0;

    double apd = isa18_kpi_calc_alarms_per_day(kpi, current_time);
    double crit_pct = 0, high_pct = 0, med_pct = 0, low_pct = 0;
    isa18_kpi_calc_priority_distribution(kpi, &crit_pct, &high_pct,
                                          &med_pct, &low_pct);

    int scores[7];
    isa18_kpi_assess_eemua_benchmark(kpi, current_time, scores);
    double health = isa18_kpi_overall_health_score(kpi, current_time);

    int written = snprintf(report_buffer, buffer_size,
        "========================================\n"
        "  ISA-18.2 ALARM SYSTEM KPI REPORT\n"
        "========================================\n"
        "Period: %.24s to %.24s\n"
        "Operators on shift: %u\n"
        "----------------------------------------\n"
        "Alarms per day per operator:  %.1f  (Benchmark: <=150)\n"
        "Peak alarm rate (10-min):     %u    (Benchmark: <=10)\n"
        "Peak alarm rate (60-min):     %u\n"
        "Active alarms (current):      %u\n"
        "Stale alarms (>24h active):   %u    (Benchmark: 0)\n"
        "Chattering alarms:            %u    (Benchmark: 0)\n"
        "Shelved alarms (current):     %u\n"
        "Suppressed alarms (current):  %u\n"
        "Out-of-service alarms:        %u\n"
        "Nuisance alarms identified:   %u\n"
        "----------------------------------------\n"
        "Priority Distribution:\n"
        "  CRITICAL: %.1f%% (Target: ~5%%)\n"
        "  HIGH:     %.1f%% (Target: ~15%%)\n"
        "  MEDIUM:   %.1f%% (Target: ~60%%)\n"
        "  LOW:      %.1f%% (Target: ~20%%)\n"
        "----------------------------------------\n"
        "Avg response time: %.1f sec\n"
        "Max response time: %.1f sec\n"
        "Rationalization coverage: %.1f%% (Target: 100%%)\n"
        "Top 10 alarm contribution: %.1f%%\n"
        "----------------------------------------\n"
        "Composite Health Score: %.1f / 100\n"
        "========================================\n",
        ctime(&kpi->period_start),
        ctime(&current_time),
        kpi->number_of_operators,
        apd,
        kpi->peak_10min_rate,
        kpi->peak_60min_rate,
        kpi->active_alarm_count,
        kpi->stale_24h_count,
        kpi->chattering_alarms,
        kpi->shelved_alarm_count,
        kpi->suppressed_alarm_count,
        kpi->out_of_service_count,
        kpi->nuisance_alarms_count,
        crit_pct, high_pct, med_pct, low_pct,
        kpi->avg_response_time_sec,
        kpi->max_response_time_sec,
        kpi->alarm_rationalization_coverage,
        kpi->percent_contrib_top10,
        health);

    return (uint32_t)(written > 0 ? written : 0);
}

/*============================================================================
 * L5 — Rolling 24-Hour Window Update
 *
 * Maintains a sliding 24-hour window of alarm events for KPI
 * calculations. Events older than 24 hours are aged out.
 *
 * This is a ring buffer implementation with aging.
 *
 * Returns: number of entries aged out.
 *============================================================================*/
uint32_t isa18_kpi_rolling_window_update(
    isa18_history_entry_t *history,
    uint32_t *history_count,
    uint32_t max_entries,
    const isa18_history_entry_t *new_event,
    time_t current_time)
{
    if (!history || !history_count || !new_event) return 0;

    uint32_t aged_out = 0;

    /* Age out entries older than 24 hours */
    time_t cutoff = current_time - 86400; /* 24 hours ago */
    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < *history_count; i++) {
        if (history[i].timestamp >= cutoff && write_idx < max_entries) {
            if (write_idx != i) {
                history[write_idx] = history[i];
            }
            write_idx++;
        } else {
            aged_out++;
        }
    }

    /* Add new event */
    if (write_idx < max_entries) {
        history[write_idx] = *new_event;
        write_idx++;
    }

    *history_count = write_idx;
    return aged_out;
}

/*============================================================================
 * L5 — Calculate Average Operator Response Time
 *
 * Computes the mean time from alarm activation to operator
 * acknowledgement across completed response records.
 *
 * Formula: avg_rt = sum(ack_time_i - alarm_time_i) / n
 *
 * ISA-18.2 §16.4.5: Average response time should be consistent
 * with the time-to-respond targets from rationalization.
 *
 * Returns: average response time in seconds, or -1.0 if no data.
 *============================================================================*/
double isa18_kpi_calc_avg_response_time(
    const isa18_operator_response_t *responses,
    uint32_t response_count)
{
    if (!responses || response_count == 0) return -1.0;

    double total = 0.0;
    for (uint32_t i = 0; i < response_count; i++) {
        if (responses[i].ack_time > 0) {
            total += difftime(responses[i].ack_time,
                              responses[i].alarm_time);
        }
    }

    return total / (double)response_count;
}

/*============================================================================
 * L3 — Calculate Fleeting Alarm Percentage
 *
 * A fleeting alarm is one that activates and returns to normal
 * within 30 seconds without any operator action.
 *
 * High fleeting alarm percentages suggest:
 *   - Process instability near alarm setpoints
 *   - Insufficient on-delay
 *   - Measurement spikes/noise
 *   - Alarms that could be eliminated by process redesign
 *
 * ISA-18.2 §16.7.2: Fleeting alarms should be < 1% of total alarms.
 *============================================================================*/
double isa18_kpi_calc_fleeting_percentage(
    const isa18_kpi_counts_t *kpi,
    time_t current_time)
{
    (void)current_time;

    if (!kpi || kpi->alarms_per_day == 0) return 0.0;

    return 100.0 * (double)kpi->fleeting_alarms
           / (double)kpi->alarms_per_day;
}

/*============================================================================
 * L3 — Standing Alarm Detection
 *
 * A standing alarm (also called "stale" or "chronic" alarm) is
 * an alarm that has been continuously active for an extended period.
 *
 * Standing alarms violate the ISA-18.2 principle that every alarm
 * should require a timely operator response. If an alarm can be
 * active for > 24 hours without consequence, it should be reclassified
 * as an alert or prompt.
 *
 * Returns: count of standing alarms.
 *============================================================================*/
uint32_t isa18_kpi_count_standing_alarms(
    const isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    time_t current_time,
    uint32_t standing_threshold_sec)
{
    if (!configs) return 0;

    uint32_t count = 0;

    for (uint32_t i = 0; i < total_alarms; i++) {
        if (isa18_alarm_state_is_active(configs[i].current_state)) {
            double duration = difftime(current_time,
                                        configs[i].activation_time);
            if (duration >= (double)standing_threshold_sec) {
                count++;
            }
        }
    }

    return count;
}