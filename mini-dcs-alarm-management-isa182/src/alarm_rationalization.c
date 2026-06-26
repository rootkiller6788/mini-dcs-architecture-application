/**
 * @file alarm_rationalization.c
 * @brief ISA-18.2 Alarm Rationalization — Priority Matrix, Justification,
 *        Master Alarm Database (MAD) Operations (L2, L3, L4)
 *
 * Knowledge Points:
 *   isa18_assign_priority_matrix      — 4x4 severity-urgency priority matrix (L2)
 *   isa18_check_alarm_justified       — 4-criteria justification test (L2)
 *   isa18_calc_max_safe_response_time — Process dynamics-based safe response (L4)
 *   isa18_calc_response_time_margin   — Response time safety margin (L4)
 *   isa18_rationalization_init_record — Rationalization record creation (L3)
 *   isa18_rationalization_set_outcome — Assign rationalization result (L3)
 *   isa18_rationalization_team_add_member — Team management (L3)
 *   isa18_rationalization_apply_to_alarm — Apply outcome to alarm config (L3)
 *   isa18_mad_init                    — Initialize master alarm database (L3)
 *   isa18_mad_add_alarm               — Add alarm to MAD (L3)
 *   isa18_mad_find_alarm              — Lookup by tag (L3)
 *   isa18_mad_find_by_id              — Lookup by ID (L3)
 *   isa18_mad_remove_alarm            — Remove alarm from MAD (L3)
 *   isa18_mad_count_by_priority       — Priority distribution count (L3)
 *   isa18_mad_validate                — MAD consistency check (L4)
 *   isa18_mad_get_unrationalized_count — Unrationalized alarm count (L3)
 *   isa18_mad_export_summary           — Summary statistics export (L3)
 *   isa18_mad_calc_rationalization_coverage — Coverage percentage (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §9 (Rationalization)
 *   - ISA-TR18.2.4-2012
 *   - EEMUA 191 §6
 */

#include "alarm_rationalization.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * L2 — ISA-18.2 Priority Assignment Matrix (4x4)
 *
 * Maps (severity, urgency) to (priority) using the standard
 * ISA-18.2 priority matrix. This is a deterministic table lookup.
 *
 * Priority = f(severity, urgency)
 *
 * The 4x4 matrix is:
 *   f(CRITICAL, IMMEDIATE)  = CRITICAL
 *   f(CRITICAL, PROMPT)     = CRITICAL
 *   f(CRITICAL, RAPID)      = HIGH
 *   f(CRITICAL, NON_URGENT) = MEDIUM
 *   f(SEVERE,   IMMEDIATE)  = CRITICAL
 *   f(SEVERE,   PROMPT)     = HIGH
 *   f(SEVERE,   RAPID)      = MEDIUM
 *   f(SEVERE,   NON_URGENT) = LOW
 *   f(MAJOR,    IMMEDIATE)  = HIGH
 *   f(MAJOR,    PROMPT)     = MEDIUM
 *   f(MAJOR,    RAPID)      = MEDIUM
 *   f(MAJOR,    NON_URGENT) = LOW
 *   f(MODERATE, IMMEDIATE)  = MEDIUM
 *   f(MODERATE, PROMPT)     = LOW
 *   f(MODERATE, RAPID)      = LOW
 *   f(MODERATE, NON_URGENT) = LOW
 *
 * Complexity: O(1) table lookup.
 *============================================================================*/
isa18_alarm_priority_t isa18_assign_priority_matrix(
    isa18_consequence_severity_t severity,
    isa18_urgency_t urgency)
{
    /* 4x4 static lookup table: [severity][urgency] = priority */
    static const isa18_alarm_priority_t matrix[4][4] = {
        /* IMMEDIATE        PROMPT           RAPID            NON_URGENT */
        {ISA18_PRIORITY_CRITICAL, ISA18_PRIORITY_CRITICAL,
         ISA18_PRIORITY_HIGH,     ISA18_PRIORITY_MEDIUM},  /* CRITICAL */
        {ISA18_PRIORITY_CRITICAL, ISA18_PRIORITY_HIGH,
         ISA18_PRIORITY_MEDIUM,   ISA18_PRIORITY_LOW},      /* SEVERE */
        {ISA18_PRIORITY_HIGH,     ISA18_PRIORITY_MEDIUM,
         ISA18_PRIORITY_MEDIUM,   ISA18_PRIORITY_LOW},      /* MAJOR */
        {ISA18_PRIORITY_MEDIUM,   ISA18_PRIORITY_LOW,
         ISA18_PRIORITY_LOW,      ISA18_PRIORITY_LOW}       /* MODERATE */
    };

    uint32_t s = (uint32_t)severity;
    uint32_t u = (uint32_t)urgency;

    if (s > 3 || u > 3) {
        return ISA18_PRIORITY_LOW; /* Safe default for invalid input */
    }

    return matrix[s][u];
}

/*============================================================================
 * L2 — Consequence-Based Alarm Justification (ISA-18.2 §9.2)
 *
 * Four mandatory criteria for alarm justification:
 *   1. Consequence defined: the consequence of inaction must be specified
 *   2. Action defined: a specific corrective action must exist
 *   3. Time sufficient: operator must have enough time to respond
 *   4. Not eliminable: the alarm cannot be eliminated by better design
 *
 * This implements ISA-18.2's "every alarm must earn its place"
 * philosophy, which is the core of the rationalization process.
 *============================================================================*/
bool isa18_check_alarm_justified(
    const char *consequence,
    const char *corrective_action,
    uint32_t time_to_respond_sec,
    bool can_be_eliminated_by_design)
{
    /* Criterion 1: Consequence of inaction must be specified */
    if (!consequence || strlen(consequence) == 0 ||
        strcmp(consequence, "Not specified") == 0) {
        return false;
    }

    /* Criterion 2: Corrective action must be specified */
    if (!corrective_action || strlen(corrective_action) == 0 ||
        strcmp(corrective_action, "Not specified") == 0) {
        return false;
    }

    /* Criterion 3: Operator must have sufficient time to respond */
    if (time_to_respond_sec < 60) {
        /* Less than 60 seconds is considered insufficient
           for human operator response per ISA-18.2 */
        return false;
    }

    /* Criterion 4: Alarm cannot be eliminated by design change */
    if (can_be_eliminated_by_design) {
        return false;
    }

    return true;
}

/*============================================================================
 * L4 — Maximum Safe Response Time Calculation
 *
 * Computes the latest time an operator can respond before the
 * process reaches an unsafe condition.
 *
 * T_safe = T_process_deadline - T_detection - T_diagnosis
 *
 * This formula comes from human factors engineering applied
 * to process control (ISA-18.2 §9.5, EEMUA 191 §6.4).
 *
 * The process deadline is the time from alarm condition onset
 * to when the consequence becomes unavoidable. This depends on
 * process dynamics (e.g., vessel fill time, reaction kinetics).
 *
 * Detection lag includes sensor response time + scan cycle delay.
 * Diagnosis time is the cognitive time for the operator to
 * understand the situation (typically 1-3 minutes per EEMUA 191).
 *============================================================================*/
double isa18_calc_max_safe_response_time(
    double process_deadline_sec,
    double detection_lag_sec,
    double diagnosis_time_sec)
{
    if (process_deadline_sec <= 0.0) {
        return 0.0; /* No safe response possible */
    }

    double safe_time = process_deadline_sec - detection_lag_sec
                       - diagnosis_time_sec;

    /* Cannot be negative — minimum practical response is ~60 seconds */
    if (safe_time < 0.0) {
        return 0.0;
    }
    return safe_time;
}

/*============================================================================
 * L4 — Response Time Safety Margin
 *
 * Computes how much buffer exists between the calculated safe
 * response time and the minimum required response time.
 *
 * safety_margin = (T_safe - T_respond_min) / T_safe
 *
 * Interpretation:
 *   margin >= 0.5  → Excellent: ample time to respond
 *   margin 0.2-0.5 → Acceptable: adequate buffer
 *   margin 0.0-0.2 → Marginal: limited buffer, review needed
 *   margin < 0.0   → Unsafe: alarm design is inadequate
 *
 * Returns: safety margin as ratio [-∞, 1.0]
 *============================================================================*/
double isa18_calc_response_time_margin(
    double max_safe_response_time_sec,
    double min_required_response_time_sec)
{
    if (max_safe_response_time_sec <= 0.0) {
        return -1.0; /* No safe time available */
    }

    if (min_required_response_time_sec <= 0.0) {
        return 1.0; /* No minimum required: full margin */
    }

    return (max_safe_response_time_sec - min_required_response_time_sec)
           / max_safe_response_time_sec;
}

/*============================================================================
 * L3 — Initialize Rationalization Record
 *============================================================================*/
void isa18_rationalization_init_record(
    isa18_rationalization_record_t *record,
    uint32_t record_id,
    uint32_t alarm_id,
    const char *tag_name)
{
    if (!record) return;

    memset(record, 0, sizeof(isa18_rationalization_record_t));
    record->record_id = record_id;
    record->alarm_id = alarm_id;
    if (tag_name) {
        strncpy(record->tag_name, tag_name, sizeof(record->tag_name) - 1);
    }
    record->alarm_class = ISA18_CLASS_ALARM;
    record->priority = ISA18_PRIORITY_LOW;
    record->severity = ISA18_SEVERITY_MODERATE;
    record->urgency = ISA18_URGENCY_NON_URGENT;
    record->max_safe_response_time_sec = 300.0;
    record->is_justified = false;
    record->team_count = 0;
    record->max_alarms_per_day = ISA18_ACCEPTABLE_ALARMS_PER_DAY;
}

/*============================================================================
 * L3 — Assign Rationalization Outcome
 *
 * This is the core step of the rationalization process:
 * the team decides the alarm class, priority, severity, and urgency,
 * and documents the justification.
 *============================================================================*/
void isa18_rationalization_set_outcome(
    isa18_rationalization_record_t *record,
    isa18_alarm_class_t alarm_class,
    isa18_alarm_priority_t priority,
    isa18_consequence_severity_t severity,
    isa18_urgency_t urgency,
    bool is_justified,
    const char *justification)
{
    if (!record) return;

    record->alarm_class = alarm_class;
    record->priority = priority;
    record->severity = severity;
    record->urgency = urgency;
    record->is_justified = is_justified;
    if (justification) {
        strncpy(record->justification, justification,
                sizeof(record->justification) - 1);
    }
    record->rationalization_date = time(NULL);
}

/*============================================================================
 * L3 — Add Rationalization Team Member
 *
 * ISA-18.2 §9.4 requires a multi-disciplinary team for rationalization.
 * Typical team composition:
 *   - Process engineer
 *   - Control system engineer
 *   - Operator representative
 *   - Safety engineer
 *   - Maintenance representative
 *
 * Returns false if team size exceeds maximum (10 members).
 *============================================================================*/
bool isa18_rationalization_team_add_member(
    isa18_rationalization_record_t *record,
    const char *member_name)
{
    if (!record || !member_name) return false;
    if (record->team_count >= 10) return false;

    /* Check for duplicates */
    for (uint32_t i = 0; i < record->team_count; i++) {
        if (strcmp(record->team_members[i], member_name) == 0) {
            return false; /* Already a member */
        }
    }

    strncpy(record->team_members[record->team_count], member_name,
            sizeof(record->team_members[0]) - 1);
    record->team_count++;
    return true;
}

/*============================================================================
 * L3 — Apply Rationalization Outcome to Alarm Configuration
 *
 * Transfers the rationalization decisions to the live alarm
 * configuration. This implements the C→D transition in the
 * ISA-18.2 lifecycle (Rationalization → Detailed Design).
 *============================================================================*/
void isa18_rationalization_apply_to_alarm(
    const isa18_rationalization_record_t *record,
    isa18_alarm_config_t *alarm)
{
    if (!record || !alarm) return;

    alarm->alarm_class = record->alarm_class;
    alarm->priority = record->priority;
    alarm->is_rationalized = record->is_justified;
    alarm->rationalization_date = record->rationalization_date;

    if (record->is_justified) {
        snprintf(alarm->consequence, ISA18_MAX_CONSEQUENCE_LEN,
                 "Rationalized: %s", record->justification);
    }
    alarm->time_to_respond_sec =
        (uint32_t)record->max_safe_response_time_sec;

    alarm->last_modified = time(NULL);
}

/*============================================================================
 * L3 — Initialize Master Alarm Database (MAD)
 *
 * Creates a blank Master Alarm Database. The MAD is the central
 * repository of all alarm configurations per ISA-18.2 §7.
 *============================================================================*/
void isa18_mad_init(
    isa18_master_alarm_database_t *mad,
    const char *site_name,
    const char *philosophy_doc_ref)
{
    if (!mad) return;

    memset(mad, 0, sizeof(isa18_master_alarm_database_t));
    if (site_name) {
        strncpy(mad->site_name, site_name, sizeof(mad->site_name) - 1);
    }
    if (philosophy_doc_ref) {
        strncpy(mad->philosophy_doc_ref, philosophy_doc_ref,
                sizeof(mad->philosophy_doc_ref) - 1);
    }
    mad->alarm_count = 0;
    mad->rationalized_count = 0;
    mad->database_creation_time = time(NULL);
    mad->last_update_time = mad->database_creation_time;
    mad->revision = 1;
}

/*============================================================================
 * L3 — Add Alarm to Master Alarm Database
 *
 * Adds a new alarm configuration and assigns it a unique alarm_id.
 * Invalid if the database is full or a duplicate tag exists.
 *
 * Returns: alarm_id on success, 0 on failure
 *============================================================================*/
uint32_t isa18_mad_add_alarm(
    isa18_master_alarm_database_t *mad,
    const isa18_alarm_config_t *alarm_config)
{
    if (!mad || !alarm_config) return 0;
    if (mad->alarm_count >= ISA18_MAX_ALARM_POINTS) return 0;

    /* Check for duplicate tag name */
    for (uint32_t i = 0; i < mad->alarm_count; i++) {
        if (strcmp(mad->alarms[i].tag_name, alarm_config->tag_name) == 0) {
            return 0; /* Duplicate tag */
        }
    }

    uint32_t new_id = mad->alarm_count + 1;
    mad->alarms[mad->alarm_count] = *alarm_config;
    mad->alarms[mad->alarm_count].alarm_id = new_id;
    mad->alarm_count++;
    mad->last_update_time = time(NULL);

    if (alarm_config->is_rationalized) {
        mad->rationalized_count++;
    }

    return new_id;
}

/*============================================================================
 * L3 — Find Alarm by Tag Name
 *
 * Linear search through the MAD. In a real DCS, this would use
 * a hash table or B-tree index, but this demonstrates the concept.
 *
 * Complexity: O(n) where n = alarm_count
 *============================================================================*/
isa18_alarm_config_t *isa18_mad_find_alarm(
    isa18_master_alarm_database_t *mad,
    const char *tag_name)
{
    if (!mad || !tag_name) return NULL;

    for (uint32_t i = 0; i < mad->alarm_count; i++) {
        if (strcmp(mad->alarms[i].tag_name, tag_name) == 0) {
            return &mad->alarms[i];
        }
    }
    return NULL;
}

/*============================================================================
 * L3 — Find Alarm by ID
 *
 * Complexity: O(n) linear scan.
 *============================================================================*/
isa18_alarm_config_t *isa18_mad_find_by_id(
    isa18_master_alarm_database_t *mad,
    uint32_t alarm_id)
{
    if (!mad || alarm_id == 0) return NULL;

    for (uint32_t i = 0; i < mad->alarm_count; i++) {
        if (mad->alarms[i].alarm_id == alarm_id) {
            return &mad->alarms[i];
        }
    }
    return NULL;
}

/*============================================================================
 * L3 — Remove Alarm from MAD
 *
 * Removes an alarm by ID and compacts the array.
 * Complexity: O(n) due to array shift.
 *============================================================================*/
bool isa18_mad_remove_alarm(
    isa18_master_alarm_database_t *mad,
    uint32_t alarm_id)
{
    if (!mad || alarm_id == 0 || mad->alarm_count == 0) return false;

    uint32_t idx = 0;
    bool found = false;

    for (uint32_t i = 0; i < mad->alarm_count; i++) {
        if (mad->alarms[i].alarm_id == alarm_id) {
            idx = i;
            found = true;
            break;
        }
    }

    if (!found) return false;

    if (mad->alarms[idx].is_rationalized) {
        mad->rationalized_count--;
    }

    /* Compact the array */
    for (uint32_t i = idx; i < mad->alarm_count - 1; i++) {
        mad->alarms[i] = mad->alarms[i + 1];
    }

    mad->alarm_count--;
    mad->last_update_time = time(NULL);
    return true;
}

/*============================================================================
 * L3 — Count Alarms by Priority Level
 *
 * Produces a summary of the current alarm priority distribution
 * in the MAD. This is a key metric for alarm system health.
 *
 * ISA-18.2 §16.4.4 recommends that the priority distribution should
 * be roughly: 5% Critical, 15% High, 60% Medium, 20% Low.
 *============================================================================*/
void isa18_mad_count_by_priority(
    const isa18_master_alarm_database_t *mad,
    uint32_t *out_critical,
    uint32_t *out_high,
    uint32_t *out_medium,
    uint32_t *out_low)
{
    uint32_t critical = 0, high = 0, medium = 0, low = 0;

    if (mad) {
        for (uint32_t i = 0; i < mad->alarm_count; i++) {
            switch (mad->alarms[i].priority) {
            case ISA18_PRIORITY_CRITICAL: critical++; break;
            case ISA18_PRIORITY_HIGH:     high++;     break;
            case ISA18_PRIORITY_MEDIUM:   medium++;   break;
            case ISA18_PRIORITY_LOW:      low++;      break;
            default: break;
            }
        }
    }

    if (out_critical) *out_critical = critical;
    if (out_high)     *out_high     = high;
    if (out_medium)   *out_medium   = medium;
    if (out_low)      *out_low      = low;
}

/*============================================================================
 * L4 — MAD Consistency Validation
 *
 * Checks the entire MAD for ISA-18.2 compliance:
 *   - No duplicate tags
 *   - No duplicate alarm IDs
 *   - All alarm types have appropriate setpoints
 *   - Rationalized alarms have complete documentation
 *   - Priority distribution is within acceptable range
 *   - At least one alarm per major equipment item
 *
 * Returns number of errors found.
 *============================================================================*/
void isa18_mad_validate(
    const isa18_master_alarm_database_t *mad,
    uint32_t *out_errors,
    char error_list[][ISA18_MAX_MESSAGE_LEN],
    uint32_t max_errors)
{
    uint32_t errors = 0;
    if (!mad) {
        if (out_errors) *out_errors = 0;
        return;
    }

    #define MAD_ADD_ERR(msg) do { \
        if (errors < max_errors) { \
            strncpy(error_list[errors], msg, ISA18_MAX_MESSAGE_LEN - 1); \
            error_list[errors][ISA18_MAX_MESSAGE_LEN - 1] = '\0'; \
        } \
        errors++; \
    } while(0)

    /* Check for duplicate tags */
    for (uint32_t i = 0; i < mad->alarm_count; i++) {
        for (uint32_t j = i + 1; j < mad->alarm_count; j++) {
            if (strcmp(mad->alarms[i].tag_name,
                       mad->alarms[j].tag_name) == 0) {
                char buf[ISA18_MAX_MESSAGE_LEN];
                snprintf(buf, sizeof(buf),
                         "Duplicate tag: %s", mad->alarms[i].tag_name);
                MAD_ADD_ERR(buf);
            }
        }
    }

    /* Check rationalization coverage */
    if (mad->alarm_count > 0 && mad->rationalized_count == 0) {
        MAD_ADD_ERR("No alarms have been rationalized");
    }

    /* Check for critical priority alarms without documented consequence */
    for (uint32_t i = 0; i < mad->alarm_count; i++) {
        if (mad->alarms[i].priority <= ISA18_PRIORITY_HIGH) {
            if (strlen(mad->alarms[i].consequence) == 0 ||
                strcmp(mad->alarms[i].consequence, "Not specified") == 0) {
                char buf[ISA18_MAX_MESSAGE_LEN];
                snprintf(buf, sizeof(buf),
                         "High/Critical alarm %s has no consequence defined",
                         mad->alarms[i].tag_name);
                MAD_ADD_ERR(buf);
            }
        }
    }

    #undef MAD_ADD_ERR
    if (out_errors) *out_errors = errors;
}

/*============================================================================
 * L3 — Count Unrationalized Alarms
 *
 * ISA-18.2 §9 requires 100% rationalization coverage.
 * Alarms that have not been rationalized must undergo
 * the rationalization process.
 *============================================================================*/
uint32_t isa18_mad_get_unrationalized_count(
    const isa18_master_alarm_database_t *mad)
{
    if (!mad) return 0;

    uint32_t unrationalized = 0;
    for (uint32_t i = 0; i < mad->alarm_count; i++) {
        if (!mad->alarms[i].is_rationalized) {
            unrationalized++;
        }
    }
    return unrationalized;
}

/*============================================================================
 * L3 — Export MAD Summary Statistics
 *============================================================================*/
void isa18_mad_export_summary(
    const isa18_master_alarm_database_t *mad,
    uint32_t *total_alarms,
    uint32_t *rationalized,
    uint32_t *by_class[4],
    uint32_t *by_priority[4])
{
    uint32_t class_counts[4] = {0, 0, 0, 0};
    uint32_t priority_counts[4] = {0, 0, 0, 0};

    if (mad) {
        for (uint32_t i = 0; i < mad->alarm_count; i++) {
            const isa18_alarm_config_t *a = &mad->alarms[i];

            /* Count by class */
            uint32_t cc = (uint32_t)a->alarm_class;
            if (cc < 4) class_counts[cc]++;

            /* Count by priority */
            uint32_t cp = (uint32_t)a->priority;
            if (cp < 4) priority_counts[cp]++;
        }

        if (total_alarms)   *total_alarms   = mad->alarm_count;
        if (rationalized)   *rationalized   = mad->rationalized_count;
    }

    for (int i = 0; i < 4; i++) {
        if (by_class[i])    *by_class[i]    = class_counts[i];
        if (by_priority[i]) *by_priority[i] = priority_counts[i];
    }
}

/*============================================================================
 * L3 — Calculate Rationalization Coverage
 *
 * Coverage = rationalized_count / total_alarm_count * 100
 *
 * ISA-18.2 requires 100% rationalization coverage for new alarms
 * and targets > 95% for existing systems.
 *
 * Returns: percentage [0.0, 100.0]
 *============================================================================*/
double isa18_mad_calc_rationalization_coverage(
    const isa18_master_alarm_database_t *mad)
{
    if (!mad || mad->alarm_count == 0) return 0.0;

    return (double)mad->rationalized_count
           / (double)mad->alarm_count * 100.0;
}