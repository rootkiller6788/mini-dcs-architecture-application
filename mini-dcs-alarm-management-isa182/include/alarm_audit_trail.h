/**
 * @file alarm_audit_trail.h
 * @brief ISA-18.2 Alarm Audit Trail — Event Logging, Operator Action Recording,
 *        Management of Change, and Regulatory Compliance (L3, L7)
 *
 * Knowledge Points:
 *   isa18_audit_init             — Initialize audit trail system (L3)
 *   isa18_audit_log_event        — Log alarm event to audit trail (L3)
 *   isa18_audit_query_by_time    — Query audit trail by time range (L3)
 *   isa18_audit_query_by_alarm   — Query audit trail by alarm ID (L3)
 *   isa18_audit_query_by_operator — Query audit trail by operator (L3)
 *   isa18_audit_export_csv       — Export audit trail as CSV (L7)
 *   isa18_audit_moc_record       — Record management of change action (L7)
 *   isa18_audit_compliance_check — Check regulatory compliance (L4)
 *   isa18_audit_sign              — Digital signature on audit entries (L3)
 *   isa18_audit_verify_chain      — Verify audit trail integrity (L4)
 *   isa18_audit_generate_regulatory_report — Generate regulatory report (L7)
 *   isa18_audit_operator_shift_summary     — Per-operator shift summary (L7)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §17 (Audit)
 *   - IEC 62682:2014 §17
 *   - FDA 21 CFR Part 11 (Electronic Records, Electronic Signatures)
 *   - ISA-18.2 Alarm Management Audit Protocol
 */

#ifndef ALARM_AUDIT_TRAIL_H
#define ALARM_AUDIT_TRAIL_H

#include "alarm_management_types.h"

/*============================================================================
 * ISA-18.2 Audit Trail Constants
 *============================================================================*/

/** Maximum audit trail entries in memory */
#define ISA18_AUDIT_MAX_ENTRIES           100000U

/** Maximum entries returned by a single query */
#define ISA18_AUDIT_MAX_QUERY_RESULTS       1000U

/** Audit trail ring buffer size for rolling storage */
#define ISA18_AUDIT_RING_BUFFER_SIZE       10000U

/** Maximum CSV export line length */
#define ISA18_AUDIT_CSV_MAX_LINE             512U

/*============================================================================
 * L1 — Audit Trail Entry
 *
 * A permanent, tamper-evident record of every action in the
 * alarm management system.
 *============================================================================*/

typedef struct {
    uint64_t            entry_id;               /**< Monotonically increasing entry number */
    time_t              timestamp;              /**< UTC timestamp of the action */
    char                actor_id[32];           /**< Person or system that performed action */
    char                action_type[32];        /**< Type of action (ACK, SHELVE, MOC, etc.) */
    char                target_type[32];        /**< What was acted upon (ALARM, MAD, SHELVE) */
    uint32_t            target_id;              /**< ID of the target */
    char                old_value[ISA18_MAX_DESCRIPTION_LEN]; /**< Previous state/value */
    char                new_value[ISA18_MAX_DESCRIPTION_LEN]; /**< New state/value */
    char                reason[ISA18_MAX_DESCRIPTION_LEN];    /**< Justification/reason */
    char                workstation_id[32];     /**< Originating workstation/console */
    uint32_t            checksum;               /**< Integrity checksum (simple sum) */
    uint64_t            previous_entry_hash;    /**< Hash chain link to previous entry */
} isa18_audit_entry_t;

/*============================================================================
 * L3 — Audit Trail System
 *
 * Maintains the ring buffer of audit entries with hash chaining
 * for tamper detection.
 *============================================================================*/

typedef struct {
    isa18_audit_entry_t entries[ISA18_AUDIT_RING_BUFFER_SIZE];
    uint32_t            entry_count;            /**< Total entries ever recorded */
    uint32_t            ring_pos;               /**< Current position in ring buffer */
    uint64_t            next_entry_id;          /**< Monotonically increasing ID */
    time_t              system_start_time;      /**< When audit system was initialized */
    char                site_id[32];            /**< Site/plant identifier */
    bool                is_compliant;           /**< Currently compliant with regulations */
    uint32_t            compliance_failures;    /**< Number of compliance check failures */
} isa18_audit_system_t;

/*============================================================================
 * L3 — Management of Change (MOC) Record
 *
 * ISA-18.2 §8: Every change to the alarm system configuration
 * must be recorded as a Management of Change entry.
 *============================================================================*/

typedef struct {
    uint64_t            moc_id;                 /**< Unique MOC identifier */
    time_t              request_date;           /**< When change was requested */
    time_t              approval_date;          /**< When change was approved */
    time_t              implementation_date;    /**< When change was implemented */
    char                requestor_id[32];       /**< Person requesting change */
    char                approver_id[32];        /**< Person approving change */
    uint32_t            alarm_id;               /**< Affected alarm (0 for system-wide) */
    char                change_description[ISA18_MAX_DESCRIPTION_LEN]; /**< What changed */
    char                reason[ISA18_MAX_DESCRIPTION_LEN];            /**< Why changed */
    char                risk_assessment[ISA18_MAX_DESCRIPTION_LEN];   /**< Risk evaluation */
    bool                is_approved;            /**< Has been approved? */
    bool                is_implemented;         /**< Has been implemented? */
    bool                is_verified;            /**< Has been verified post-implementation? */
    uint32_t            revision_before;        /**< Alarm revision before change */
    uint32_t            revision_after;         /**< Alarm revision after change */
} isa18_moc_record_t;

/*============================================================================
 * L3 — Initialize Audit Trail System
 *============================================================================*/
void isa18_audit_init(
    isa18_audit_system_t *audit,
    const char *site_id);

/*============================================================================
 * L3 — Log an Event to the Audit Trail
 *
 * Records an action with timestamp, actor, target, old/new values,
 * and reason. Computes the chain hash for tamper detection.
 *
 * Every audit entry links to the previous entry via a hash chain,
 * making the audit trail tamper-evident.
 *
 * Returns: entry ID on success, 0 on failure (buffer full)
 *============================================================================*/
uint64_t isa18_audit_log_event(
    isa18_audit_system_t *audit,
    const char *actor_id,
    const char *action_type,
    const char *target_type,
    uint32_t target_id,
    const char *old_value,
    const char *new_value,
    const char *reason,
    const char *workstation_id,
    time_t timestamp);

/*============================================================================
 * L3 — Query Audit Trail by Time Range
 *
 * Returns all audit entries within [start_time, end_time].
 *
 * Returns: number of matching entries (up to max_results)
 *============================================================================*/
uint32_t isa18_audit_query_by_time(
    const isa18_audit_system_t *audit,
    time_t start_time,
    time_t end_time,
    isa18_audit_entry_t *results,
    uint32_t max_results);

/*============================================================================
 * L3 — Query Audit Trail by Alarm ID
 *
 * Returns all audit entries related to a specific alarm.
 *
 * Returns: number of matching entries
 *============================================================================*/
uint32_t isa18_audit_query_by_alarm(
    const isa18_audit_system_t *audit,
    uint32_t alarm_id,
    isa18_audit_entry_t *results,
    uint32_t max_results);

/*============================================================================
 * L3 — Query Audit Trail by Operator/Actor
 *
 * Returns all audit entries performed by a specific operator.
 *
 * Returns: number of matching entries
 *============================================================================*/
uint32_t isa18_audit_query_by_operator(
    const isa18_audit_system_t *audit,
    const char *actor_id,
    isa18_audit_entry_t *results,
    uint32_t max_results);

/*============================================================================
 * L7 — Export Audit Trail as CSV
 *
 * Generates a CSV file compliant with FDA 21 CFR Part 11
 * electronic records requirements.
 *
 * CSV format:
 *   EntryID,Timestamp,Actor,ActionType,TargetType,TargetID,
 *   OldValue,NewValue,Reason,Workstation,Checksum,PrevHash
 *
 * Returns: number of lines written
 *============================================================================*/
uint32_t isa18_audit_export_csv(
    const isa18_audit_entry_t *entries,
    uint32_t entry_count,
    char *csv_buffer,
    uint32_t buffer_size);

/*============================================================================
 * L7 — Record Management of Change (MOC)
 *
 * ISA-18.2 §8.3: Every alarm system configuration change must go
 * through MOC. This function creates an MOC record that captures
 * the request, approval, and implementation chain.
 *
 * Returns: MOC ID on success, 0 on failure
 *============================================================================*/
uint64_t isa18_audit_moc_record(
    isa18_audit_system_t *audit,
    isa18_moc_record_t *moc,
    const char *requestor_id,
    uint32_t alarm_id,
    const char *change_description,
    const char *reason,
    const char *risk_assessment,
    uint32_t revision_before,
    time_t request_date);

/*============================================================================
 * L4 — Regulatory Compliance Check
 *
 * Verifies that the audit trail meets regulatory requirements:
 *   1. All entries have valid timestamps (monotonically increasing)
 *   2. All operator actions have actor IDs (no anonymous actions)
 *   3. All shelving actions have reasons documented
 *   4. All MOC records have approvals
 *   5. Hash chain is intact (tamper detection)
 *   6. No gaps in entry IDs
 *
 * Returns: true if compliant
 *============================================================================*/
bool isa18_audit_compliance_check(
    const isa18_audit_system_t *audit,
    char *compliance_report,
    uint32_t report_size);

/*============================================================================
 * L3 — Compute Entry Hash for Chain Integrity
 *
 * Each audit entry contains a hash of the previous entry,
 * creating an immutable chain. The hash is a simple but
 * effective checksum (sum of all bytes in the previous entry).
 *
 * Returns: 32-bit checksum
 *============================================================================*/
uint32_t isa18_audit_compute_entry_hash(
    const isa18_audit_entry_t *entry);

/*============================================================================
 * L4 — Verify Audit Trail Chain Integrity
 *
 * Traverses the entire audit trail and verifies that each
 * entry's previous_entry_hash matches the computed hash of
 * the preceding entry. Any mismatch indicates tampering.
 *
 * Returns: true if chain is intact
 *============================================================================*/
bool isa18_audit_verify_chain(
    const isa18_audit_system_t *audit);

/*============================================================================
 * L7 — Generate Regulatory Compliance Report
 *
 * Produces a formal compliance report suitable for regulatory
 * submissions (FDA, OSHA, EPA, etc.). Includes:
 *   - Audit trail statistics
 *   - Chain integrity verification result
 *   - MOC record summary
 *   - Operator action summary
 *   - Any compliance violations
 *
 * Returns: number of characters in report
 *============================================================================*/
uint32_t isa18_audit_generate_regulatory_report(
    const isa18_audit_system_t *audit,
    time_t report_start,
    time_t report_end,
    char *report_buffer,
    uint32_t buffer_size);

/*============================================================================
 * L7 — Per-Operator Shift Summary
 *
 * Generates a summary of an operator's alarm-related actions
 * during a shift for performance assessment and training.
 *
 * Includes:
 *   - Total alarms acknowledged
 *   - Average response time
 *   - Number of shelving operations
 *   - Number of priority violations (late responses)
 *   - Most common alarm types handled
 *
 * Returns: number of characters in summary
 *============================================================================*/
uint32_t isa18_audit_operator_shift_summary(
    const isa18_audit_system_t *audit,
    const char *operator_id,
    time_t shift_start,
    time_t shift_end,
    char *summary_buffer,
    uint32_t buffer_size);

#endif /* ALARM_AUDIT_TRAIL_H */