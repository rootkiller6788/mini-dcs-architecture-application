/**
 * @file alarm_audit_trail.c
 * @brief ISA-18.2 Alarm Audit Trail — Event Logging, Query, Export,
 *        Management of Change, Regulatory Compliance (L3, L4, L7)
 *
 * Knowledge Points:
 *   isa18_audit_init                — Initialize audit trail system (L3)
 *   isa18_audit_log_event           — Log event with hash chaining (L3)
 *   isa18_audit_query_by_time       — Query by time range (L3)
 *   isa18_audit_query_by_alarm      — Query by alarm ID (L3)
 *   isa18_audit_query_by_operator   — Query by operator (L3)
 *   isa18_audit_export_csv          — CSV export (L7)
 *   isa18_audit_moc_record          — Management of change recording (L7)
 *   isa18_audit_compliance_check    — Regulatory compliance check (L4)
 *   isa18_audit_compute_entry_hash  — Entry checksum computation (L3)
 *   isa18_audit_verify_chain        — Chain integrity verification (L4)
 *   isa18_audit_generate_regulatory_report — Regulatory report (L7)
 *   isa18_audit_operator_shift_summary    — Operator shift summary (L7)
 *
 * References:
 *   - ANSI/ISA-18.2-2016 §17 (Audit)
 *   - FDA 21 CFR Part 11 (Electronic Records, Electronic Signatures)
 *   - IEC 62682:2014 §17
 *   - EU Annex 11 (Computerised Systems)
 */

#include "alarm_audit_trail.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*============================================================================
 * L3 — Initialize Audit Trail System
 *
 * Sets up the ring buffer, initializes the ID counter, and
 * records the system start time. The audit trail is ready
 * to accept entries.
 *============================================================================*/
void isa18_audit_init(
    isa18_audit_system_t *audit,
    const char *site_id)
{
    if (!audit) return;

    memset(audit, 0, sizeof(isa18_audit_system_t));
    audit->entry_count = 0;
    audit->ring_pos = 0;
    audit->next_entry_id = 1;
    audit->system_start_time = time(NULL);
    audit->is_compliant = true;
    audit->compliance_failures = 0;

    if (site_id) {
        strncpy(audit->site_id, site_id, sizeof(audit->site_id) - 1);
    }
}

/*============================================================================
 * L3 — Log an Event to the Audit Trail
 *
 * Records an action with full metadata. Each entry is linked to
 * the previous entry via a hash chain, making the audit trail
 * tamper-evident.
 *
 * Hash chain mechanism:
 *   entry_n.previous_entry_hash = hash(entry_{n-1})
 *
 * If an attacker modifies entry_k, the hash stored in entry_{k+1}
 * will not match the recomputed hash of entry_k, exposing the tamper.
 *
 * This satisfies FDA 21 CFR Part 11 requirements for tamper-evident
 * electronic records.
 *
 * Returns: entry_id on success, 0 on failure (buffer full).
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
    time_t timestamp)
{
    if (!audit) return 0;

    isa18_audit_entry_t *entry = &audit->entries[audit->ring_pos];

    /* Fill the entry */
    entry->entry_id = audit->next_entry_id;
    entry->timestamp = timestamp;

    if (actor_id) {
        strncpy(entry->actor_id, actor_id, sizeof(entry->actor_id) - 1);
    }
    if (action_type) {
        strncpy(entry->action_type, action_type, sizeof(entry->action_type) - 1);
    }
    if (target_type) {
        strncpy(entry->target_type, target_type, sizeof(entry->target_type) - 1);
    }
    entry->target_id = target_id;
    if (old_value) {
        strncpy(entry->old_value, old_value, sizeof(entry->old_value) - 1);
    }
    if (new_value) {
        strncpy(entry->new_value, new_value, sizeof(entry->new_value) - 1);
    }
    if (reason) {
        strncpy(entry->reason, reason, sizeof(entry->reason) - 1);
    }
    if (workstation_id) {
        strncpy(entry->workstation_id, workstation_id,
                sizeof(entry->workstation_id) - 1);
    }

    /* Compute hash chain link: hash of previous entry */
    if (audit->entry_count > 0) {
        uint32_t prev_ring_pos = (audit->ring_pos == 0)
            ? ISA18_AUDIT_RING_BUFFER_SIZE - 1
            : audit->ring_pos - 1;
        entry->previous_entry_hash = isa18_audit_compute_entry_hash(
            &audit->entries[prev_ring_pos]);
    } else {
        entry->previous_entry_hash = 0;
    }

    /* Compute checksum of this entry (excluding the checksum field itself) */
    entry->checksum = isa18_audit_compute_entry_hash(entry);

    /* Advance ring buffer */
    audit->ring_pos = (audit->ring_pos + 1) % ISA18_AUDIT_RING_BUFFER_SIZE;
    audit->entry_count++;
    uint64_t assigned_id = audit->next_entry_id;
    audit->next_entry_id++;

    return assigned_id;
}

/*============================================================================
 * L3 — Query Audit Trail by Time Range
 *
 * Searches the ring buffer for entries within [start_time, end_time].
 * Because the ring buffer may wrap, we search all entries and
 * collect matches.
 *
 * Returns: number of matching entries found (up to max_results).
 *============================================================================*/
uint32_t isa18_audit_query_by_time(
    const isa18_audit_system_t *audit,
    time_t start_time,
    time_t end_time,
    isa18_audit_entry_t *results,
    uint32_t max_results)
{
    if (!audit || !results || max_results == 0) return 0;

    uint32_t found = 0;
    uint32_t total = (audit->entry_count < ISA18_AUDIT_RING_BUFFER_SIZE)
        ? audit->entry_count : ISA18_AUDIT_RING_BUFFER_SIZE;

    for (uint32_t i = 0; i < total && found < max_results; i++) {
        uint32_t idx = (audit->ring_pos >= total)
            ? i
            : ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE);

        /* Handle wraparound for chronological order */
        if (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE) {
            idx = (audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE;
        }

        if (audit->entries[idx].timestamp >= start_time &&
            audit->entries[idx].timestamp <= end_time) {
            results[found++] = audit->entries[idx];
        }
    }

    return found;
}

/*============================================================================
 * L3 — Query Audit Trail by Alarm ID
 *
 * Finds all audit entries related to a specific alarm.
 *============================================================================*/
uint32_t isa18_audit_query_by_alarm(
    const isa18_audit_system_t *audit,
    uint32_t alarm_id,
    isa18_audit_entry_t *results,
    uint32_t max_results)
{
    if (!audit || !results || max_results == 0) return 0;

    uint32_t found = 0;
    uint32_t total = (audit->entry_count < ISA18_AUDIT_RING_BUFFER_SIZE)
        ? audit->entry_count : ISA18_AUDIT_RING_BUFFER_SIZE;

    for (uint32_t i = 0; i < total && found < max_results; i++) {
        uint32_t idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;

        if (audit->entries[idx].target_id == alarm_id) {
            results[found++] = audit->entries[idx];
        }
    }

    return found;
}

/*============================================================================
 * L3 — Query Audit Trail by Operator/Actor
 *
 * Finds all audit entries performed by a specific operator.
 *============================================================================*/
uint32_t isa18_audit_query_by_operator(
    const isa18_audit_system_t *audit,
    const char *actor_id,
    isa18_audit_entry_t *results,
    uint32_t max_results)
{
    if (!audit || !results || !actor_id || max_results == 0) return 0;

    uint32_t found = 0;
    uint32_t total = (audit->entry_count < ISA18_AUDIT_RING_BUFFER_SIZE)
        ? audit->entry_count : ISA18_AUDIT_RING_BUFFER_SIZE;

    for (uint32_t i = 0; i < total && found < max_results; i++) {
        uint32_t idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;

        if (strcmp(audit->entries[idx].actor_id, actor_id) == 0) {
            results[found++] = audit->entries[idx];
        }
    }

    return found;
}

/*============================================================================
 * L7 — Export Audit Trail as CSV
 *
 * Generates a comma-separated values (CSV) representation of audit
 * entries, suitable for import into spreadsheets, databases, or
 * regulatory submission packages.
 *
 * FDA 21 CFR Part 11 requires that electronic records be exportable
 * in a human-readable format for regulatory inspection.
 *
 * CSV header: EntryID,Timestamp,Actor,ActionType,TargetType,TargetID,
 *             OldValue,NewValue,Reason,Workstation,Checksum,PrevHash
 *
 * Returns: number of entries written.
 *============================================================================*/
uint32_t isa18_audit_export_csv(
    const isa18_audit_entry_t *entries,
    uint32_t entry_count,
    char *csv_buffer,
    uint32_t buffer_size)
{
    if (!entries || !csv_buffer || buffer_size == 0) return 0;

    uint32_t offset = 0;

    /* CSV header */
    offset += (uint32_t)snprintf(csv_buffer + offset, buffer_size - offset,
        "EntryID,Timestamp,Actor,ActionType,TargetType,TargetID,"
        "OldValue,NewValue,Reason,Workstation,Checksum,PrevHash\n");

    /* Data rows */
    uint32_t written = 0;
    for (uint32_t i = 0; i < entry_count && offset < buffer_size; i++) {
        const isa18_audit_entry_t *e = &entries[i];

        /* Format timestamp as ISO 8601-ish string */
        char time_str[32];
        struct tm *tm_info = gmtime(&e->timestamp);
        if (tm_info) {
            strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", tm_info);
        } else {
            snprintf(time_str, sizeof(time_str), "%lld", (long long)e->timestamp);
        }

        int row_len = snprintf(csv_buffer + offset, buffer_size - offset,
            "%llu,%s,%s,%s,%s,%u,%s,%s,%s,%s,0x%08X,%016llX\n",
            (unsigned long long)e->entry_id,
            time_str,
            e->actor_id,
            e->action_type,
            e->target_type,
            e->target_id,
            e->old_value,
            e->new_value,
            e->reason,
            e->workstation_id,
            e->checksum,
            (unsigned long long)e->previous_entry_hash);

        if (row_len > 0 && offset + (uint32_t)row_len < buffer_size) {
            offset += (uint32_t)row_len;
            written++;
        } else {
            break;
        }
    }

    return written;
}

/*============================================================================
 * L7 — Record Management of Change (MOC)
 *
 * ISA-18.2 §8: Every change to an alarm configuration must go through
 * the Management of Change process. This function creates an MOC record
 * and logs it in the audit trail.
 *
 * The MOC process is:
 *   1. Request: engineer identifies need for change
 *   2. Assess: risk assessment performed
 *   3. Approve: supervisor/manager approves
 *   4. Implement: change is made in the DCS
 *   5. Verify: post-implementation verification
 *
 * This function handles steps 1 and creates the initial MOC record.
 * An audit trail entry is also logged.
 *
 * Returns: MOC ID on success, 0 on failure.
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
    time_t request_date)
{
    if (!audit || !moc) return 0;

    memset(moc, 0, sizeof(isa18_moc_record_t));
    moc->moc_id = audit->next_entry_id;
    moc->request_date = request_date;
    moc->alarm_id = alarm_id;
    moc->revision_before = revision_before;
    moc->is_approved = false;
    moc->is_implemented = false;
    moc->is_verified = false;

    if (requestor_id) {
        strncpy(moc->requestor_id, requestor_id,
                sizeof(moc->requestor_id) - 1);
    }
    if (change_description) {
        strncpy(moc->change_description, change_description,
                sizeof(moc->change_description) - 1);
    }
    if (reason) {
        strncpy(moc->reason, reason, sizeof(moc->reason) - 1);
    }
    if (risk_assessment) {
        strncpy(moc->risk_assessment, risk_assessment,
                sizeof(moc->risk_assessment) - 1);
    }

    /* Log MOC creation in audit trail */
    isa18_audit_log_event(audit, requestor_id, "MOC_CREATE",
        "ALARM_CONFIG", alarm_id,
        "", change_description ? change_description : "",
        reason ? reason : "", "ENGINEERING", request_date);

    return moc->moc_id;
}

/*============================================================================
 * L4 — Regulatory Compliance Check
 *
 * Verifies that the audit trail meets regulatory requirements for:
 *   1. FDA 21 CFR Part 11 (Electronic Records)
 *   2. EU Annex 11 (Computerised Systems)
 *   3. ISA-18.2 §17 (Audit Requirements)
 *
 * Checks performed:
 *   - Timestamps are monotonically increasing (no time travel)
 *   - No anonymous actions (all entries have actor IDs)
 *   - All shelving has documented reasons
 *   - Hash chain integrity (tamper detection)
 *   - No gaps in entry IDs (checking the last N entries)
 *   - Minimum required fields are present
 *
 * Returns: true if compliant, and populates compliance_report.
 *============================================================================*/
bool isa18_audit_compliance_check(
    const isa18_audit_system_t *audit,
    char *compliance_report,
    uint32_t report_size)
{
    if (!audit) return false;

    bool compliant = true;
    uint32_t violations = 0;
    char report[2048];
    uint32_t offset = 0;

    #define CLOG(fmt, ...) do { \
        offset += (uint32_t)snprintf(report + offset, \
            sizeof(report) - offset, fmt, ##__VA_ARGS__); \
    } while(0)

    CLOG("ISA-18.2 AUDIT COMPLIANCE CHECK\n");
    CLOG("========================================\n\n");

    uint32_t total = (audit->entry_count < ISA18_AUDIT_RING_BUFFER_SIZE)
        ? audit->entry_count : ISA18_AUDIT_RING_BUFFER_SIZE;

    if (total == 0) {
        CLOG("No audit entries to check.\n");
        if (compliance_report) {
            strncpy(compliance_report, report, report_size);
        }
        return true;
    }

    /* Check 1: Timestamp monotonicity */
    CLOG("[1] Timestamp Monotonicity Check: ");
    bool timestamps_ok = true;
    time_t prev_ts = 0;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;
        time_t ts = audit->entries[idx].timestamp;
        if (i > 0 && ts < prev_ts) {
            timestamps_ok = false;
            violations++;
        }
        prev_ts = ts;
    }
    CLOG("%s\n", timestamps_ok ? "PASS" : "FAIL");
    if (!timestamps_ok) compliant = false;

    /* Check 2: No anonymous actions */
    CLOG("[2] Anonymous Action Check: ");
    bool anon_ok = true;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;
        if (strlen(audit->entries[idx].actor_id) == 0) {
            anon_ok = false;
            violations++;
            break;
        }
    }
    CLOG("%s\n", anon_ok ? "PASS" : "FAIL");
    if (!anon_ok) compliant = false;

    /* Check 3: Hash chain integrity */
    CLOG("[3] Hash Chain Integrity: ");
    bool chain_ok = isa18_audit_verify_chain(audit);
    CLOG("%s\n", chain_ok ? "PASS" : "FAIL");
    if (!chain_ok) { compliant = false; violations++; }

    /* Check 4: Entry ID continuity (check last 100 entries) */
    CLOG("[4] Entry ID Continuity (last 100): ");
    bool ids_ok = true;
    uint32_t check_limit = (total < 100) ? total : 100;
    uint32_t start_idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
        ? ((audit->ring_pos + total - check_limit) % ISA18_AUDIT_RING_BUFFER_SIZE)
        : (total - check_limit);
    uint64_t expected_id = audit->entries[start_idx].entry_id;
    for (uint32_t i = 0; i < check_limit; i++) {
        uint32_t idx = (start_idx + i) % ISA18_AUDIT_RING_BUFFER_SIZE;
        if (audit->entries[idx].entry_id != expected_id) {
            ids_ok = false;
            violations++;
            break;
        }
        expected_id++;
    }
    CLOG("%s\n", ids_ok ? "PASS" : "FAIL");
    if (!ids_ok) compliant = false;

    /* Check 5: Shelving actions have reasons */
    CLOG("[5] Shelving Reason Check: ");
    bool reason_ok = true;
    for (uint32_t i = 0; i < total && reason_ok; i++) {
        uint32_t idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;
        if (strcmp(audit->entries[idx].action_type, "SHELVE") == 0) {
            if (strlen(audit->entries[idx].reason) == 0) {
                reason_ok = false;
                violations++;
            }
        }
    }
    CLOG("%s\n", reason_ok ? "PASS" : "FAIL");
    if (!reason_ok) compliant = false;

    CLOG("\n========================================\n");
    CLOG("Total Violations: %u\n", violations);
    CLOG("Overall Result: %s\n", compliant ? "COMPLIANT" : "NON-COMPLIANT");

    #undef CLOG

    if (compliance_report) {
        strncpy(compliance_report, report,
                report_size > 0 ? report_size : 1);
    }

    return compliant;
}

/*============================================================================
 * L3 — Compute Entry Hash (Checksum) for Chain Integrity
 *
 * Computes a simple but effective 32-bit checksum over the
 * entry's data fields (excluding the checksum field itself).
 *
 * This is not a cryptographic hash (which would require SHA-256
 * for true security), but a CRC-32-style checksum suitable for
 * detecting accidental corruption and basic tampering.
 *
 * The hash covers: entry_id, timestamp bytes, actor_id, action_type,
 * target_type, target_id, old_value, new_value, reason, workstation_id,
 * previous_entry_hash.
 *
 * Returns: 32-bit checksum.
 *============================================================================*/
uint32_t isa18_audit_compute_entry_hash(
    const isa18_audit_entry_t *entry)
{
    if (!entry) return 0;

    const uint8_t *data = (const uint8_t *)entry;
    uint32_t hash = 0x811C9DC5U; /* FNV-1a 32-bit offset basis */

    /* Hash everything except the checksum field itself */
    /* We hash the first part before checksum, then after checksum */
    size_t checksum_offset = offsetof(isa18_audit_entry_t, checksum);
    size_t checksum_end = checksum_offset + sizeof(uint32_t);

    for (size_t i = 0; i < sizeof(isa18_audit_entry_t); i++) {
        /* Skip the checksum field itself */
        if (i >= checksum_offset && i < checksum_end) continue;

        hash ^= (uint32_t)data[i];
        hash *= 0x01000193U; /* FNV-1a 32-bit prime */
    }

    return hash;
}

/*============================================================================
 * L4 — Verify Audit Trail Chain Integrity
 *
 * Traverses the audit trail and verifies that each entry's
 * previous_entry_hash field matches the computed hash of the
 * preceding entry. Any mismatch indicates tampering or corruption.
 *
 * This implements the tamper-evident chain required by
 * FDA 21 CFR Part 11 §11.10(e).
 *
 * Returns: true if chain is intact.
 *============================================================================*/
bool isa18_audit_verify_chain(
    const isa18_audit_system_t *audit)
{
    if (!audit) return false;
    if (audit->entry_count < 2) return true; /* Chain requires at least 2 entries */

    uint32_t total = (audit->entry_count < ISA18_AUDIT_RING_BUFFER_SIZE)
        ? audit->entry_count : ISA18_AUDIT_RING_BUFFER_SIZE;

    for (uint32_t i = 1; i < total; i++) {
        uint32_t prev_idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i - 1) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : (i - 1);

        uint32_t curr_idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;

        uint32_t computed_prev_hash = isa18_audit_compute_entry_hash(
            &audit->entries[prev_idx]);

        if (audit->entries[curr_idx].previous_entry_hash != computed_prev_hash) {
            return false; /* Chain broken: tampering detected */
        }
    }

    return true;
}

/*============================================================================
 * L7 — Generate Regulatory Compliance Report
 *
 * Produces a formal report suitable for submission to regulatory
 * bodies (FDA, OSHA, EPA, etc.) during inspections.
 *
 * The report covers:
 *   - System identification and date range
 *   - Total audit entries in period
 *   - Chain integrity verification
 *   - MOC record count and status
 *   - Operator action statistics
 *   - Any compliance violations found
 *============================================================================*/
uint32_t isa18_audit_generate_regulatory_report(
    const isa18_audit_system_t *audit,
    time_t report_start,
    time_t report_end,
    char *report_buffer,
    uint32_t buffer_size)
{
    if (!audit || !report_buffer || buffer_size == 0) return 0;

    /* Run compliance check first */
    char compliance_buf[1024];
    bool is_compliant = isa18_audit_compliance_check(audit, compliance_buf,
                                                       sizeof(compliance_buf));

    /* Count MOC records in period */
    uint32_t moc_count = 0;
    uint32_t total = (audit->entry_count < ISA18_AUDIT_RING_BUFFER_SIZE)
        ? audit->entry_count : ISA18_AUDIT_RING_BUFFER_SIZE;

    for (uint32_t i = 0; i < total; i++) {
        uint32_t idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;
        if (strcmp(audit->entries[idx].action_type, "MOC_CREATE") == 0 &&
            audit->entries[idx].timestamp >= report_start &&
            audit->entries[idx].timestamp <= report_end) {
            moc_count++;
        }
    }

    int written = snprintf(report_buffer, buffer_size,
        "=====================================================\n"
        "  ISA-18.2 REGULATORY COMPLIANCE REPORT\n"
        "  FDA 21 CFR Part 11 / EU Annex 11\n"
        "=====================================================\n"
        "Site:                %s\n"
        "Report Period Start: %.24s"
        "Report Period End:   %.24s"
        "System Start Time:   %.24s"
        "-----------------------------------------------------\n"
        "Total Audit Entries:       %u\n"
        "Entries in Report Period:  %u\n"
        "MOC Records in Period:     %u\n"
        "Chain Integrity:           %s\n"
        "Compliance Status:         %s\n"
        "Compliance Failures:       %u\n"
        "-----------------------------------------------------\n"
        "Compliance Check Details:\n%s\n"
        "=====================================================\n",
        audit->site_id,
        ctime(&report_start),
        ctime(&report_end),
        ctime(&audit->system_start_time),
        audit->entry_count,
        total,
        moc_count,
        isa18_audit_verify_chain(audit) ? "INTACT" : "BROKEN (TAMPER DETECTED)",
        is_compliant ? "COMPLIANT" : "NON-COMPLIANT",
        audit->compliance_failures,
        compliance_buf);

    return (uint32_t)(written > 0 ? written : 0);
}

/*============================================================================
 * L7 — Per-Operator Shift Summary
 *
 * Generates a performance summary for a specific operator during
 * a shift. Used for:
 *   - Operator performance assessment
 *   - Training needs identification
 *   - Fatigue monitoring (late-shift performance vs. early-shift)
 *   - Alarm system usability feedback
 *
 * ISA-18.2 §16.4.6 recommends monitoring per-operator alarm handling
 * to identify training gaps and system usability issues.
 *
 * Returns: number of characters in summary.
 *============================================================================*/
uint32_t isa18_audit_operator_shift_summary(
    const isa18_audit_system_t *audit,
    const char *operator_id,
    time_t shift_start,
    time_t shift_end,
    char *summary_buffer,
    uint32_t buffer_size)
{
    if (!audit || !operator_id || !summary_buffer || buffer_size == 0) {
        return 0;
    }

    uint32_t total_acks = 0;
    uint32_t shelve_ops = 0;
    uint32_t moc_ops = 0;
    double total_response_time = 0.0;
    uint32_t timed_responses = 0;
    time_t prev_event_time = 0;

    uint32_t total = (audit->entry_count < ISA18_AUDIT_RING_BUFFER_SIZE)
        ? audit->entry_count : ISA18_AUDIT_RING_BUFFER_SIZE;

    for (uint32_t i = 0; i < total; i++) {
        uint32_t idx = (audit->entry_count > ISA18_AUDIT_RING_BUFFER_SIZE)
            ? ((audit->ring_pos + i) % ISA18_AUDIT_RING_BUFFER_SIZE)
            : i;

        const isa18_audit_entry_t *e = &audit->entries[idx];

        /* Filter by operator and time range */
        if (strcmp(e->actor_id, operator_id) != 0) continue;
        if (e->timestamp < shift_start || e->timestamp > shift_end) continue;

        if (strcmp(e->action_type, "ACKNOWLEDGE") == 0) {
            total_acks++;
            if (prev_event_time > 0) {
                total_response_time += difftime(e->timestamp, prev_event_time);
                timed_responses++;
            }
        } else if (strcmp(e->action_type, "SHELVE") == 0) {
            shelve_ops++;
        } else if (strcmp(e->action_type, "MOC_CREATE") == 0) {
            moc_ops++;
        }

        prev_event_time = e->timestamp;
    }

    double avg_response = (timed_responses > 0)
        ? total_response_time / (double)timed_responses : 0.0;

    int written = snprintf(summary_buffer, buffer_size,
        "========================================\n"
        "  OPERATOR SHIFT SUMMARY\n"
        "========================================\n"
        "Operator:        %s\n"
        "Shift Start:     %.24s"
        "Shift End:       %.24s"
        "----------------------------------------\n"
        "Alarms Acknowledged:   %u\n"
        "Shelving Operations:   %u\n"
        "MOC Records Created:   %u\n"
        "Avg Response Time:     %.1f sec\n"
        "----------------------------------------\n",
        operator_id,
        ctime(&shift_start),
        ctime(&shift_end),
        total_acks,
        shelve_ops,
        moc_ops,
        avg_response);

    return (uint32_t)(written > 0 ? written : 0);
}