/**
 * @file alarm_engine.h
 * @brief ISA-18.2 Alarm Detection Engine — Limit Checking, Deadband,
 *        On/Off Delay, State Machine, Flood Detection (L3, L5)
 *
 * Knowledge Points:
 *   isa18_check_high_alarm      — High limit checking with deadband (L3)
 *   isa18_check_low_alarm       — Low limit checking with deadband (L3)
 *   isa18_check_deviation_alarm — Deviation from setpoint alarm (L3)
 *   isa18_check_roc_alarm       — Rate-of-change detection (L3)
 *   isa18_check_bad_measurement — NaN/Inf/range violation detection (L3)
 *   isa18_apply_deadband        — Hysteresis for return-to-normal (L2)
 *   isa18_alarm_state_transition — ISA-18.2 5-state state machine (L3)
 *   isa18_engine_scan           — Full alarm scan cycle (L5)
 *   isa18_flood_detector_update — Rolling-window flood detection (L5)
 *   isa18_flood_detector_check  — Check if alarm flood is active (L5)
 *   isa18_engine_generate_event — Generate alarm event record (L3)
 *   isa18_engine_acknowledge    — Operator acknowledges alarm (L3)
 *   isa18_engine_shelve         — Operator shelves an alarm (L3)
 *   isa18_engine_priority_sort  — Sort active alarms by priority (L5)
 *   isa18_engine_get_active_list — Get active alarm list for annunciator (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §11 (Alarm States)
 *   - ISA-TR18.2.4-2012 §4 (Alarm Design)
 *   - ISA-101.01-2015 (HMI for alarm display)
 */

#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include "alarm_management_types.h"

/*============================================================================
 * L3 — High Alarm Limit Check
 *
 * Evaluates high alarm condition:
 *   ALARM = (PV >= setpoint) AND persists for on_delay
 *   RTN   = (PV <= setpoint - deadband) AND persists for off_delay
 *
 * Uses deadband (hysteresis) to prevent chattering.
 *
 * ISA-18.2 §7.3.1: High and high-high alarms require deadband.
 *
 * Returns: true if alarm condition is active (PV above setpoint)
 *============================================================================*/
bool isa18_check_high_alarm(
    double process_value,
    double setpoint,
    double deadband);

/*============================================================================
 * L3 — Low Alarm Limit Check
 *
 * Evaluates low alarm condition:
 *   ALARM = (PV <= setpoint) AND persists for on_delay
 *   RTN   = (PV >= setpoint + deadband) AND persists for off_delay
 *
 * Returns: true if alarm condition is active (PV below setpoint)
 *============================================================================*/
bool isa18_check_low_alarm(
    double process_value,
    double setpoint,
    double deadband);

/*============================================================================
 * L3 — Deviation Alarm Check
 *
 * ISA-18.2 §7.3.2: Deviation alarm is triggered when:
 *   |PV - target_setpoint| >= deviation_limit
 *
 * Used for cascade control secondary loops and ratio control.
 *
 * Returns: true if deviation exceeds limit
 *============================================================================*/
bool isa18_check_deviation_alarm(
    double process_value,
    double target_setpoint,
    double deviation_limit);

/*============================================================================
 * L3 — Rate-of-Change (ROC) Alarm Check
 *
 * ISA-18.2 §7.3.3: ROC alarm triggers when the absolute rate of
 * change of the process variable exceeds a configured limit.
 *
 * Computed as finite difference:
 *   ROC = |PV(t) - PV(t-dt)| / dt
 *
 * Returns: true if ROC exceeds limit
 *============================================================================*/
bool isa18_check_roc_alarm(
    double current_value,
    double previous_value,
    double time_delta_sec,
    double roc_limit);

/*============================================================================
 * L3 — Bad Measurement Detection
 *
 * ISA-18.2 §7.3.4: Detects sensor failures, out-of-range signals,
 * NaN (not-a-number), and infinite values.
 *
 * Uses IEEE 754 classification + range validation.
 *
 * Returns: true if measurement is bad
 *============================================================================*/
bool isa18_check_bad_measurement(
    double process_value,
    double sensor_low_range,
    double sensor_high_range);

/*============================================================================
 * L4 — Discrepancy Alarm (2oo3 voting)
 *
 * ISA-18.2 §7.3.8: When redundant measurements disagree beyond
 * a configured tolerance, a discrepancy alarm is raised.
 *
 * Uses the median of three measurements (2oo3 voting) and checks
 * if any measurement deviates from the median by more than tolerance.
 *
 * Returns: true if discrepancy is detected
 *============================================================================*/
bool isa18_check_discrepancy_alarm(
    double measurement_a,
    double measurement_b,
    double measurement_c,
    double tolerance);

/*============================================================================
 * L2 — Deadband (Hysteresis) Application
 *
 * ISA-18.2 §7.4: Deadband prevents alarm chattering by requiring
 * the process variable to move significantly past the setpoint
 * before the return-to-normal transition occurs.
 *
 * This implements the standard process industry practice of adding
 * hysteresis to binary alarm thresholds.
 *
 * Returns the effective threshold for return-to-normal:
 *   For HIGH: effective_return = setpoint - deadband
 *   For LOW:  effective_return = setpoint + deadband
 *============================================================================*/
double isa18_apply_deadband(
    double setpoint,
    double deadband,
    isa18_alarm_type_t alarm_type);

/*============================================================================
 * L3 — ISA-18.2 Alarm State Transition
 *
 * Implements the full 5-state state machine per ISA-18.2 Figure 11-1.
 *
 * Inputs:
 *   - condition_active: is the alarm condition currently true?
 *   - operator_ack: did the operator press acknowledge?
 *   - current_state: the alarm's current state
 *
 * Transitions:
 *   NORMAL + condition && !suppressed && !shelved -> ACTIVE_UNACK
 *   ACTIVE_UNACK + ack -> ACTIVE_ACK
 *   ACTIVE_ACK + !condition -> RTN_UNACK  (ring-back behavior)
 *   ACTIVE_UNACK + !condition -> RTN_UNACK
 *   RTN_UNACK + ack -> CLEARED
 *   ACTIVE_ACK + ack + !condition -> CLEARED
 *   CLEARED -> NORMAL (after removal from annunciator)
 *
 * Suppressed or shelved alarms do NOT transition from NORMAL.
 *
 * Returns: the next state
 *============================================================================*/
isa18_alarm_state_t isa18_alarm_state_transition(
    isa18_alarm_state_t current_state,
    bool condition_active,
    bool operator_ack,
    bool is_suppressed,
    bool is_shelved);

/*============================================================================
 * L5 — Alarm Scan Cycle (Full Engine Iteration)
 *
 * Performs one complete scan of all configured alarms:
 *   1. For each alarm, evaluate the condition (high/low/dev/ROC/bad)
 *   2. Apply on-delay/off-delay timing
 *   3. Execute state machine transition
 *   4. Generate alarm event if state changed
 *   5. Update annunciator list
 *   6. Update flood detector
 *   7. Update KPI counters
 *
 * This is the main real-time loop of the alarm system, typically
 * executed at a 250ms to 1s scan rate.
 *
 * Parameters:
 *   runtime:    current alarm system state
 *   process_values: array of process values indexed by alarm position
 *   value_count:    number of process values (must equal total_alarms)
 *   current_time:   current wall-clock time
 *
 * Returns: number of state transitions that occurred
 *============================================================================*/
uint32_t isa18_engine_scan(
    isa18_alarm_system_runtime_t *runtime,
    const double *process_values,
    uint32_t value_count,
    time_t current_time);

/*============================================================================
 * L5 — Alarm Flood Detection (Rolling Window)
 *
 * ISA-18.2 §16.5: An alarm flood is declared when the number of
 * alarm activations in a 10-minute rolling window exceeds the threshold.
 *
 * EEMUA 191 thresholds:
 *   >= 10 alarms / 10 min  → manageable
 *   >= 30 alarms / 10 min  → over-manageable (flood)
 *   >= 100 alarms / 10 min → definitely excessive
 *
 * This function updates the flood detector state based on a new alarm event.
 *
 * Returns: true if flood just started with this event
 *============================================================================*/
bool isa18_flood_detector_update(
    isa18_alarm_flood_detector_t *detector,
    time_t event_time,
    uint32_t flood_threshold);

/*============================================================================
 * L5 — Check if Alarm Flood is Currently Active
 *
 * Also handles flood end detection: if the 10-minute window has
 * elapsed with no new alarms, the flood ends.
 *============================================================================*/
bool isa18_flood_detector_check(
    isa18_alarm_flood_detector_t *detector,
    time_t current_time);

/*============================================================================
 * L3 — Generate Alarm Event Record
 *
 * Creates an isa18_alarm_event_t when an alarm changes state.
 * Records the PV value, setpoint, operator ID (if applicable),
 * and a human-readable message.
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
    time_t timestamp);

/*============================================================================
 * L3 — Operator Acknowledges Alarm
 *
 * Transitions ACTIVE_UNACK → ACTIVE_ACK or RTN_UNACK → CLEARED.
 * Records the operator ID and timestamp.
 *============================================================================*/
void isa18_engine_acknowledge(
    isa18_alarm_config_t *alarm,
    const char *operator_id,
    time_t ack_time);

/*============================================================================
 * L5 — Priority-Based Active Alarm Sorting
 *
 * Sorts the annunciator's active alarm list by priority
 * (CRITICAL > HIGH > MEDIUM > LOW), then by activation time
 * (oldest first) within each priority level.
 *
 * ISA-101.01 HMI standard recommends grouping alarms by priority
 * with the most critical alarms at the top of the display.
 *
 * Complexity: O(n^2) insertion sort adapted for priority-then-age
 *============================================================================*/
uint32_t isa18_engine_priority_sort(
    isa18_alarm_event_t *events,
    uint32_t event_count);

/*============================================================================
 * L3 — Get Active Alarm List for Annunciator
 *
 * Filters the configured alarms to those in ACTIVE_UNACK or
 * ACTIVE_ACK state, ordered by priority. Returns count.
 *============================================================================*/
uint32_t isa18_engine_get_active_list(
    const isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    uint32_t *active_alarm_ids,
    uint32_t max_ids);

/*============================================================================
 * L5 — Stale Alarm Detection
 *
 * An alarm is "stale" (ISA-18.2 §16.4.3 / EEMUA 191 §8.3.2) if it
 * has been active without acknowledgement for longer than a threshold.
 *
 * Returns: count of stale alarms
 *============================================================================*/
uint32_t isa18_engine_count_stale_alarms(
    const isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    time_t current_time,
    uint32_t stale_threshold_sec,
    uint32_t *stale_alarm_ids,
    uint32_t max_stale_ids);

/*============================================================================
 * L5 — Engine Runtime Initialization
 *============================================================================*/
void isa18_engine_runtime_init(
    isa18_alarm_system_runtime_t *runtime,
    isa18_alarm_config_t *configs,
    uint32_t total_alarms);

#endif /* ALARM_ENGINE_H */