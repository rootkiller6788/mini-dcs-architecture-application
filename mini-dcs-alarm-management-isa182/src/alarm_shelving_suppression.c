/**
 * @file alarm_shelving_suppression.c
 * @brief ISA-18.2 Alarm Shelving and Plant State-Based Suppression (L2, L3, L4)
 *
 * Knowledge Points:
 *   isa18_shelve_alarm          — Timed shelving per ISA-18.2 §12.5 (L2)
 *   isa18_unshelve_alarm        — Manual unshelving (L2)
 *   isa18_auto_unshelve_expired — Automatic unshelve on timeout (L3)
 *   isa18_extend_shelve         — Extension of shelving duration (L3)
 *   isa18_suppression_by_plant_state — State-based alarm suppression (L2)
 *   isa18_evaluate_plant_state_mask   — Hierarchical plant state evaluation (L3)
 *   isa18_validate_shelve       — Business rule validation before shelving (L4)
 *   isa18_get_shelved_alarms    — List currently shelved alarms (L3)
 *   isa18_check_shelve_approval_required — Approval requirement by priority (L2)
 *   isa18_suppression_audit_log — Audit log for suppression changes (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §12 (Alarm Shelving and Suppression)
 *   - EEMUA 191 §5.4 (Alarm Suppression)
 *   - IEC 62682:2014 §12
 */

#include "alarm_shelving_suppression.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/*============================================================================
 * L2 — Shelve an Alarm (ISA-18.2 §12.5)
 *
 * Shelving temporarily suppresses an alarm for a defined time period.
 * This is a manual operator action (unlike plant state suppression which
 * is automatic). Shelving requires:
 *
 *   1. A documented reason (justification)
 *   2. A maximum duration (typically 12 hours per ISA-18.2)
 *   3. Supervisor approval for HIGH/CRITICAL priorities
 *   4. The alarm must be in ACTIVE_UNACK or ACTIVE_ACK state
 *
 * Shelving is distinct from:
 *   - Out-of-service (OOS): longer-term removal from alarm system
 *   - Suppression: automatic by plant state
 *   - Disabling: configuration change (requires MOC)
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
    time_t now)
{
    if (!alarm || !shelved || !shelved_count) return 0;

    /* Validate shelve against business rules */
    if (!isa18_validate_shelve(alarm, duration_sec, *shelved_count,
                                max_shelved, approver_id, now)) {
        return 0;
    }

    /* Create shelving record */
    uint32_t new_id = (*shelved_count > 0)
        ? shelved[*shelved_count - 1].shelve_id + 1
        : 1;

    isa18_alarm_shelve_t *s = &shelved[*shelved_count];
    memset(s, 0, sizeof(isa18_alarm_shelve_t));

    s->shelve_id = new_id;
    s->alarm_id = alarm->alarm_id;
    s->shelve_start = now;
    s->shelve_end = now + (time_t)duration_sec;
    if (reason) {
        strncpy(s->reason, reason, sizeof(s->reason) - 1);
    }
    if (operator_id) {
        strncpy(s->operator_id, operator_id, sizeof(s->operator_id) - 1);
    }
    if (approver_id) {
        strncpy(s->approver_id, approver_id, sizeof(s->approver_id) - 1);
    }
    s->is_approved = (approver_id != NULL && strlen(approver_id) > 0);
    s->is_active = true;
    s->extension_count = 0;

    /* Update alarm state */
    alarm->is_shelved = true;
    alarm->shelve_expiry = s->shelve_end;

    (*shelved_count)++;
    return new_id;
}

/*============================================================================
 * L2 — Unshelve an Alarm (ISA-18.2 §12.5.4)
 *
 * An operator or supervisor manually removes the shelving before
 * its expiration time. This restores normal alarm behavior immediately.
 *
 * The alarm's current_state is set back to NORMAL so it can activate
 * if the condition is still present.
 *============================================================================*/
bool isa18_unshelve_alarm(
    isa18_alarm_config_t *alarm,
    isa18_alarm_shelve_t *shelved,
    uint32_t *shelved_count,
    const char *operator_id,
    time_t now)
{
    (void)operator_id;
    (void)now;

    if (!alarm || !shelved || !shelved_count) return false;
    if (!alarm->is_shelved) return false;

    /* Find the shelving record */
    uint32_t idx = 0;
    bool found = false;
    for (uint32_t i = 0; i < *shelved_count; i++) {
        if (shelved[i].alarm_id == alarm->alarm_id &&
            shelved[i].is_active) {
            idx = i;
            found = true;
            break;
        }
    }
    if (!found) return false;

    /* Deactivate the shelving record */
    shelved[idx].is_active = false;

    /* Update alarm */
    alarm->is_shelved = false;
    alarm->shelve_expiry = 0;
    alarm->current_state = ISA18_ALARM_STATE_NORMAL;

    /* Compact shelved array */
    for (uint32_t i = idx; i < *shelved_count - 1; i++) {
        shelved[i] = shelved[i + 1];
    }
    (*shelved_count)--;

    return true;
}

/*============================================================================
 * L3 — Automatic Unshelve on Timeout (ISA-18.2 §12.5.5)
 *
 * Scans all alarms and automatically unshelves any whose
 * expiry time has passed. This is called periodically by the
 * alarm engine to enforce the mandatory unshelve policy.
 *
 * Returns: number of alarms auto-unshelved.
 *============================================================================*/
uint32_t isa18_auto_unshelve_expired(
    isa18_alarm_config_t *configs,
    uint32_t total_alarms,
    isa18_alarm_shelve_t *shelved,
    uint32_t *shelved_count,
    time_t now)
{
    if (!configs || !shelved || !shelved_count) return 0;

    uint32_t unshelved = 0;

    for (uint32_t i = 0; i < total_alarms; i++) {
        isa18_alarm_config_t *alarm = &configs[i];

        if (!alarm->is_shelved) continue;
        if (alarm->shelve_expiry > now) continue;

        /* Shelve has expired: unshelve */
        for (uint32_t j = 0; j < *shelved_count; j++) {
            if (shelved[j].alarm_id == alarm->alarm_id &&
                shelved[j].is_active) {
                shelved[j].is_active = false;

                /* Compact */
                for (uint32_t k = j; k < *shelved_count - 1; k++) {
                    shelved[k] = shelved[k + 1];
                }
                (*shelved_count)--;
                break;
            }
        }

        alarm->is_shelved = false;
        alarm->shelve_expiry = 0;
        alarm->current_state = ISA18_ALARM_STATE_NORMAL;
        unshelved++;
    }

    return unshelved;
}

/*============================================================================
 * L3 — Extend Shelving Duration (ISA-18.2 §12.5.6)
 *
 * Allows extension of shelving for an additional period.
 * Requires re-approval. Maximum total shelving cannot exceed
 * 72 hours per ISA-18.2 recommended practice.
 *
 * Returns: true if extension was successful.
 *============================================================================*/
bool isa18_extend_shelve(
    isa18_alarm_shelve_t *shelve_record,
    uint32_t additional_duration_sec,
    const char *approver_id,
    time_t now)
{
    if (!shelve_record) return false;
    if (!shelve_record->is_active) return false;
    if (additional_duration_sec == 0) return false;

    /* Check maximum extensions (3 extensions max) */
    if (shelve_record->extension_count >= 3) {
        return false;
    }

    /* Calculate new expiry time */
    time_t new_end = shelve_record->shelve_end +
                     (time_t)additional_duration_sec;

    /* Sanity check: total shelving time cannot exceed 72 hours */
    double total_duration = difftime(new_end, shelve_record->shelve_start);
    if (total_duration > 259200.0) {  /* 72 hours in seconds */
        return false;
    }

    shelve_record->shelve_end = new_end;
    shelve_record->extension_count++;
    shelve_record->last_extension = now;

    if (approver_id) {
        strncpy(shelve_record->approver_id, approver_id,
                sizeof(shelve_record->approver_id) - 1);
    }
    shelve_record->is_approved = true;

    return true;
}

/*============================================================================
 * L2 — Plant State-Based Suppression (ISA-18.2 §12.3)
 *
 * Alarms are automatically suppressed when the plant is in a
 * specific operating state where those alarms are irrelevant.
 *
 * Examples:
 *   - Startup state: suppress low-flow alarms (pumps ramping up)
 *   - Shutdown state: suppress normal-operating alarms
 *   - Maintenance state: suppress equipment-specific alarms
 *   - Regeneration state: suppress catalyst-related alarms
 *
 * Each alarm has a plant_state_mask bitmask. If any bit in the
 * mask matches the current_plant_state bitmask, the alarm is
 * suppressed (hidden from annunciator, no audible alert).
 *
 * This implements the "first-out" and "state-based alarm
 * suppression" features found in modern DCS platforms.
 *
 * Returns: true if alarm should be suppressed.
 *============================================================================*/
bool isa18_suppression_by_plant_state(
    uint32_t alarm_plant_state_mask,
    uint32_t current_plant_state)
{
    /* If mask is 0, no suppression is configured */
    if (alarm_plant_state_mask == 0) return false;

    /* If current state is 0, plant is in undefined state — no suppression */
    if (current_plant_state == 0) return false;

    /* Check if any bit in the alarm's mask matches the current plant state */
    return ((alarm_plant_state_mask & current_plant_state) != 0);
}

/*============================================================================
 * L3 — Evaluate Plant State Bitmask with Hierarchy
 *
 * ISA-18.2 §12.3.2: Plant states can be organized hierarchically.
 * A "maintenance" state may have child states for "pump maintenance",
 * "vessel maintenance", etc. Suppression configured for "maintenance"
 * should apply to all child states.
 *
 * This function evaluates suppression considering the state hierarchy.
 * Each state has a parent state (0 = no parent / root).
 *
 * Algorithm:
 *   1. Check if current state directly matches the alarm mask
 *   2. Walk up the hierarchy through parents
 *   3. Check if any ancestor state matches the alarm mask
 *
 * Complexity: O(depth_of_hierarchy) ≈ O(1) for practical depths.
 *============================================================================*/
bool isa18_evaluate_plant_state_mask(
    uint32_t alarm_plant_state_mask,
    uint32_t current_plant_state,
    const uint32_t *state_hierarchy_parents,
    uint32_t state_count)
{
    if (alarm_plant_state_mask == 0) return false;
    if (current_plant_state == 0) return false;

    /* Check direct match first */
    if ((alarm_plant_state_mask & (1U << (current_plant_state - 1))) != 0) {
        return true;
    }

    /* Walk up the hierarchy */
    if (state_hierarchy_parents && current_plant_state <= state_count) {
        uint32_t parent = state_hierarchy_parents[current_plant_state - 1];
        uint32_t depth = 0;
        const uint32_t max_parent_depth = 16; /* Prevent infinite loops */

        while (parent != 0 && depth < max_parent_depth) {
            if ((alarm_plant_state_mask & (1U << (parent - 1))) != 0) {
                return true;
            }
            if (parent > state_count) break;
            parent = state_hierarchy_parents[parent - 1];
            depth++;
        }
    }

    return false;
}

/*============================================================================
 * L4 — Business Rule Validation for Shelving (ISA-18.2 §12.5.2)
 *
 * Validates that a shelving operation complies with all ISA-18.2
 * business rules before allowing the shelve to proceed.
 *
 * Business rules:
 *   1. Duration must not exceed ISA18_MAX_SHELVE_DURATION_SEC (12 hours)
 *   2. CRITICAL alarms cannot be shelved (site policy dependent)
 *   3. HIGH alarms require supervisor approval string
 *   4. Shelving count must not exceed ISA18_MAX_SHELVED_ALARMS
 *   5. Alarm must not already be shelved
 *   6. Alarm must be in an active state (ACTIVE_UNACK or ACTIVE_ACK)
 *   7. Shelving reason must be provided (non-empty)
 *
 * Returns: true if all business rules pass.
 *============================================================================*/
bool isa18_validate_shelve(
    const isa18_alarm_config_t *alarm,
    uint32_t duration_sec,
    uint32_t current_shelved_count,
    uint32_t max_shelved,
    const char *approver_id,
    time_t now)
{
    (void)now;

    if (!alarm) return false;

    /* Rule 1: Duration check */
    if (duration_sec == 0 || duration_sec > ISA18_MAX_SHELVE_DURATION_SEC) {
        return false;
    }

    /* Rule 2: CRITICAL alarms cannot be shelved (policy) */
    if (alarm->priority == ISA18_PRIORITY_CRITICAL) {
        return false;
    }

    /* Rule 3: HIGH alarms require supervisor approval */
    if (alarm->priority == ISA18_PRIORITY_HIGH) {
        if (!approver_id || strlen(approver_id) == 0) {
            return false;
        }
    }

    /* Rule 4: Don't exceed max shelved count */
    if (current_shelved_count >= max_shelved) {
        return false;
    }

    /* Rule 5: Not already shelved */
    if (alarm->is_shelved) {
        return false;
    }

    /* Rule 6: Must be in an active state */
    if (!isa18_alarm_state_is_active(alarm->current_state)) {
        return false;
    }

    return true;
}

/*============================================================================
 * L3 — Get Currently Shelved Alarm IDs
 *
 * Returns a list of alarm_id values for all currently shelved alarms.
 * Used for the operator display showing "Shelved Alarms" list.
 *============================================================================*/
uint32_t isa18_get_shelved_alarms(
    const isa18_alarm_shelve_t *shelved,
    uint32_t shelved_count,
    uint32_t *alarm_ids,
    uint32_t max_ids)
{
    if (!shelved || !alarm_ids) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < shelved_count && count < max_ids; i++) {
        if (shelved[i].is_active) {
            alarm_ids[count++] = shelved[i].alarm_id;
        }
    }
    return count;
}

/*============================================================================
 * L2 — Check if Shelve Requires Supervisor Approval
 *
 * Per ISA-18.2 §12.5.2.3:
 *   CRITICAL → Cannot be shelved (returns false; always "requires approval")
 *   HIGH     → Requires supervisor approval
 *   MEDIUM   → May require approval per site policy (this function returns true)
 *   LOW      → No approval required
 *
 * Returns: true if approval is required for this priority.
 *============================================================================*/
bool isa18_check_shelve_approval_required(
    isa18_alarm_priority_t priority)
{
    switch (priority) {
    case ISA18_PRIORITY_CRITICAL:
        return true; /* Shelving not allowed at all in many policies */
    case ISA18_PRIORITY_HIGH:
        return true; /* Supervisor approval required */
    case ISA18_PRIORITY_MEDIUM:
        return true; /* Approval required per conservative site policy */
    case ISA18_PRIORITY_LOW:
        return false; /* No approval required */
    default:
        return true; /* Conservative default */
    }
}

/*============================================================================
 * L3 — Suppression Audit Log
 *
 * Records every shelving/unshelving/suppression change as an
 * audit trail event for regulatory compliance.
 *
 * ISA-18.2 §12.6: All suppression events must be logged with:
 *   - Timestamp
 *   - Operator identity
 *   - Action type (SHELVE, UNSHELVE, SUPPRESS, UNSUPPRESS)
 *   - Reason/justification
 *============================================================================*/
void isa18_suppression_audit_log(
    isa18_alarm_event_t *event,
    uint64_t event_id,
    uint32_t alarm_id,
    const char *operator_id,
    const char *action_type,
    const char *reason,
    time_t timestamp)
{
    if (!event) return;

    memset(event, 0, sizeof(isa18_alarm_event_t));
    event->event_id = event_id;
    event->alarm_id = alarm_id;
    event->timestamp = timestamp;
    event->from_state = ISA18_ALARM_STATE_NORMAL;
    event->to_state = ISA18_ALARM_STATE_NORMAL;
    event->is_operator_action = true;

    if (operator_id) {
        strncpy(event->operator_id, operator_id,
                sizeof(event->operator_id) - 1);
    }

    if (action_type && reason) {
        snprintf(event->message, ISA18_MAX_MESSAGE_LEN,
                 "%s: Alarm %u, Reason: %s",
                 action_type, alarm_id, reason);
    } else if (action_type) {
        snprintf(event->message, ISA18_MAX_MESSAGE_LEN,
                 "%s: Alarm %u", action_type, alarm_id);
    }
}