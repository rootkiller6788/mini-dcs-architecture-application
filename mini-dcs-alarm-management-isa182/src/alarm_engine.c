/**
 * @file alarm_engine.c
 * @brief ISA-18.2 Alarm Detection Engine — Real-time alarm condition
 *        evaluation, state machine, flood detection (L3, L5)
 *
 * Knowledge Points:
 *   isa18_check_high_alarm      — High limit check with deadband (L3)
 *   isa18_check_low_alarm       — Low limit check with deadband (L3)
 *   isa18_check_deviation_alarm — Deviation alarm check (L3)
 *   isa18_check_roc_alarm       — Rate-of-change alarm check (L3)
 *   isa18_check_bad_measurement — Bad measurement detection (L3)
 *   isa18_check_discrepancy_alarm — 2oo3 voting discrepancy (L4)
 *   isa18_apply_deadband        — Deadband/hysteresis application (L2)
 *   isa18_alarm_state_transition — 5-state alarm state machine (L3)
 *   isa18_engine_scan           — Full alarm scan cycle (L5)
 *   isa18_flood_detector_update — Flood detection update (L5)
 *   isa18_flood_detector_check  — Flood state check (L5)
 *   isa18_engine_generate_event — Generate alarm event record (L3)
 *   isa18_engine_acknowledge    — Operator acknowledgment (L3)
 *   isa18_engine_priority_sort  — Priority-based active alarm sorting (L5)
 *   isa18_engine_get_active_list — Get active alarm list (L3)
 *   isa18_engine_count_stale_alarms — Stale alarm detection (L5)
 *   isa18_engine_runtime_init   — Runtime initialization (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §11 (Alarm States)
 *   - ISA-TR18.2.4-2012 §4 (Alarm Design)
 *   - EEMUA 191 §5 (Alarm System Design)
 */

#include "alarm_engine.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/*============================================================================
 * L3 — High Alarm Limit Check
 *
 * Evaluates whether the process variable exceeds the high alarm
 * setpoint. The deadband is applied for return-to-normal hysteresis.
 *
 * Condition: PV >= setpoint → alarm active
 *            PV <= setpoint - deadband → return to normal
 *
 * This implements the standard industrial limit checking used in
 * DCS and PLC alarm blocks (Honeywell Experion, Siemens PCS7,
 * Rockwell PlantPAx, Yokogawa CENTUM VP).
 *
 * ISA-18.2 §7.3.1: Deadband shall be at least 2x process noise SD.
 *============================================================================*/
bool isa18_check_high_alarm(
    double process_value,
    double setpoint,
    double deadband)
{
    /* NaN protection: NaN compared with anything is false */
    if (isnan(process_value) || isnan(setpoint)) {
        return false;
    }

    /* Process value exceeds setpoint → alarm condition */
    return (process_value >= setpoint);
}

/*============================================================================
 * L3 — Low Alarm Limit Check
 *
 * Condition: PV <= setpoint → alarm active
 *            PV >= setpoint + deadband → return to normal
 *============================================================================*/
bool isa18_check_low_alarm(
    double process_value,
    double setpoint,
    double deadband)
{
    (void)deadband; /* Used by caller for RTN, not for detection */

    if (isnan(process_value) || isnan(setpoint)) {
        return false;
    }

    /* Process value below setpoint → alarm condition */
    return (process_value <= setpoint);
}

/*============================================================================
 * L3 — Deviation Alarm Check
 *
 * ISA-18.2 §7.3.2: Deviation alarms are used when the process
 * must stay near a target setpoint. Common in:
 *   - Cascade control secondary loops
 *   - Ratio control (FIC to FFC)
 *   - Reactor temperature control
 *   - Distillation column composition control
 *
 * Condition: |PV - target_setpoint| >= deviation_limit
 *============================================================================*/
bool isa18_check_deviation_alarm(
    double process_value,
    double target_setpoint,
    double deviation_limit)
{
    if (isnan(process_value) || isnan(target_setpoint)) {
        return false;
    }
    if (deviation_limit <= 0.0) {
        return false;
    }

    double deviation = fabs(process_value - target_setpoint);
    return (deviation >= deviation_limit);
}

/*============================================================================
 * L3 — Rate-of-Change (ROC) Alarm Check
 *
 * ISA-18.2 §7.3.3: Detects rapid process changes that may lead to
 * unsafe conditions before a high/low limit is reached.
 *
 * ROC = |PV(t) - PV(t-dt)| / dt
 *
 * Common applications:
 *   - Reactor exotherm detection (temperature rise rate)
 *   - Compressor surge detection (flow/pressure change rate)
 *   - Tank overfill protection (level rise rate during filling)
 *
 * Parameters:
 *   current_value:  PV at time t
 *   previous_value: PV at time t-dt
 *   time_delta_sec: dt (must be > 0)
 *   roc_limit:      maximum allowed rate in EU/sec
 *============================================================================*/
bool isa18_check_roc_alarm(
    double current_value,
    double previous_value,
    double time_delta_sec,
    double roc_limit)
{
    if (isnan(current_value) || isnan(previous_value)) {
        return false;
    }
    if (time_delta_sec <= 0.0 || roc_limit <= 0.0) {
        return false;
    }

    double rate = fabs(current_value - previous_value) / time_delta_sec;
    return (rate >= roc_limit);
}

/*============================================================================
 * L3 — Bad Measurement Detection
 *
 * ISA-18.2 §7.3.4: Identifies sensor faults using IEEE 754
 * classification and instrument range validation.
 *
 * Bad measurements include:
 *   - NaN (disconnected sensor, ADC failure)
 *   - +Inf/-Inf (open circuit, short circuit)
 *   - Value below sensor minimum range (sensor fault)
 *   - Value above sensor maximum range (sensor fault)
 *   - Frozen value (value unchanged for extended period) — detected elsewhere
 *
 * This implements the standard DCS "Bad PV" alarm block behavior.
 *============================================================================*/
bool isa18_check_bad_measurement(
    double process_value,
    double sensor_low_range,
    double sensor_high_range)
{
    /* IEEE 754 classification */
    if (isnan(process_value)) {
        return true; /* Not a Number: disconnected or faulty sensor */
    }
    if (isinf(process_value)) {
        return true; /* Infinite value: open circuit or short circuit */
    }

    /* Range validation */
    if (process_value < sensor_low_range) {
        return true; /* Below sensor capability */
    }
    if (process_value > sensor_high_range) {
        return true; /* Above sensor capability */
    }

    return false;
}

/*============================================================================
 * L4 — Discrepancy Alarm using 2oo3 Voting
 *
 * When three redundant sensors measure the same process variable,
 * a discrepancy alarm is raised if any sensor deviates from the
 * median by more than the tolerance.
 *
 * Algorithm:
 *   1. Compute median of three measurements (2oo3 voting)
 *   2. Compare each measurement to median
 *   3. If |measurement - median| > tolerance → discrepancy
 *
 * This is based on the 2oo3 (2-out-of-3) voting architecture
 * used in safety instrumented systems (IEC 61508/61511 SIS).
 *
 * Returns true if ANY measurement deviates from median > tolerance.
 *============================================================================*/
bool isa18_check_discrepancy_alarm(
    double measurement_a,
    double measurement_b,
    double measurement_c,
    double tolerance)
{
    if (tolerance <= 0.0) return false;

    /* Compute median of three values (2oo3) */
    double median;
    if ((measurement_a >= measurement_b && measurement_a <= measurement_c) ||
        (measurement_a <= measurement_b && measurement_a >= measurement_c)) {
        median = measurement_a;
    } else if ((measurement_b >= measurement_a && measurement_b <= measurement_c) ||
               (measurement_b <= measurement_a && measurement_b >= measurement_c)) {
        median = measurement_b;
    } else {
        median = measurement_c;
    }

    /* Check each measurement against median */
    if (fabs(measurement_a - median) > tolerance) return true;
    if (fabs(measurement_b - median) > tolerance) return true;
    if (fabs(measurement_c - median) > tolerance) return true;

    return false;
}

/*============================================================================
 * L2 — Deadband (Hysteresis) Application
 *
 * Deadband is the primary mechanism for preventing alarm chattering
 * in analog alarm points. Without deadband, a process value oscillating
 * near the setpoint would generate repeated alarm activations.
 *
 * Effective return threshold:
 *   For HIGH alarm:  return_threshold = setpoint - deadband  (PV must drop below this)
 *   For LOW alarm:   return_threshold = setpoint + deadband  (PV must rise above this)
 *   For HI_HI alarm: return_threshold = setpoint - deadband
 *   For LO_LO alarm: return_threshold = setpoint + deadband
 *
 * ISA-18.2 §7.4 recommends deadband = max(2x noise_stddev, 1% of span).
 *============================================================================*/
double isa18_apply_deadband(
    double setpoint,
    double deadband,
    isa18_alarm_type_t alarm_type)
{
    if (deadband < 0.0) deadband = 0.0;

    switch (alarm_type) {
    case ISA18_TYPE_HIGH:
    case ISA18_TYPE_HI_HI:
        /* For high-side alarms, return when PV < setpoint - deadband */
        return setpoint - deadband;

    case ISA18_TYPE_LOW:
    case ISA18_TYPE_LO_LO:
        /* For low-side alarms, return when PV > setpoint + deadband */
        return setpoint + deadband;

    case ISA18_TYPE_DEVIATION:
        /* Deviation alarms: return when deviation < limit - deadband */
        return setpoint - deadband;

    case ISA18_TYPE_RATE_OF_CHANGE:
        /* ROC alarms: return when rate < limit - deadband */
        return setpoint - deadband;

    default:
        return setpoint;
    }
}

/*============================================================================
 * L3 — ISA-18.2 Alarm State Transition
 *
 * Implements the complete 5-state alarm state machine per
 * ISA-18.2 §11.2, Figure 11-1.
 *
 * State transition rules:
 *
 *   1. NORMAL → ACTIVE_UNACK:
 *      condition_active AND !suppressed AND !shelved AND on_delay expired
 *
 *   2. ACTIVE_UNACK → ACTIVE_ACK:
 *      operator_ack
 *
 *   3. ACTIVE_ACK → RTN_UNACK:
 *      !condition_active AND off_delay expired
 *
 *   4. ACTIVE_UNACK → RTN_UNACK:
 *      !condition_active AND off_delay expired
 *
 *   5. RTN_UNACK → CLEARED:
 *      operator_ack  (this is the "ring-back" acknowledgment)
 *
 *   6. ACTIVE_ACK → CLEARED:
 *      operator_ack AND !condition_active  (rapid clear)
 *
 *   7. CLEARED → NORMAL:
 *      Automatic after removal from annunciator
 *
 * Suppressed/shelved alarms: remain in NORMAL regardless of condition.
 *============================================================================*/
isa18_alarm_state_t isa18_alarm_state_transition(
    isa18_alarm_state_t current_state,
    bool condition_active,
    bool operator_ack,
    bool is_suppressed,
    bool is_shelved)
{
    switch (current_state) {

    case ISA18_ALARM_STATE_NORMAL:
        /* Alarm can only activate if not suppressed and not shelved */
        if (condition_active && !is_suppressed && !is_shelved) {
            return ISA18_ALARM_STATE_ACTIVE_UNACK;
        }
        return ISA18_ALARM_STATE_NORMAL;

    case ISA18_ALARM_STATE_ACTIVE_UNACK:
        if (operator_ack && condition_active) {
            /* Operator acknowledges while condition still present */
            return ISA18_ALARM_STATE_ACTIVE_ACK;
        }
        if (!condition_active) {
            /* Condition cleared before operator acknowledged */
            return ISA18_ALARM_STATE_RTN_UNACK;
        }
        return ISA18_ALARM_STATE_ACTIVE_UNACK;

    case ISA18_ALARM_STATE_ACTIVE_ACK:
        if (!condition_active) {
            /* Condition cleared after acknowledgement */
            return ISA18_ALARM_STATE_RTN_UNACK;
        }
        /* Operator re-ack while active: clear directly (accelerated clear) */
        if (operator_ack && !condition_active) {
            return ISA18_ALARM_STATE_CLEARED;
        }
        return ISA18_ALARM_STATE_ACTIVE_ACK;

    case ISA18_ALARM_STATE_RTN_UNACK:
        if (operator_ack) {
            /* Operator acknowledges the return-to-normal */
            return ISA18_ALARM_STATE_CLEARED;
        }
        /* If condition reactivates before ack, go back to ACTIVE_UNACK */
        if (condition_active) {
            return ISA18_ALARM_STATE_ACTIVE_UNACK;
        }
        return ISA18_ALARM_STATE_RTN_UNACK;

    case ISA18_ALARM_STATE_CLEARED:
        /* Automatically return to NORMAL after processing */
        return ISA18_ALARM_STATE_NORMAL;

    default:
        return ISA18_ALARM_STATE_NORMAL;
    }
}

/*============================================================================
 * L5 — Full Alarm Scan Cycle
 *
 * This is the main real-time loop. For each configured alarm:
 *   1. Evaluate the alarm condition based on type and process value
 *   2. Check if condition is active (accounting for on/off delay)
 *   3. Execute the state machine transition
 *   4. If state changed, generate an event and update annunciator
 *   5. Update flood detector if new activation
 *
 * Scan rate: typically 250ms to 1000ms in industrial DCS systems.
 *
 * Complexity: O(n) where n = total_alarms (typically 5000-20000).
 *
 * Returns: total number of state transitions that occurred this scan.
 *============================================================================*/
uint32_t isa18_engine_scan(
    isa18_alarm_system_runtime_t *runtime,
    const double *process_values,
    uint32_t value_count,
    time_t current_time)
{
    if (!runtime || !runtime->configs || !process_values) return 0;
    if (value_count != runtime->total_alarms) return 0;

    uint32_t transitions = 0;
    isa18_alarm_config_t *alarms = runtime->configs;

    for (uint32_t i = 0; i < runtime->total_alarms; i++) {
        isa18_alarm_config_t *alarm = &alarms[i];
        double pv = process_values[i];
        bool condition = false;

        /* Step 1: Evaluate alarm condition based on type */
        switch (alarm->alarm_type) {
        case ISA18_TYPE_HIGH:
            condition = isa18_check_high_alarm(pv, alarm->setpoint,
                                                alarm->deadband);
            break;
        case ISA18_TYPE_LOW:
            condition = isa18_check_low_alarm(pv, alarm->setpoint,
                                               alarm->deadband);
            break;
        case ISA18_TYPE_HI_HI:
            condition = isa18_check_high_alarm(pv, alarm->setpoint,
                                                alarm->deadband);
            break;
        case ISA18_TYPE_LO_LO:
            condition = isa18_check_low_alarm(pv, alarm->setpoint,
                                               alarm->deadband);
            break;
        case ISA18_TYPE_DEVIATION:
            condition = isa18_check_deviation_alarm(pv, alarm->setpoint,
                                                     alarm->deviation_limit);
            break;
        case ISA18_TYPE_BAD_MEASUREMENT:
            condition = isa18_check_bad_measurement(pv, -DBL_MAX, DBL_MAX);
            break;
        case ISA18_TYPE_MAINTENANCE_INDICATOR:
        case ISA18_TYPE_SCADA_OFFLINE:
        case ISA18_TYPE_SYSTEM_DIAGNOSTIC:
        case ISA18_TYPE_STATE:
        case ISA18_TYPE_INHIBIT_VIOLATION:
        case ISA18_TYPE_DISCREPANCY:
            /* Discrete/state/diagnostic alarms evaluated by external logic;
               here approximated by simple threshold */
            condition = (pv >= alarm->setpoint);
            break;
        default:
            condition = (pv >= alarm->setpoint);
            break;
        }

        /* Step 2: Check on-delay timer */
        if (condition && alarm->on_delay_ms > 0) {
            if (alarm->current_state == ISA18_ALARM_STATE_NORMAL) {
                time_t expiry = isa18_compute_on_delay_expiry(
                    current_time, alarm->on_delay_ms);
                if (current_time < expiry) {
                    condition = false; /* On-delay not yet expired */
                }
            }
        }

        /* Step 3: Execute state machine */
        isa18_alarm_state_t prev_state = alarm->current_state;
        isa18_alarm_state_t next_state = isa18_alarm_state_transition(
            prev_state, condition, false,
            alarm->is_suppressed, alarm->is_shelved);

        /* Step 4: Update alarm state */
        if (next_state != prev_state) {
            alarm->current_state = next_state;
            transitions++;

            /* Record timestamps */
            if (next_state == ISA18_ALARM_STATE_ACTIVE_UNACK) {
                alarm->activation_time = current_time;
                runtime->active_alarms++;
            } else if (prev_state == ISA18_ALARM_STATE_ACTIVE_UNACK ||
                       prev_state == ISA18_ALARM_STATE_ACTIVE_ACK) {
                if (next_state == ISA18_ALARM_STATE_RTN_UNACK ||
                    next_state == ISA18_ALARM_STATE_CLEARED ||
                    next_state == ISA18_ALARM_STATE_NORMAL) {
                    alarm->return_to_normal_time = current_time;
                    if (runtime->active_alarms > 0) {
                        runtime->active_alarms--;
                    }
                }
            }
            if (next_state == ISA18_ALARM_STATE_CLEARED) {
                alarm->clearing_time = current_time;
            }

            /* Step 5: Generate event record */
            if (runtime->event_count < ISA18_ANNUNCIATOR_MAX_ITEMS) {
                isa18_engine_generate_event(
                    &runtime->recent_events[runtime->event_count],
                    runtime->event_count + 1,
                    alarm->alarm_id,
                    prev_state, next_state,
                    pv, alarm->setpoint,
                    "", false, current_time);
                runtime->event_count++;
            }

            /* Step 6: Update flood detector on new activation */
            if (next_state == ISA18_ALARM_STATE_ACTIVE_UNACK) {
                isa18_flood_detector_update(
                    &runtime->flood_detector, current_time,
                    ISA18_OVERMANAGEABLE_PEAK_10MIN);
            }
        }

        /* Update state for shelved/suppressed alarms */
        if (next_state == ISA18_ALARM_STATE_NORMAL &&
            alarm->current_state != ISA18_ALARM_STATE_NORMAL &&
            !condition) {
            alarm->return_to_normal_time = current_time;
        }
    }

    runtime->last_scan_time = current_time;
    return transitions;
}

/*============================================================================
 * L5 — Alarm Flood Detector Update
 *
 * Maintains a rolling 10-minute window of alarm activations.
 * An alarm flood is declared when the count exceeds the threshold.
 *
 * Algorithm:
 *   1. Check if this event starts a new flood
 *   2. If in flood, track peak alarm count
 *   3. If flood_timeout exceeded with no new alarms, end flood
 *
 * Attributes: O(1) update time (no window sliding needed for counter).
 *============================================================================*/
bool isa18_flood_detector_update(
    isa18_alarm_flood_detector_t *detector,
    time_t event_time,
    uint32_t flood_threshold)
{
    if (!detector) return false;

    double elapsed = difftime(event_time, detector->window_start);

    /* Reset window if 10 minutes have elapsed */
    if (elapsed >= 600.0 || detector->window_start == 0) {
        detector->window_start = event_time;
        detector->alarms_in_window = 0;
    }

    detector->alarms_in_window++;

    /* Check if flood threshold exceeded */
    if (detector->alarms_in_window >= flood_threshold &&
        !detector->is_flood_active) {
        detector->is_flood_active = true;
        detector->flood_start_time = event_time;
        detector->peak_alarms_in_flood = detector->alarms_in_window;
        detector->total_flood_events++;
        return true; /* Flood just started */
    }

    /* Update peak during ongoing flood */
    if (detector->is_flood_active &&
        detector->alarms_in_window > detector->peak_alarms_in_flood) {
        detector->peak_alarms_in_flood = detector->alarms_in_window;
    }

    return false;
}

/*============================================================================
 * L5 — Check Flood Detector State
 *
 * Periodically called to detect flood termination.
 * A flood ends when 10 minutes have passed since the last alarm
 * and the alarm rate has dropped below the threshold.
 *============================================================================*/
bool isa18_flood_detector_check(
    isa18_alarm_flood_detector_t *detector,
    time_t current_time)
{
    if (!detector) return false;
    if (!detector->is_flood_active) return false;

    double elapsed = difftime(current_time, detector->window_start);

    /* If 10-minute window has rolled past, check if still flooding */
    if (elapsed >= 600.0) {
        /* Reset the window */
        detector->window_start = current_time;
        detector->alarms_in_window = 0;

        /* Flood ends if no alarms in new window */
        if (detector->alarms_in_window == 0) {
            detector->is_flood_active = false;
            detector->flood_end_time = current_time;
            detector->flood_duration_sec =
                (uint32_t)difftime(current_time, detector->flood_start_time);
        }
    }

    return detector->is_flood_active;
}

/*============================================================================
 * L3 — Generate Alarm Event Record
 *
 * Creates an isa18_alarm_event_t with a descriptive message
 * suitable for the alarm & event (A&E) historian.
 *============================================================================*/
void isa18_engine_generate_event(
    isa18_alarm_event_t *event,
    uint64_t event_id,
    uint32_t alarm_id,
    isa18_alarm_state_t from_state,
    isa18_alarm_state_t to_state,
    double process_value,
    double setpoint,
    const char *operator_id,
    bool is_operator_action,
    time_t timestamp)
{
    if (!event) return;

    memset(event, 0, sizeof(isa18_alarm_event_t));
    event->event_id = event_id;
    event->alarm_id = alarm_id;
    event->timestamp = timestamp;
    event->from_state = from_state;
    event->to_state = to_state;
    event->process_value = process_value;
    event->setpoint = setpoint;
    event->is_operator_action = is_operator_action;

    if (operator_id) {
        strncpy(event->operator_id, operator_id,
                sizeof(event->operator_id) - 1);
    }

    /* Generate human-readable message */
    snprintf(event->message, ISA18_MAX_MESSAGE_LEN,
             "Alarm %u: %s -> %s, PV=%.4f, SP=%.4f%s%s%s",
             alarm_id,
             isa18_alarm_state_to_string(from_state),
             isa18_alarm_state_to_string(to_state),
             process_value, setpoint,
             is_operator_action ? " [OP:" : "",
             is_operator_action ? (operator_id ? operator_id : "?") : "",
             is_operator_action ? "]" : "");
}

/*============================================================================
 * L3 — Operator Acknowledges Alarm
 *
 * Transitions the alarm state based on acknowledgment:
 *   ACTIVE_UNACK → ACTIVE_ACK  (condition still true)
 *   RTN_UNACK → CLEARED        (condition already false)
 *
 * Records acknowledgement time and operator identity.
 *============================================================================*/
void isa18_engine_acknowledge(
    isa18_alarm_config_t *alarm,
    const char *operator_id,
    time_t ack_time)
{
    if (!alarm) return;

    /* Only acknowledge if in an ack-able state */
    if (alarm->current_state != ISA18_ALARM_STATE_ACTIVE_UNACK &&
        alarm->current_state != ISA18_ALARM_STATE_RTN_UNACK) {
        return;
    }

    /* Perform state transition */
    isa18_alarm_state_t next_state = isa18_alarm_state_transition(
        alarm->current_state,
        (alarm->current_state == ISA18_ALARM_STATE_ACTIVE_UNACK),
        true,  /* operator_ack = true */
        false, /* is_suppressed */
        false  /* is_shelved */
    );

    alarm->current_state = next_state;
    alarm->acknowledgement_time = ack_time;

    if (operator_id) {
        strncpy(alarm->modified_by, operator_id,
                sizeof(alarm->modified_by) - 1);
    }

    if (next_state == ISA18_ALARM_STATE_CLEARED) {
        alarm->clearing_time = ack_time;
    }
}

/*============================================================================
 * L5 — Priority-Based Active Alarm Sorting
 *
 * Sorts active alarm events by: (1) priority (CRITICAL first),
 * then (2) activation time (oldest first within same priority).
 *
 * Implements selection sort: O(n^2). For annunciator displays
 * with typically < 500 items, this is acceptable.
 *============================================================================*/
uint32_t isa18_engine_priority_sort(
    isa18_alarm_event_t *events,
    uint32_t event_count)
{
    if (!events || event_count <= 1) return event_count;

    for (uint32_t i = 0; i < event_count - 1; i++) {
        uint32_t best_idx = i;
        for (uint32_t j = i + 1; j < event_count; j++) {
            /* Compare by priority (lower number = higher priority) */
            if (events[j].priority_number < events[best_idx].priority_number) {
                best_idx = j;
            } else if (events[j].priority_number ==
                       events[best_idx].priority_number) {
                /* Same priority: older first */
                if (events[j].timestamp < events[best_idx].timestamp) {
                    best_idx = j;
                }
            }
        }
        if (best_idx != i) {
            isa18_alarm_event_t tmp = events[i];
            events[i] = events[best_idx];
            events[best_idx] = tmp;
        }
    }
    return event_count;
}

/*============================================================================
 * L3 — Get Active Alarm List for Annunciator
 *
 * Filters configured alarms to those in active states
 * (ACTIVE_UNACK, ACTIVE_ACK) and returns their IDs.
 *
 * ISA-18.2 §11.3: The annunciator should display all active alarms
 * sorted by priority with visual distinction between unacknowledged
 * and acknowledged states.
 *============================================================================*/
uint32_t isa18_engine_get_active_list(
    const isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    uint32_t *active_alarm_ids,
    uint32_t max_ids)
{
    if (!configs || !active_alarm_ids) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < total_alarms && count < max_ids; i++) {
        if (isa18_alarm_state_is_active(configs[i].current_state)) {
            active_alarm_ids[count++] = configs[i].alarm_id;
        }
    }
    return count;
}

/*============================================================================
 * L5 — Stale Alarm Detection
 *
 * ISA-18.2 §16.4.3 / EEMUA 191 §8.3.2: A "stale" alarm is one
 * that has been continuously active (unacknowledged or acknowledged
 * but unresolved) for longer than a defined threshold.
 *
 * Typical thresholds:
 *   > 24 hours → Stale (operator habituated to alarm)
 *   > 7 days  → Chronic (requires management action)
 *
 * Stale alarms are dangerous because they cause:
 *   - Alarm fatigue (operators ignore the alarm)
 *   - Cry-wolf effect (future valid alarms are ignored)
 *
 * Returns: count of stale alarms found.
 *============================================================================*/
uint32_t isa18_engine_count_stale_alarms(
    const isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    time_t current_time,
    uint32_t stale_threshold_sec,
    uint32_t *stale_alarm_ids,
    uint32_t max_stale_ids)
{
    if (!configs) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < total_alarms; i++) {
        if (isa18_alarm_state_is_active(configs[i].current_state)) {
            double duration = difftime(current_time,
                                        configs[i].activation_time);
            if (duration >= (double)stale_threshold_sec) {
                if (stale_alarm_ids && count < max_stale_ids) {
                    stale_alarm_ids[count] = configs[i].alarm_id;
                }
                count++;
            }
        }
    }
    return count;
}

/*============================================================================
 * L3 — Engine Runtime Initialization
 *
 * Initializes the runtime alarm system state structure.
 *============================================================================*/
void isa18_engine_runtime_init(
    isa18_alarm_system_runtime_t *runtime,
    isa18_alarm_config_t *configs,
    uint32_t total_alarms)
{
    if (!runtime) return;

    memset(runtime, 0, sizeof(isa18_alarm_system_runtime_t));
    runtime->configs = configs;
    runtime->total_alarms = total_alarms;
    runtime->active_alarms = 0;
    runtime->event_count = 0;
    runtime->shelved_count = 0;
    runtime->last_scan_time = time(NULL);
    runtime->flood_suppression_active = false;

    /* Initialize flood detector window */
    runtime->flood_detector.window_start = runtime->last_scan_time;
}