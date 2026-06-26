/**
 * @file alarm_shelving_suppression.h
 * @brief ISA-18.2 Alarm Shelving and Plant State-Based Suppression (L2, L3)
 *
 * Knowledge Points:
 *   isa18_shelve_alarm          — Timed shelving per ISA-18.2 §12.5 (L2)
 *   isa18_unshelve_alarm        — Manual unshelving (L2)
 *   isa18_auto_unshelve_expired — Automatic unshelve on timeout (L3)
 *   isa18_extend_shelve         — Extension of shelving duration (L3)
 *   isa18_suppression_by_plant_state — State-based suppression per ISA-18.2 §12.3 (L2)
 *   isa18_evaluate_plant_state_mask   — Plant state bitmask evaluation (L3)
 *   isa18_validate_shelve       — Business rule validation before shelving (L4)
 *   isa18_get_shelved_alarms    — List all currently shelved alarms (L3)
 *   isa18_check_shelve_approval — Mandatory supervisor approval check (L2)
 *   isa18_suppression_audit_log — Record suppression changes for audit (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §12 (Alarm Shelving and Suppression)
 *   - EEMUA 191 §5.4 (Alarm Suppression)
 *   - IEC 62682:2014 §12
 */

#ifndef ALARM_SHELVING_SUPPRESSION_H
#define ALARM_SHELVING_SUPPRESSION_H

#include "alarm_management_types.h"

/*============================================================================
 * L2 — Alarm Shelving (ISA-18.2 §12.5)
 *
 * Shelving is the manual, intentional suppression of an alarm for
 * a defined, limited time period. Unlike suppression (which is
 * automatic based on plant state), shelving requires:
 *   1. Operator action
 *   2. Justification reason documented
 *   3. Supervisor approval (for some priorities)
 *   4. Automatic unshelving after timeout
 *
 * ISA-18.2 §12.5.3: Maximum initial shelf duration is 12 hours.
 * Extensions require re-approval.
 *
 * Parameters:
 *   alarm    — the alarm to shelve
 *   shelved  — shelving records array in runtime system
 *   count    — current shelved count
 *   max_count — max allowed shelved alarms
 *   reason   — justification text
 *   operator_id — operator performing shelving
 *   approver_id — supervisor approving (can be empty for LOW priority)
 *   duration_sec — how long to shelve (max ISA18_MAX_SHELVE_DURATION_SEC)
 *   now      — current time
 *
 * Returns: shelve_id on success, 0 on failure
 *============================================================================*/
uint32_t isa18_shelve_alarm(
    isa18_alarm_config_t *alarm,
    isa18_alarm_shelve_t *shelved,
    uint32_t *shelved_count,
    uint32_t max_shelved,
    const char *reason,
    const char *operator_id,
    const char *approver_id,
    uint32_t duration_sec,
    time_t now);

/*============================================================================
 * L2 — Manual Unshelving (ISA-18.2 §12.5.4)
 *
 * An operator or supervisor manually removes the shelve before
 * its expiration. This restores normal alarm behavior.
 *
 * Returns true on success.
 *============================================================================*/
bool isa18_unshelve_alarm(
    isa18_alarm_config_t *alarm,
    isa18_alarm_shelve_t *shelved,
    uint32_t *shelved_count,
    const char *operator_id,
    time_t now);

/*============================================================================
 * L3 — Automatic Unshelve on Timeout (ISA-18.2 §12.5.5)
 *
 * Scans all shelved alarms and unshelves any whose expiry time
 * has passed. This should be called periodically by the alarm engine.
 *
 * Returns: number of alarms auto-unshelved
 *============================================================================*/
uint32_t isa18_auto_unshelve_expired(
    isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    isa18_alarm_shelve_t *shelved,
    uint32_t *shelved_count,
    time_t now);

/*============================================================================
 * L3 — Extend Shelving Duration (ISA-18.2 §12.5.6)
 *
 * Extends the shelving period for an additional duration.
 * Requires re-approval. Maximum total shelving time cannot
 * exceed 72 hours (3 extensions of 12 hours each).
 *
 * Returns true on success.
 *============================================================================*/
bool isa18_extend_shelve(
    isa18_alarm_shelve_t *shelve_record,
    uint32_t additional_duration_sec,
    const char *approver_id,
    time_t now);

/*============================================================================
 * L2 — Plant State-Based Suppression (ISA-18.2 §12.3)
 *
 * Alarms are automatically suppressed when the plant is in a
 * specific operating state where those alarms are irrelevant.
 *
 * Examples:
 *   - Pump trip alarms suppressed during maintenance
 *   - High temperature alarms suppressed during startup
 *   - Low flow alarms suppressed when pump is stopped
 *
 * Plant states are encoded as a bitmask (64 possible states).
 * Each alarm has a plant_state_mask: if any bit in the mask
 * matches the current plant_state, the alarm is suppressed.
 *
 * Returns true if alarm should be suppressed.
 *============================================================================*/
bool isa18_suppression_by_plant_state(
    uint32_t alarm_plant_state_mask,
    uint32_t current_plant_state);

/*============================================================================
 * L3 — Evaluate Plant State Bitmask
 *
 * ISA-18.2 §12.3.2: Plant states are hierarchical. This function
 * evaluates whether a state or any of its parent states match
 * the suppression mask. Handles up to 64 plant states.
 *
 * Returns true if any suppression condition is met.
 *============================================================================*/
bool isa18_evaluate_plant_state_mask(
    uint32_t alarm_plant_state_mask,
    uint32_t current_plant_state,
    const uint32_t *state_hierarchy_parents,
    uint32_t state_count);

/*============================================================================
 * L4 — Business Rule Validation for Shelving (ISA-18.2 §12.5.2)
 *
 * Validates that a shelving operation meets all business rules:
 *   1. Duration does not exceed maximum
 *   2. CRITICAL alarms cannot be shelved (in most policies)
 *   3. HIGH alarms require supervisor approval
 *   4. Shelving count does not exceed maximum
 *   5. Alarm is not already shelved
 *   6. Alarm state allows shelving (ACTIVE_UNACK, ACTIVE_ACK only)
 *
 * Returns true if shelving is allowed.
 *============================================================================*/
bool isa18_validate_shelve(
    const isa18_alarm_config_t *alarm,
    uint32_t duration_sec,
    uint32_t current_shelved_count,
    uint32_t max_shelved,
    const char *approver_id,
    time_t now);

/*============================================================================
 * L3 — Get Currently Shelved Alarms
 *
 * Returns list of alarm IDs that are currently shelved.
 *============================================================================*/
uint32_t isa18_get_shelved_alarms(
    const isa18_alarm_shelve_t *shelved,
    uint32_t shelved_count,
    uint32_t *alarm_ids,
    uint32_t max_ids);

/*============================================================================
 * L2 — Check Shelve Supervisor Approval (ISA-18.2 §12.5.2.3)
 *
 * HIGH and CRITICAL priority alarms require supervisor approval
 * before shelving. MEDIUM may require approval per site policy.
 * LOW typically does not require approval.
 *
 * Returns true if approval is required for this priority.
 *============================================================================*/
bool isa18_check_shelve_approval_required(
    isa18_alarm_priority_t priority);

/*============================================================================
 * L3 — Suppression Audit Log Generation
 *
 * Every suppression/shelving change must be recorded for audit.
 * ISA-18.2 §12.6 requires tracking of all suppression events.
 *============================================================================*/
void isa18_suppression_audit_log(
    isa18_alarm_event_t *event,
    uint64_t event_id,
    uint32_t alarm_id,
    const char *operator_id,
    const char *action_type,
    const char *reason,
    time_t timestamp);

#endif /* ALARM_SHELVING_SUPPRESSION_H */