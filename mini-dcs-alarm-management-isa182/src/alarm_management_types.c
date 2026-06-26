/**
 * @file alarm_management_types.c
 * @brief ISA-18.2 Alarm Management — Type Utility Functions (L1, L2)
 *
 * Knowledge Points (each function = one independent concept):
 *   isa18_priority_to_string    — Priority enum to human-readable string (L1)
 *   isa18_alarm_type_to_string  — Alarm type enum to description (L1)
 *   isa18_alarm_state_to_string — Alarm state enum to string (L1)
 *   isa18_lifecycle_phase_to_string — Lifecycle phase enum to string (L1)
 *   isa18_alarm_class_to_string — Alarm class enum to string (L1)
 *   isa18_severity_to_string    — Severity enum to string (L1)
 *   isa18_urgency_to_string     — Urgency enum to string (L1)
 *   isa18_alarm_config_init     — Initialize alarm config with defaults (L3)
 *   isa18_alarm_config_set_high — Configure high alarm setpoint (L3)
 *   isa18_alarm_config_set_low  — Configure low alarm setpoint (L3)
 *   isa18_alarm_config_validate — Validate alarm config consistency (L4)
 *   isa18_alarm_type_is_analog  — Check if alarm type is analog (L2)
 *   isa18_alarm_type_is_discrete — Check if alarm type is discrete (L2)
 *   isa18_priority_color_code   — ISA-101 HMI color code for priority (L7)
 *   isa18_compare_alarms_by_priority — Comparison function for sorting (L5)
 *
 * References:
 *   - ISA-101.01-2015 (HMI color standards)
 *   - ANSI/ISA-18.2-2016
 */

#include "alarm_management_types.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * L1 — Enum to String Converters
 *
 * Each converter maps an ISA-18.2 enumeration to its standard
 * text representation for HMI display and audit trails.
 *============================================================================*/

const char *isa18_priority_to_string(isa18_alarm_priority_t priority)
{
    switch (priority) {
    case ISA18_PRIORITY_CRITICAL: return "CRITICAL";
    case ISA18_PRIORITY_HIGH:     return "HIGH";
    case ISA18_PRIORITY_MEDIUM:   return "MEDIUM";
    case ISA18_PRIORITY_LOW:      return "LOW";
    default:                      return "UNKNOWN";
    }
}

const char *isa18_alarm_type_to_string(isa18_alarm_type_t type)
{
    switch (type) {
    case ISA18_TYPE_HIGH:               return "HIGH";
    case ISA18_TYPE_LOW:                return "LOW";
    case ISA18_TYPE_HI_HI:              return "HI_HI";
    case ISA18_TYPE_LO_LO:              return "LO_LO";
    case ISA18_TYPE_DEVIATION:          return "DEVIATION";
    case ISA18_TYPE_RATE_OF_CHANGE:     return "RATE_OF_CHANGE";
    case ISA18_TYPE_BAD_MEASUREMENT:    return "BAD_MEASUREMENT";
    case ISA18_TYPE_SYSTEM_DIAGNOSTIC:  return "SYSTEM_DIAGNOSTIC";
    case ISA18_TYPE_DISCREPANCY:        return "DISCREPANCY";
    case ISA18_TYPE_STATE:              return "STATE";
    case ISA18_TYPE_SCADA_OFFLINE:      return "SCADA_OFFLINE";
    case ISA18_TYPE_INHIBIT_VIOLATION:  return "INHIBIT_VIOLATION";
    case ISA18_TYPE_MAINTENANCE_INDICATOR: return "MAINTENANCE_INDICATOR";
    default:                            return "UNKNOWN";
    }
}

const char *isa18_alarm_state_to_string(isa18_alarm_state_t state)
{
    switch (state) {
    case ISA18_ALARM_STATE_NORMAL:        return "NORMAL";
    case ISA18_ALARM_STATE_ACTIVE_UNACK:  return "ACTIVE_UNACK";
    case ISA18_ALARM_STATE_ACTIVE_ACK:    return "ACTIVE_ACK";
    case ISA18_ALARM_STATE_RTN_UNACK:     return "RTN_UNACK";
    case ISA18_ALARM_STATE_CLEARED:       return "CLEARED";
    default:                              return "UNKNOWN";
    }
}

const char *isa18_lifecycle_phase_to_string(isa18_lifecycle_phase_t phase)
{
    switch (phase) {
    case ISA18_LIFECYCLE_PHILOSOPHY:      return "A: PHILOSOPHY";
    case ISA18_LIFECYCLE_IDENTIFICATION:  return "B: IDENTIFICATION";
    case ISA18_LIFECYCLE_RATIONALIZATION: return "C: RATIONALIZATION";
    case ISA18_LIFECYCLE_DETAILED_DESIGN: return "D: DETAILED_DESIGN";
    case ISA18_LIFECYCLE_IMPLEMENTATION:  return "E: IMPLEMENTATION";
    case ISA18_LIFECYCLE_OPERATION:       return "F: OPERATION";
    case ISA18_LIFECYCLE_MAINTENANCE:     return "G: MAINTENANCE";
    case ISA18_LIFECYCLE_MONITORING:      return "H: MONITORING";
    case ISA18_LIFECYCLE_AUDIT:           return "I: AUDIT";
    default:                              return "UNKNOWN";
    }
}

const char *isa18_alarm_class_to_string(isa18_alarm_class_t alarm_class)
{
    switch (alarm_class) {
    case ISA18_CLASS_ALARM:    return "ALARM";
    case ISA18_CLASS_ALERT:    return "ALERT";
    case ISA18_CLASS_PROMPT:   return "PROMPT";
    case ISA18_CLASS_NO_ALARM: return "NO_ALARM";
    default:                   return "UNKNOWN";
    }
}

const char *isa18_severity_to_string(isa18_consequence_severity_t severity)
{
    switch (severity) {
    case ISA18_SEVERITY_CRITICAL: return "CRITICAL";
    case ISA18_SEVERITY_SEVERE:   return "SEVERE";
    case ISA18_SEVERITY_MAJOR:    return "MAJOR";
    case ISA18_SEVERITY_MODERATE: return "MODERATE";
    default:                      return "UNKNOWN";
    }
}

const char *isa18_urgency_to_string(isa18_urgency_t urgency)
{
    switch (urgency) {
    case ISA18_URGENCY_IMMEDIATE:   return "IMMEDIATE (<=3min)";
    case ISA18_URGENCY_PROMPT:      return "PROMPT (<=15min)";
    case ISA18_URGENCY_RAPID:       return "RAPID (<=30min)";
    case ISA18_URGENCY_NON_URGENT:  return "NON_URGENT (>30min)";
    default:                        return "UNKNOWN";
    }
}

/*============================================================================
 * L3 — Initialize Alarm Configuration with Defaults
 *
 * Sets up a new alarm_config_t with safe default values before
 * the detailed design phase populates the actual parameters.
 *============================================================================*/
void isa18_alarm_config_init(isa18_alarm_config_t *alarm, uint32_t alarm_id,
                              const char *tag_name)
{
    if (!alarm) return;

    memset(alarm, 0, sizeof(isa18_alarm_config_t));
    alarm->alarm_id = alarm_id;
    if (tag_name) {
        strncpy(alarm->tag_name, tag_name, sizeof(alarm->tag_name) - 1);
    }
    alarm->alarm_type = ISA18_TYPE_HIGH;
    alarm->priority = ISA18_PRIORITY_LOW;
    alarm->alarm_class = ISA18_CLASS_ALARM;
    alarm->setpoint = 0.0;
    alarm->deadband = 0.0;
    alarm->rate_of_change_limit = 0.0;
    alarm->deviation_limit = 0.0;
    alarm->on_delay_ms = 0;
    alarm->off_delay_ms = 0;
    strncpy(alarm->consequence, "Not specified",
            sizeof(alarm->consequence) - 1);
    strncpy(alarm->corrective_action, "Not specified",
            sizeof(alarm->corrective_action) - 1);
    alarm->time_to_respond_sec = ISA18_MAX_RESPONSE_TIME_DEFAULT;
    alarm->rationalization_date = 0;
    alarm->is_rationalized = false;
    alarm->plant_state_mask = 0;
    alarm->alarm_group_id = 0;
    alarm->revision = 1;
    alarm->last_modified = 0;
    alarm->current_state = ISA18_ALARM_STATE_NORMAL;
    alarm->activation_time = 0;
    alarm->acknowledgement_time = 0;
    alarm->return_to_normal_time = 0;
    alarm->clearing_time = 0;
    alarm->is_shelved = false;
    alarm->shelve_expiry = 0;
    alarm->is_suppressed = false;
}

/*============================================================================
 * L3 — Configure Alarm as High Type
 *
 * Sets the alarm to HIGH type with the specified setpoint and deadband.
 * Deadband prevents chattering when the process variable oscillates
 * near the setpoint.
 *
 * ISA-18.2 §7.3.1: Deadband should be set to at least 2x the
 * measurement noise standard deviation.
 *============================================================================*/
void isa18_alarm_config_set_high(isa18_alarm_config_t *alarm,
                                  double setpoint, double deadband)
{
    if (!alarm) return;
    alarm->alarm_type = ISA18_TYPE_HIGH;
    alarm->setpoint = setpoint;
    alarm->deadband = deadband;
    alarm->last_modified = time(NULL);
}

/*============================================================================
 * L3 — Configure Alarm as Low Type
 *============================================================================*/
void isa18_alarm_config_set_low(isa18_alarm_config_t *alarm,
                                 double setpoint, double deadband)
{
    if (!alarm) return;
    alarm->alarm_type = ISA18_TYPE_LOW;
    alarm->setpoint = setpoint;
    alarm->deadband = deadband;
    alarm->last_modified = time(NULL);
}

/*============================================================================
 * L4 — Validate Alarm Configuration Consistency
 *
 * Checks that the alarm configuration is internally consistent
 * per ISA-18.2 design rules:
 *   1. Deadband must be non-negative
 *   2. HI_HI setpoint must be > HIGH setpoint (for the same tag)
 *   3. LO_LO setpoint must be < LOW setpoint
 *   4. On-delay and off-delay must not both be non-zero unnecessarily
 *   5. ROC limit must be > 0 for ROC type alarms
 *   6. Deviation limit must be > 0 for deviation type alarms
 *   7. Rationalized alarm must have non-empty consequence/action
 *   8. Time to respond must be > 0
 *
 * Returns: number of validation errors found
 *============================================================================*/
uint32_t isa18_alarm_config_validate(const isa18_alarm_config_t *alarm,
                                      char error_buf[][ISA18_MAX_MESSAGE_LEN],
                                      uint32_t max_errors)
{
    uint32_t errors = 0;
    if (!alarm) return 0;

    #define ADD_ERR(msg) do { \
        if (errors < max_errors) { \
            strncpy(error_buf[errors], msg, ISA18_MAX_MESSAGE_LEN - 1); \
            error_buf[errors][ISA18_MAX_MESSAGE_LEN - 1] = '\0'; \
        } \
        errors++; \
    } while(0)

    if (alarm->deadband < 0.0) {
        ADD_ERR("Deadband must be >= 0");
    }
    if (alarm->alarm_type == ISA18_TYPE_RATE_OF_CHANGE &&
        alarm->rate_of_change_limit <= 0.0) {
        ADD_ERR("ROC alarm requires positive rate_of_change_limit");
    }
    if (alarm->alarm_type == ISA18_TYPE_DEVIATION &&
        alarm->deviation_limit <= 0.0) {
        ADD_ERR("Deviation alarm requires positive deviation_limit");
    }
    if (alarm->is_rationalized) {
        if (strlen(alarm->consequence) == 0 ||
            strcmp(alarm->consequence, "Not specified") == 0) {
            ADD_ERR("Rationalized alarm must define consequence");
        }
        if (strlen(alarm->corrective_action) == 0 ||
            strcmp(alarm->corrective_action, "Not specified") == 0) {
            ADD_ERR("Rationalized alarm must define corrective action");
        }
    }
    if (alarm->time_to_respond_sec == 0) {
        ADD_ERR("Time to respond must be > 0");
    }
    if (alarm->on_delay_ms > 60000) {
        ADD_ERR("On-delay exceeds 60 seconds (check configuration)");
    }
    if (alarm->off_delay_ms > 60000) {
        ADD_ERR("Off-delay exceeds 60 seconds (check configuration)");
    }

    #undef ADD_ERR
    return errors;
}

/*============================================================================
 * L2 — Check if Alarm Type is Analog (Continuous)
 *
 * Analog alarms operate on continuous process values (4-20mA, float).
 *--------------------------------------------------------------------------*/
bool isa18_alarm_type_is_analog(isa18_alarm_type_t type)
{
    switch (type) {
    case ISA18_TYPE_HIGH:
    case ISA18_TYPE_LOW:
    case ISA18_TYPE_HI_HI:
    case ISA18_TYPE_LO_LO:
    case ISA18_TYPE_DEVIATION:
    case ISA18_TYPE_RATE_OF_CHANGE:
    case ISA18_TYPE_BAD_MEASUREMENT:
        return true;
    default:
        return false;
    }
}

/*============================================================================
 * L2 — Check if Alarm Type is Discrete (Binary/State)
 *
 * Discrete alarms operate on boolean or enumerated state values.
 *--------------------------------------------------------------------------*/
bool isa18_alarm_type_is_discrete(isa18_alarm_type_t type)
{
    switch (type) {
    case ISA18_TYPE_STATE:
    case ISA18_TYPE_SYSTEM_DIAGNOSTIC:
    case ISA18_TYPE_SCADA_OFFLINE:
    case ISA18_TYPE_INHIBIT_VIOLATION:
    case ISA18_TYPE_MAINTENANCE_INDICATOR:
        return true;
    default:
        return false;
    }
}

/*============================================================================
 * L7 — ISA-101 HMI Color Code for Alarm Priority
 *
 * ISA-101.01-2015 §6.3 defines standard HMI colors for alarm
 * annunciation. These colors enable operators to rapidly
 * assess alarm severity.
 *
 * Critical: Red (#FF0000)   — Flashing
 * High:     Magenta (#FF00FF) — Flashing on activation
 * Medium:   Yellow (#FFFF00)
 * Low:      White (#FFFFFF)
 *
 * Returns a color index:
 *   0 = Red, 1 = Magenta, 2 = Yellow, 3 = White, -1 = Unknown
 *============================================================================*/
int isa18_priority_color_code(isa18_alarm_priority_t priority)
{
    switch (priority) {
    case ISA18_PRIORITY_CRITICAL: return 0;  /* Red */
    case ISA18_PRIORITY_HIGH:     return 1;  /* Magenta */
    case ISA18_PRIORITY_MEDIUM:   return 2;  /* Yellow */
    case ISA18_PRIORITY_LOW:      return 3;  /* White */
    default:                      return -1;
    }
}

/*============================================================================
 * L5 — Compare Two Alarms by Priority (for Sorting)
 *
 * Used to sort alarm lists so critical alarms appear first.
 * Comparison order: priority (CRITICAL first), then activation time
 * (oldest first).
 *
 * Returns: negative if a < b, 0 if equal, positive if a > b
 *============================================================================*/
int isa18_compare_alarms_by_priority(const isa18_alarm_config_t *a,
                                      const isa18_alarm_config_t *b)
{
    if (!a && !b) return 0;
    if (!a) return 1;
    if (!b) return -1;

    /* Lower enum value = higher priority */
    if (a->priority != b->priority) {
        return (int)a->priority - (int)b->priority;
    }

    /* Same priority: older activation first */
    if (a->activation_time != b->activation_time) {
        if (a->activation_time < b->activation_time) return -1;
        if (a->activation_time > b->activation_time) return 1;
    }
    return 0;
}

/*============================================================================
 * L3 — Alarm Config Copy (Management of Change Baseline)
 *
 * Creates a snapshot of an alarm configuration for MOC comparison.
 *============================================================================*/
void isa18_alarm_config_copy(const isa18_alarm_config_t *src,
                              isa18_alarm_config_t *dst)
{
    if (!src || !dst) return;
    memcpy(dst, src, sizeof(isa18_alarm_config_t));
}

/*============================================================================
 * L3 — Check if Alarm is Return-to-Normal (RTN)
 *
 * An alarm is "returned to normal" when the process condition
 * that caused it is no longer present, but the operator has not
 * yet acknowledged.
 *
 * States representing RTN: RTN_UNACK, CLEARED
 *============================================================================*/
bool isa18_alarm_state_is_rtn(isa18_alarm_state_t state)
{
    return (state == ISA18_ALARM_STATE_RTN_UNACK ||
            state == ISA18_ALARM_STATE_CLEARED);
}

/*============================================================================
 * L2 — Check if Alarm State is Active (Requires Annunciation)
 *
 * Alarms in these states should appear on the annunciator
 * and require operator attention.
 *============================================================================*/
bool isa18_alarm_state_is_active(isa18_alarm_state_t state)
{
    return (state == ISA18_ALARM_STATE_ACTIVE_UNACK ||
            state == ISA18_ALARM_STATE_ACTIVE_ACK);
}

/*============================================================================
 * L1 — Check if Alarm State Represents an Acknowledged Condition
 *============================================================================*/
bool isa18_alarm_state_is_acknowledged(isa18_alarm_state_t state)
{
    return (state == ISA18_ALARM_STATE_ACTIVE_ACK ||
            state == ISA18_ALARM_STATE_CLEARED);
}

/*============================================================================
 * L1 — Get Priority as Numeric Ranking (1-4 for KPI calculations)
 *============================================================================*/
uint32_t isa18_priority_to_numeric(isa18_alarm_priority_t priority)
{
    switch (priority) {
    case ISA18_PRIORITY_CRITICAL: return 1;
    case ISA18_PRIORITY_HIGH:     return 2;
    case ISA18_PRIORITY_MEDIUM:   return 3;
    case ISA18_PRIORITY_LOW:      return 4;
    default:                      return 0;
    }
}

/*============================================================================
 * L3 — Compute On-Delay Expiry Time
 *
 * Given the time the condition first became true, compute when
 * the on-delay timer expires, after which the alarm activates.
 *
 * Returns: expiry timestamp (0 if no delay configured)
 *============================================================================*/
time_t isa18_compute_on_delay_expiry(time_t condition_true_time,
                                      uint32_t on_delay_ms)
{
    if (on_delay_ms == 0) return condition_true_time;
    return condition_true_time + (time_t)(on_delay_ms / 1000);
}

/*============================================================================
 * L3 — Compute Off-Delay Expiry Time
 *
 * Given the time the condition first became false, compute when
 * the off-delay timer expires, after which the alarm clears.
 *============================================================================*/
time_t isa18_compute_off_delay_expiry(time_t condition_false_time,
                                       uint32_t off_delay_ms)
{
    if (off_delay_ms == 0) return condition_false_time;
    return condition_false_time + (time_t)(off_delay_ms / 1000);
}

/*============================================================================
 * L2 — Alarm Activation Eligibility Check
 *
 * An alarm can only activate (transition NORMAL -> ACTIVE_UNACK) if:
 *   1. The alarm is configured (has a valid tag and setpoint)
 *   2. The alarm is rationalized (per IS-18.2 §9)
 *   3. The alarm is not shelved
 *   4. The alarm is not suppressed by current plant state
 *   5. The alarm is classified as ISA18_CLASS_ALARM
 *
 * Returns: true if the alarm is eligible to activate
 *============================================================================*/
bool isa18_alarm_can_activate(const isa18_alarm_config_t *alarm)
{
    if (!alarm) return false;
    if (!alarm->is_rationalized) return false;
    if (alarm->is_shelved) return false;
    if (alarm->is_suppressed) return false;
    if (alarm->alarm_class != ISA18_CLASS_ALARM) return false;
    return true;
}