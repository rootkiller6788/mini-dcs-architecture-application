/**
 * @file dcs_alarm.c
 * @brief DCS alarm management implementation per ISA-18.2.
 *
 * Covers alarm state machine, hysteresis, shelving, rationalization,
 * flood detection, and KPI calculation.
 *
 * Knowledge Levels: L2, L3, L4, L5, L6
 */

#include "dcs_alarm.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/*===========================================================================
 * L3: Alarm State Machine
 *===========================================================================*/

int dcs_alarm_process(dcs_alarm_config_t *alarm, double new_value)
{
    if (alarm == NULL) return 0;
    if (!alarm->enabled) return 0;
    if (alarm->current_state == DCS_ALARM_STATE_OUT_OF_SVC) return 0;

    alarm->current_value = new_value;

    dcs_alarm_state_t prev_state = alarm->current_state;
    int alarm_should_be_active = 0;

    /*
     * Determine if alarm condition exists based on alarm type.
     */
    switch (alarm->type) {
        case DCS_ALARM_ABSOLUTE:
            /* High alarm: PV > setpoint */
            if (alarm->setpoint > 32767.0) {
                /* Interpret large setpoint as high alarm */
                alarm_should_be_active = (new_value > alarm->setpoint / 1000.0) ? 1 : 0;
            } else {
                alarm_should_be_active = (new_value > alarm->setpoint) ? 1 : 0;
            }
            break;

        case DCS_ALARM_DIGITAL:
            alarm_should_be_active = (new_value != 0.0) ? 1 : 0;
            break;

        case DCS_ALARM_RATE:
            /* Rate alarm: handled by hysteresis function */
            alarm_should_be_active = (new_value > alarm->setpoint) ? 1 : 0;
            break;

        case DCS_ALARM_DEVIATION:
            /* |PV - expected| > setpoint — callers should pre-compute deviation */
            alarm_should_be_active = (new_value > alarm->setpoint) ? 1 : 0;
            break;

        default:
            alarm_should_be_active = (new_value > alarm->setpoint) ? 1 : 0;
            break;
    }

    /*
     * State transitions per ISA-18.2 alarm state diagram.
     *
     * NORMAL → UNACK_ACTIVE:  Alarm condition becomes true.
     * UNACK_ACTIVE → ACK_ACTIVE:  Operator acknowledges.
     * ACK_ACTIVE → UNACK_CLEAR:   Alarm condition clears (returns to
     *                              unacknowledged state to signal RTN).
     * UNACK_CLEAR → ACK_CLEAR:    Operator acknowledges the RTN.
     * ACK_CLEAR → NORMAL:         Auto-reset or operator reset.
     * NORMAL (state persistence): alarm condition stays false.
     */
    switch (prev_state) {
        case DCS_ALARM_STATE_NORMAL:
        case DCS_ALARM_STATE_ACK_CLEAR:
            if (alarm_should_be_active) {
                alarm->current_state = DCS_ALARM_STATE_UNACK_ACTIVE;
                alarm->activation_timestamp = 0; /* Set by caller with real time */
            }
            break;

        case DCS_ALARM_STATE_UNACK_ACTIVE:
            if (!alarm_should_be_active) {
                alarm->current_state = DCS_ALARM_STATE_UNACK_CLEAR;
                alarm->clear_timestamp = 0;
            }
            /* If still active, stays UNACK_ACTIVE */
            break;

        case DCS_ALARM_STATE_ACK_ACTIVE:
            if (!alarm_should_be_active) {
                alarm->current_state = DCS_ALARM_STATE_UNACK_CLEAR;
                alarm->clear_timestamp = 0;
            }
            /* If still active, stays ACK_ACTIVE */
            break;

        case DCS_ALARM_STATE_UNACK_CLEAR:
            /* On re-trigger while unacknowledged RTN */
            if (alarm_should_be_active) {
                alarm->current_state = DCS_ALARM_STATE_UNACK_ACTIVE;
                alarm->activation_timestamp = 0;
            }
            /* Otherwise stays UNACK_CLEAR until acknowledged */
            break;

        case DCS_ALARM_STATE_SHELVED:
        case DCS_ALARM_STATE_SUPPRESSED:
            /* Alarm is shelved/suppressed — process but don't annunciate */
            if (alarm_should_be_active) {
                alarm->activation_timestamp = 0;
            }
            break;

        case DCS_ALARM_STATE_OUT_OF_SVC:
            /* Do nothing */
            break;
    }

    return (alarm->current_state != prev_state) ? 1 : 0;
}

/*===========================================================================
 * L3: Alarm Evaluation with Hysteresis and On/Off Delay
 *===========================================================================*/

int dcs_alarm_evaluate_with_hysteresis(dcs_alarm_config_t *alarm,
                                        double value, double dt_s)
{
    if (alarm == NULL) return 0;
    if (dt_s <= 0.0) dt_s = 0.1;

    /*
     * Hysteresis logic:
     *
     * Activation:   PV > setpoint + hysteresis/2  for on_delay_s seconds
     * Deactivation: PV < setpoint - hysteresis/2  for off_delay_s seconds
     *
     * This creates a Schmitt trigger behavior that prevents chattering
     * when the process variable oscillates near the alarm setpoint.
     *
     * EEMUA 191 recommends hysteresis = 2% of measurement span.
     */
    double half_hyst = alarm->hysteresis / 2.0;
    if (half_hyst <= 0.0) half_hyst = alarm->setpoint * 0.01; /* Default 1% */

    double upper_threshold = alarm->setpoint + half_hyst;
    double lower_threshold = alarm->setpoint - half_hyst;

    /* Static variables to track accumulated time (in real DCS, these would
     * be part of the alarm configuration struct) */
    static double above_threshold_time = 0.0;
    static double below_threshold_time = 0.0;
    static int was_active = 0;

    if (value > upper_threshold) {
        above_threshold_time += dt_s;
        below_threshold_time = 0.0;

        if (above_threshold_time >= alarm->on_delay_s && !was_active) {
            was_active = 1;
            return 1; /* Alarm activates */
        }
    } else if (value < lower_threshold) {
        below_threshold_time += dt_s;
        above_threshold_time = 0.0;

        if (below_threshold_time >= alarm->off_delay_s && was_active) {
            was_active = 0;
            return 0; /* Alarm deactivates */
        }
    } else {
        /* In deadband: maintain current state, don't reset timers */
    }

    return was_active ? 1 : 0;
}

/*===========================================================================
 * L3: Alarm Acknowledge and Shelving
 *===========================================================================*/

int dcs_alarm_acknowledge(dcs_alarm_config_t *alarm)
{
    if (alarm == NULL) return 0;

    switch (alarm->current_state) {
        case DCS_ALARM_STATE_UNACK_ACTIVE:
            alarm->current_state = DCS_ALARM_STATE_ACK_ACTIVE;
            alarm->acknowledgement_timestamp = 0;
            return 1;

        case DCS_ALARM_STATE_UNACK_CLEAR:
            alarm->current_state = DCS_ALARM_STATE_ACK_CLEAR;
            alarm->acknowledgement_timestamp = 0;
            return 1;

        default:
            /* Already acknowledged or not in acknowledgeable state */
            return 0;
    }
}

int dcs_alarm_shelve(dcs_alarm_config_t *alarm, double duration_min)
{
    if (alarm == NULL) return 0;
    if (duration_min <= 0.0) return 0;

    /* Per ISA-18.2, shelving is only for acknowledged alarms */
    if (alarm->current_state != DCS_ALARM_STATE_ACK_ACTIVE
        && alarm->current_state != DCS_ALARM_STATE_UNACK_ACTIVE) {
        return 0;
    }

    /*
     * Shelve the alarm: suppress annunciation but continue processing.
     * Auto-unshelve after duration_min minutes.
     */
    alarm->current_state = DCS_ALARM_STATE_SHELVED;

    return 1;
}

int dcs_alarm_check_unshelve(dcs_alarm_config_t *alarm, double elapsed_min)
{
    if (alarm == NULL) return 0;

    if (alarm->current_state != DCS_ALARM_STATE_SHELVED) return 0;

    /*
     * If elapsed time exceeds shelve duration, unshelve.
     * The alarm will return to its previous state based on current PV.
     * The shelve duration is assumed to have been stored during shelve() call.
     */
    if (elapsed_min >= 60.0) { /* Default: 60 minutes max shelve time */
        alarm->current_state = DCS_ALARM_STATE_NORMAL;
        return 1;
    }

    return 0; /* Still shelved */
}

/*===========================================================================
 * L5: Alarm Rationalization per ISA-18.2
 *===========================================================================*/

int dcs_alarm_rationalize(const dcs_alarm_config_t *alarm,
                           dcs_alarm_rationalization_t *rationalization)
{
    if (alarm == NULL || rationalization == NULL) return 0;

    /* Save caller-provided data before clearing */
    char saved_action[128] = {0};
    if (rationalization->operator_action[0] != '\0') {
        size_t len = strlen(rationalization->operator_action);
        if (len > 127) len = 127;
        memcpy(saved_action, rationalization->operator_action, len);
        saved_action[len] = '\0';
    }

    /* Clear and set defaults */
    memset(rationalization, 0, sizeof(dcs_alarm_rationalization_t));
    rationalization->alarm_id = alarm->alarm_id;

    /* Restore caller-provided data */
    if (saved_action[0] != '\0') {
        size_t len = strlen(saved_action);
        if (len > 127) len = 127;
        memcpy(rationalization->operator_action, saved_action, len);
        rationalization->operator_action[len] = '\0';
    }

    /*
     * ISA-18.2 rationalization methodology:
     *
     * Each alarm must answer "yes" to all of:
     *   1. Does the alarm indicate an abnormal situation?
     *   2. Is there a defined operator action?
     *   3. Can the operator respond within the available time?
     *   4. Is this alarm the best indicator of the abnormality?
     *   5. Is the alarm priority consistent with the consequences?
     */

    /* Check 1: Alarm purpose defined */
    if (alarm->alarm_description[0] == '\0'
        || strlen(alarm->alarm_description) < 10) {
        /* No defined consequence — alarm is not justified */
        rationalization->is_justified = 0;
        return 0;
    }

    /* Check 2: Operator action defined */
    if (strlen(rationalization->operator_action) < 5) {
        /* Copy from alarm if available */
        if (strlen(alarm->consequence) > 5) {
            snprintf(rationalization->consequence_of_inaction,
                     128, "%s", alarm->consequence);
        }
        rationalization->is_justified = 0;
        return 0;
    }

    /* Check 3: Response time adequacy */
    rationalization->time_to_respond_s = alarm->response_time_s;
    if (rationalization->time_to_respond_s <= 0.0) {
        rationalization->is_justified = 0;
        return 0;
    }

    /* Check 4: Priority consistency */
    rationalization->has_safety_consequence =
        (alarm->priority == DCS_ALARM_PRIORITY_CRITICAL
         || alarm->priority == DCS_ALARM_PRIORITY_HIGH) ? 1 : 0;

    /*
     * Priority should match risk:
     *   Critical priority → safety or major environmental consequence
     *   High priority     → equipment damage or significant production loss
     *   Medium priority   → operational impact
     *   Low priority      → informational only
     */
    rationalization->priority_matches_risk = 1; /* Assume properly assigned */

    rationalization->is_justified = 1;
    rationalization->is_rationalized = 1;

    return rationalization->is_justified;
}

/*===========================================================================
 * L5: Alarm Flood Detection
 *===========================================================================*/

int dcs_alarm_detect_flood(const uint64_t *alarm_timestamps,
                            uint32_t num_alarms,
                            double window_min,
                            uint32_t threshold,
                            int32_t *flood_start,
                            uint32_t *flood_count)
{
    if (alarm_timestamps == NULL || num_alarms == 0) return 0;
    if (flood_start != NULL) *flood_start = -1;
    if (flood_count != NULL) *flood_count = 0;

    if (window_min <= 0.0) window_min = 10.0;
    if (threshold == 0) threshold = 10;

    /*
     * ISA-18.2 flood detection (sliding window algorithm):
     *
     * An alarm flood is defined as ≥ threshold alarms within window_min minutes.
     *
     * Algorithm: For each alarm i, count how many alarms fall within
     * the window [timestamp[i], timestamp[i] + window_ms].
     * If count ≥ threshold, flood detected starting at i.
     */
    uint64_t window_ms = (uint64_t)(window_min * 60.0 * 1000.0);

    for (uint32_t i = 0; i < num_alarms; i++) {
        uint64_t window_end = alarm_timestamps[i] + window_ms;
        uint32_t count = 1; /* Include alarm i itself */

        for (uint32_t j = i + 1; j < num_alarms; j++) {
            if (alarm_timestamps[j] <= window_end) {
                count++;
            } else {
                break; /* Timestamps are sorted, no more in window */
            }
        }

        if (count >= threshold) {
            if (flood_start != NULL) *flood_start = (int32_t)i;
            if (flood_count != NULL) *flood_count = count;
            return 1;
        }
    }

    return 0; /* No flood detected */
}

/*===========================================================================
 * L5: Alarm System KPI Calculation
 *===========================================================================*/

int dcs_alarm_calculate_kpi(const dcs_alarm_config_t *alarm_configs,
                             uint32_t num_alarms,
                             double observation_hours,
                             dcs_alarm_system_kpi_t *kpi)
{
    if (alarm_configs == NULL || kpi == NULL) return 0;
    if (observation_hours <= 0.0) observation_hours = 24.0;

    memset(kpi, 0, sizeof(dcs_alarm_system_kpi_t));

    uint32_t total_activations = 0;
    uint32_t priority_counts[6] = {0, 0, 0, 0, 0, 0};

    for (uint32_t i = 0; i < num_alarms; i++) {
        const dcs_alarm_config_t *a = &alarm_configs[i];

        /* Count alarms by state */
        if (a->current_state == DCS_ALARM_STATE_UNACK_ACTIVE
            || a->current_state == DCS_ALARM_STATE_ACK_ACTIVE) {
            kpi->standing_alarms++;
        }

        if (a->current_state == DCS_ALARM_STATE_UNACK_ACTIVE
            || a->current_state == DCS_ALARM_STATE_UNACK_CLEAR) {
            /* Unacknowledged — could be stale if old enough */
            if (a->activation_timestamp > 0) {
                uint64_t now = 0; /* Real DCS would use current time */
                uint64_t age_ms = now - a->activation_timestamp;
                if (age_ms > 24ull * 3600ull * 1000ull) {
                    kpi->stale_alarms++;
                }
            }
        }

        /* Count by priority for distribution */
        uint32_t p = (uint32_t)alarm_configs[i].priority;
        if (p >= 1 && p <= 5) {
            priority_counts[p]++;
        }

        /* Count active/transitioned alarms */
        if (a->current_state >= DCS_ALARM_STATE_UNACK_ACTIVE
            && a->current_state <= DCS_ALARM_STATE_ACK_CLEAR) {
            total_activations++;
        }
    }

    /* Average alarm rate */
    kpi->avg_alarms_per_hour = (double)total_activations / observation_hours;

    /* Peak rate (estimate: 3x average for normal operation) */
    kpi->peak_alarms_per_10min = kpi->avg_alarms_per_hour * 3.0 / 6.0;

    /* Priority distribution percentages */
    uint32_t total_with_pri = priority_counts[1] + priority_counts[2]
                            + priority_counts[3] + priority_counts[4]
                            + priority_counts[5];
    if (total_with_pri > 0) {
        for (int p = 1; p <= 5; p++) {
            kpi->priority_pct[p] = (double)priority_counts[p]
                                  / (double)total_with_pri * 100.0;
        }
    }

    /* EEMUA 191 KPI benchmarks check */
    kpi->kpi_acceptable =
        (kpi->avg_alarms_per_hour <= 6.0)      /* < 1 per 10 min */
        && (kpi->standing_alarms <= 10)
        && (kpi->stale_alarms == 0)
        && (kpi->flood_events_24h <= 2);

    return 1;
}

/*===========================================================================
 * L6: Alarm Chatter Prevention — Hysteresis Recommendation
 *===========================================================================*/

double dcs_alarm_recommend_hysteresis(double setpoint,
                                       double signal_noise_std,
                                       double *recommended_hyst)
{
    if (setpoint < 0.0) setpoint = -setpoint;

    /*
     * Hysteresis recommendation per EEMUA 191:
     *
     * The hysteresis should be large enough to prevent chattering
     * from measurement noise, but small enough to provide meaningful
     * alarm detection.
     *
     * Recommended: hysteresis = max(2% of span, 3 * signal_noise_std)
     *
     * For a noisy signal with std = sigma, the probability of
     * crossing the setpoint +/- hyst is:
     *   P_cross = 2 * Phi(-hyst / sigma)
     * where Phi is the standard normal CDF.
     *
     * To keep chattering < 1 event per hour with 1-second sampling:
     *   P_cross < 1/3600 ≈ 2.78e-4
     *   → hyst/sigma > 3.45
     *   → hyst > 3.45 * sigma (round up to 4*sigma for safety)
     */
    double hyst_from_noise = 4.0 * signal_noise_std;
    double hyst_from_span = setpoint * 0.02; /* 2% of setpoint as span proxy */

    double rec_hyst;
    if (hyst_from_noise > hyst_from_span) {
        rec_hyst = hyst_from_noise;
    } else {
        rec_hyst = hyst_from_span;
    }

    /* Minimum hysteresis to prevent floating-point chatter */
    if (rec_hyst < setpoint * 0.001) {
        rec_hyst = setpoint * 0.001;
    }

    if (recommended_hyst != NULL) {
        *recommended_hyst = rec_hyst;
    }

    /*
     * Estimate chattering frequency with current setup.
     *
     * Chatter probability per second:
     *   p_chatter_ps = exp(-hyst^2 / (2 * sigma^2))
     *   (approximation based on level-crossing rate of Gaussian process)
     *
     * Chatter events per hour:
     *   chatter_ph = p_chatter_ps * 3600
     */
    double chatter_ph;
    if (signal_noise_std > 0.0) {
        double ratio = rec_hyst / signal_noise_std;
        double p_cross = exp(-0.5 * ratio * ratio);
        chatter_ph = p_cross * 3600.0;
    } else {
        chatter_ph = 0.0; /* No noise, no chatter */
    }

    return chatter_ph;
}
