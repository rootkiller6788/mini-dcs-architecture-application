/**
 * @file alarm_rationalization.h
 * @brief ISA-18.2 Alarm Rationalization — Priority Matrix, Justification,
 *        Master Alarm Database Operations (L2, L4)
 *
 * Knowledge Points:
 *   isa18_assign_priority_matrix      — 4x4 severity-urgency priority matrix (L2)
 *   isa18_rationalization_init_record — Create rationalization record (L3)
 *   isa18_rationalization_justify     — Justification assessment per ISA-18.2 (L2)
 *   isa18_rationalization_calc_response_time — Max safe response time (L4)
 *   isa18_mad_init                    — Initialize master alarm database (L3)
 *   isa18_mad_add_alarm               — Add alarm to MAD (L3)
 *   isa18_mad_find_alarm              — Look up alarm by tag (L3)
 *   isa18_mad_remove_alarm            — Remove alarm from MAD (L3)
 *   isa18_mad_count_by_priority       — Count alarms by priority level (L3)
 *   isa18_mad_validate                — MAD consistency check (L4)
 *   isa18_mad_get_unrationalized      — Find alarms not yet rationalized (L3)
 *   isa18_mad_export_summary          — Export MAD summary statistics (L3)
 *   isa18_check_alarm_justified       — Determine if alarm is justified (L2)
 *   isa18_rationalization_team_add_member — Add team member (L3)
 *   isa18_calc_response_time_margin   — Response time safety margin (L4)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §9 (Rationalization)
 *   - ISA-TR18.2.4-2012 (Advanced Methods)
 *   - EEMUA 191 §6 (Alarm Prioritization)
 */

#ifndef ALARM_RATIONALIZATION_H
#define ALARM_RATIONALIZATION_H

#include "alarm_management_types.h"

/*============================================================================
 * L2 — ISA-18.2 Priority Assignment Matrix
 *
 * Maps (severity, urgency) to (priority) per ISA-18.2 Table 9-1.
 * The matrix encodes the "Consequence of Inaction" weighting.
 *
 *        | Immediate | Prompt  | Rapid   | Non-Urgent
 * -------+-----------+---------+---------+------------
 * Critical| CRITICAL | CRITICAL| HIGH    | MEDIUM
 * Severe  | CRITICAL | HIGH   | MEDIUM  | LOW
 * Major   | HIGH     | MEDIUM  | MEDIUM  | LOW
 * Moderate| MEDIUM   | LOW     | LOW     | LOW
 *
 * This is the 4x4 matrix variant. ISA-18.2 also allows 3x3.
 *============================================================================*/
isa18_alarm_priority_t isa18_assign_priority_matrix(
    isa18_consequence_severity_t severity,
    isa18_urgency_t urgency);

/*============================================================================
 * L2 — Consequence-Based Alarm Justification
 *
 * ISA-18.2 §9.2: An alarm is justified only if:
 *   1. There is a defined consequence of inaction
 *   2. There is a defined operator response
 *   3. The operator has sufficient time to respond
 *   4. The alarm cannot be eliminated by design change
 *
 * Returns true if all four criteria are satisfied.
 *============================================================================*/
bool isa18_check_alarm_justified(
    const char *consequence,
    const char *corrective_action,
    uint32_t time_to_respond_sec,
    bool can_be_eliminated_by_design);

/*============================================================================
 * L4 — Maximum Safe Response Time Calculation
 *
 * Calculates the maximum time an operator has to respond before
 * the consequence becomes unavoidable. Based on process dynamics.
 *
 * Formula: T_safe = T_process_deadline - T_detection - T_diagnosis
 *
 * Where:
 *   T_process_deadline = time from alarm condition to consequence
 *   T_detection        = time for sensor to detect condition
 *   T_diagnosis        = time for operator to understand the situation
 *
 * ISA-18.2 §9.5: T_safe must be >= T_respond_min (typically 10 min)
 *============================================================================*/
double isa18_calc_max_safe_response_time(
    double process_deadline_sec,
    double detection_lag_sec,
    double diagnosis_time_sec);

/*============================================================================
 * L4 — Response Time Safety Margin
 *
 * Ensures operator response time has adequate margin:
 *   safety_margin = (T_safe - T_respond_min) / T_safe
 *
 * A negative margin means the alarm is not fit for purpose
 * (the process will reach the consequence before the operator can respond).
 *
 * Returns safety margin as a ratio [0.0, 1.0] or negative.
 *============================================================================*/
double isa18_calc_response_time_margin(
    double max_safe_response_time_sec,
    double min_required_response_time_sec);

/*============================================================================
 * L3 — Initialize Rationalization Record
 *============================================================================*/
void isa18_rationalization_init_record(
    isa18_rationalization_record_t *record,
    uint32_t record_id,
    uint32_t alarm_id,
    const char *tag_name);

/*============================================================================
 * L3 — Assign Rationalization Outcome
 *
 * Sets the class, priority, severity, urgency, and justification
 * on a rationalization record. This is the core step that converts
 * an identified alarm into a rationalized one.
 *============================================================================*/
void isa18_rationalization_set_outcome(
    isa18_rationalization_record_t *record,
    isa18_alarm_class_t alarm_class,
    isa18_alarm_priority_t priority,
    isa18_consequence_severity_t severity,
    isa18_urgency_t urgency,
    bool is_justified,
    const char *justification);

/*============================================================================
 * L3 — Add Rationalization Team Member
 *============================================================================*/
bool isa18_rationalization_team_add_member(
    isa18_rationalization_record_t *record,
    const char *member_name);

/*============================================================================
 * L3 — Export Rationalization to Alarm Config
 *
 * Transfers the rationalization outcome to the alarm configuration.
 * This implements the C->D transition in the ISA-18.2 lifecycle.
 *============================================================================*/
void isa18_rationalization_apply_to_alarm(
    const isa18_rationalization_record_t *record,
    isa18_alarm_config_t *alarm);

/*============================================================================
 * L3 — Master Alarm Database (MAD) Operations
 *============================================================================*/

/** Initialize an empty master alarm database */
void isa18_mad_init(
    isa18_master_alarm_database_t *mad,
    const char *site_name,
    const char *philosophy_doc_ref);

/** Add a new alarm configuration to the MAD. Returns alarm_id on success, 0 on failure. */
uint32_t isa18_mad_add_alarm(
    isa18_master_alarm_database_t *mad,
    const isa18_alarm_config_t *alarm_config);

/** Find an alarm by its tag name. Returns pointer or NULL. */
isa18_alarm_config_t *isa18_mad_find_alarm(
    isa18_master_alarm_database_t *mad,
    const char *tag_name);

/** Find an alarm by its ID. Returns pointer or NULL. Complexity: O(n). */
isa18_alarm_config_t *isa18_mad_find_by_id(
    isa18_master_alarm_database_t *mad,
    uint32_t alarm_id);

/** Remove an alarm from the MAD. Returns true on success. */
bool isa18_mad_remove_alarm(
    isa18_master_alarm_database_t *mad,
    uint32_t alarm_id);

/** Count alarms at each priority level */
void isa18_mad_count_by_priority(
    const isa18_master_alarm_database_t *mad,
    uint32_t *out_critical,
    uint32_t *out_high,
    uint32_t *out_medium,
    uint32_t *out_low);

/** Validate MAD for consistency: no duplicate tags, all rationalized, valid setpoints */
void isa18_mad_validate(
    const isa18_master_alarm_database_t *mad,
    uint32_t *out_errors,
    char error_list[][ISA18_MAX_MESSAGE_LEN],
    uint32_t max_errors);

/** Get count of alarms not yet rationalized */
uint32_t isa18_mad_get_unrationalized_count(
    const isa18_master_alarm_database_t *mad);

/** Export MAD summary statistics */
void isa18_mad_export_summary(
    const isa18_master_alarm_database_t *mad,
    uint32_t *total_alarms,
    uint32_t *rationalized,
    uint32_t *by_class[4],
    uint32_t *by_priority[4]);

/** Calculate rationalization coverage percentage */
double isa18_mad_calc_rationalization_coverage(
    const isa18_master_alarm_database_t *mad);

#endif /* ALARM_RATIONALIZATION_H */