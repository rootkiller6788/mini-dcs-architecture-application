/**
 * @file centum_vp_redundancy.c
 * @brief CENTUM VP Redundancy — Pair-and-Spare, Failover, Synchronization
 *
 * Knowledge Points:
 *   centum_redundancy_config_init — Redundancy configuration initialization (L3)
 *   centum_redundancy_pair_init — Redundancy pair setup (L2)
 *   centum_redundancy_set_role — Role assignment (primary/standby) (L2)
 *   centum_redundancy_initiate_sync — Database synchronization between CPUs (L3)
 *   centum_redundancy_check_sync_health — Sync health monitoring (L3)
 *   centum_redundancy_sync_progress — Synchronization progress tracking (L3)
 *   centum_redundancy_perform_failover — Failover execution (L5)
 *   centum_redundancy_manual_switchover — Manual operator-triggered switchover (L2)
 *   centum_redundancy_validate_pair_health — Pair health diagnostics (L3)
 *   centum_redundancy_calculate_availability — System availability calculation (L4)
 *   centum_redundancy_mtbf_hours — Mean Time Between Failures estimation (L4)
 *   centum_redundancy_mttr_seconds — Mean Time To Repair estimation (L4)
 *   centum_failover_log_init / _add / _get_latest / _print_summary — Event logging (L3)
 *   centum_redundancy_is_bumpless_possible — Bumpless failover check (L2)
 *   centum_redundancy_switchover_time_estimate — Switchover time estimation (L5)
 *
 * References:
 *   - Yokogawa CENTUM VP Pair-and-Spare Redundancy White Paper
 *   - IEC 61508 Functional Safety (SIL requirements for redundancy)
 *   - MIL-HDBK-217F Reliability Prediction
 */

#include "centum_vp_redundancy.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * centum_redundancy_config_init
 *
 * Initializes the complete redundancy configuration for a CENTUM VP
 * system. CENTUM VP implements "Pair-and-Spare" architecture where
 * critical components (CPU, power, Vnet/IP bus, I/O bus) are duplicated.
 * The spare component is kept synchronized and ready to take over
 * within one control scan cycle.
 *
 * L3 — Engineering Structure: Redundancy configuration as defined in
 * CENTUM VP system generation.
 *============================================================================*/
void centum_redundancy_config_init(centum_redundancy_config_t *redun)
{
    if (!redun) return;
    memset(redun, 0, sizeof(centum_redundancy_config_t));

    /* Initialize CPU pair */
    centum_redundancy_pair_init(&redun->cpu_pair, REDUN_PAIR_CPU, 1);

    /* Initialize power supply pair */
    centum_redundancy_pair_init(&redun->power_pair, REDUN_PAIR_POWER, 2);

    /* Initialize Vnet/IP Bus A and B pairs */
    centum_redundancy_pair_init(&redun->vnet_pair_a, REDUN_PAIR_VNET, 3);
    centum_redundancy_pair_init(&redun->vnet_pair_b, REDUN_PAIR_VNET, 4);

    /* Initialize I/O bus pair */
    centum_redundancy_pair_init(&redun->io_bus_pair, REDUN_PAIR_IO_BUS, 5);

    redun->io_module_pair_count = 0;
    redun->system_redundant = false;
    redun->failover_in_progress = false;
    redun->last_system_failover = 0;
}

/*============================================================================
 * centum_redundancy_pair_init
 *
 * Initializes a single redundancy pair (e.g., CPU A + CPU B).
 * Sets both members to OFFLINE role, IDLE sync state.
 *
 * L2 — Core Concept: Pair-and-Spare as the CENTUM VP redundancy model.
 *============================================================================*/
void centum_redundancy_pair_init(centum_redundancy_pair_t *pair,
                                  centum_redundancy_pair_type_t type, uint16_t id)
{
    if (!pair) return;
    memset(pair, 0, sizeof(centum_redundancy_pair_t));
    pair->pair_id = id;
    pair->pair_type = type;
    pair->primary.role = REDUN_ROLE_OFFLINE;
    pair->standby.role = REDUN_ROLE_OFFLINE;
    pair->primary.sync_state = REDUN_SYNC_STATE_IDLE;
    pair->standby.sync_state = REDUN_SYNC_STATE_IDLE;
    pair->pair_healthy = false;
    pair->failover_count = 0;
    pair->last_failover_time = 0;
    pair->uptime_synchronized_seconds = 0;
}

/*============================================================================
 * centum_redundancy_set_role
 *
 * Assigns a role to a pair member. CENTUM VP supports:
 *   PRIMARY  — Active controller, executing control logic
 *   STANDBY  — Hot standby, tracking primary's state
 *   SYNCING  — Copying database from primary (transitional)
 *   MAINT    — Maintenance mode (not participating)
 *   ISOLATED — Network isolated (fault condition)
 *
 * Role transitions trigger sync state changes and potential failover.
 *
 * L2 — Core Concept: Primary/Standby role assignment in redundant controllers.
 *============================================================================*/
bool centum_redundancy_set_role(centum_cpu_pair_member_t *member,
                                 centum_redundancy_role_t role)
{
    if (!member) return false;

    /* Validate transition */
    centum_redundancy_role_t prev = member->role;

    /* OFFLINE members cannot become PRIMARY directly; must go through SYNCING */
    if (prev == REDUN_ROLE_OFFLINE && role == REDUN_ROLE_PRIMARY) {
        return false;
    }

    /* ISOLATED members can only go to OFFLINE or MAINT */
    if (prev == REDUN_ROLE_ISOLATED &&
        role != REDUN_ROLE_OFFLINE && role != REDUN_ROLE_MAINT) {
        return false;
    }

    member->role = role;
    member->last_role_change = time(NULL);

    /* Set sync state based on role */
    switch (role) {
        case REDUN_ROLE_PRIMARY:
            member->sync_state = REDUN_SYNC_STATE_SYNCHRONIZED;
            break;
        case REDUN_ROLE_STANDBY:
            member->sync_state = REDUN_SYNC_STATE_TRACKING;
            break;
        case REDUN_ROLE_SYNCING:
            member->sync_state = REDUN_SYNC_STATE_COPYING;
            break;
        default:
            member->sync_state = REDUN_SYNC_STATE_IDLE;
            break;
    }

    return true;
}

/*============================================================================
 * centum_redundancy_initiate_sync
 *
 * Starts database synchronization from primary to standby CPU.
 * CENTUM VP synchronizes:
 *   1. Project database (function blocks, I/O config, sequences)
 *   2. Dynamic data (PID integral terms, sequence states, alarm states)
 *   3. HIS configuration (graphics, trends, reports)
 *
 * The sync process must complete within one scan cycle to maintain
 * deterministic behavior. KFCS2 achieves this via dedicated sync
 * hardware (high-speed memory copy over backplane).
 *
 * L3 — Engineering Structure: CPU pair synchronization mechanism.
 *============================================================================*/
bool centum_redundancy_initiate_sync(centum_redundancy_pair_t *pair)
{
    if (!pair) return false;

    /* Both members must be present */
    if (pair->primary.role == REDUN_ROLE_OFFLINE ||
        pair->standby.role == REDUN_ROLE_OFFLINE) {
        return false;
    }

    /* Set standby to SYNCING role */
    pair->standby.role = REDUN_ROLE_SYNCING;
    pair->standby.sync_state = REDUN_SYNC_STATE_COPYING;
    pair->standby.sync_data_bytes = 0;
    pair->standby.sync_data_total = 1024 * 1024 * 64; /* ~64 MB typical database */
    pair->standby.sync_progress_percent = 0.0;
    pair->standby.last_sync_time = time(NULL);

    return true;
}

/*============================================================================
 * centum_redundancy_check_sync_health
 *
 * Checks if the synchronization between primary and standby is healthy.
 * CENTUM VP monitors sync health continuously; a mismatch triggers
 * a system alarm and prevents automatic failover.
 *
 * Healthy conditions:
 *   - Both members have consistent memory checksums
 *   - Project database versions match
 *   - Sync state is SYNCHRONIZED or TRACKING
 *
 * L3 — Engineering Structure: Sync health diagnostic check.
 *============================================================================*/
bool centum_redundancy_check_sync_health(const centum_redundancy_pair_t *pair)
{
    if (!pair) return false;

    if (pair->primary.sync_state != REDUN_SYNC_STATE_SYNCHRONIZED &&
        pair->primary.sync_state != REDUN_SYNC_STATE_TRACKING) {
        return false;
    }
    if (pair->standby.sync_state != REDUN_SYNC_STATE_SYNCHRONIZED &&
        pair->standby.sync_state != REDUN_SYNC_STATE_TRACKING) {
        return false;
    }

    if (!pair->primary.memory_consistent || !pair->standby.memory_consistent) {
        return false;
    }
    if (!pair->primary.database_consistent || !pair->standby.database_consistent) {
        return false;
    }

    return true;
}

/*============================================================================
 * centum_redundancy_sync_progress
 *
 * Returns synchronization progress as a percentage (0-100%).
 * Displayed on the FCS tuning panel during initial sync.
 *
 * L3 — Engineering Structure: Sync progress indication.
 *============================================================================*/
double centum_redundancy_sync_progress(const centum_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;
    if (pair->standby.sync_data_total == 0) return 100.0;
    return (double)pair->standby.sync_data_bytes /
           (double)pair->standby.sync_data_total * 100.0;
}

/*============================================================================
 * centum_redundancy_perform_failover
 *
 * Executes a failover from primary to standby. CENTUM VP failover is:
 *   1. Bumpless if sync is healthy (standby takes over within scan cycle)
 *   2. Bumpless if outputs are held at last value during transition
 *   3. NOT bumpless if sync is mismatched (outputs go to failsafe)
 *
 * Failover types:
 *   MANUAL    — Operator initiates via SYS window
 *   AUTO      — Automatic on primary fault detection
 *   SCHEDULED — Planned maintenance switchover
 *   FAULT     — Hardware fault forcing failover
 *
 * L5 — Algorithm: Redundancy failover execution with state transfer.
 *============================================================================*/
bool centum_redundancy_perform_failover(centum_redundancy_pair_t *pair,
                                         centum_failover_type_t type,
                                         centum_failover_log_t *log)
{
    if (!pair) return false;

    /* Cannot failover if standby is not ready */
    if (pair->standby.role != REDUN_ROLE_STANDBY) return false;
    if (pair->standby.sync_state == REDUN_SYNC_STATE_MISMATCH) return false;

    /* Record the failover event */
    if (log) {
        centum_failover_event_t event;
        memset(&event, 0, sizeof(event));
        snprintf(event.event_description, sizeof(event.event_description),
                 "Failover pair %u: %s to Standby",
                 pair->pair_id, centum_redundancy_role_to_string(pair->primary.role));
        event.failover_type = type;
        event.event_time = time(NULL);
        event.prev_role = pair->primary.role;
        event.new_role = REDUN_ROLE_STANDBY;
        event.success = centum_redundancy_check_sync_health(pair);
        event.switchover_time_ms = centum_redundancy_switchover_time_estimate(pair);
        centum_failover_log_add(log, &event);
    }

    /* Swap roles */
    centum_cpu_pair_member_t temp = pair->primary;
    pair->primary = pair->standby;
    pair->standby = temp;

    /* Update new primary role */
    pair->primary.role = REDUN_ROLE_PRIMARY;
    pair->primary.last_role_change = time(NULL);

    /* Old primary becomes standby */
    pair->standby.role = REDUN_ROLE_STANDBY;
    pair->standby.last_role_change = time(NULL);

    /* Update statistics */
    pair->failover_count++;
    pair->last_failover_time = time(NULL);
    pair->last_failover_type = type;

    return true;
}

/*============================================================================
 * centum_redundancy_manual_switchover
 *
 * Operator-initiated switchover from the HIS SYS window.
 * This is a planned operation, so bumpless transfer is expected
 * if the standby is synchronized.
 *
 * L2 — Core Concept: Manual switchover for maintenance/testing.
 *============================================================================*/
bool centum_redundancy_manual_switchover(centum_redundancy_pair_t *pair,
                                          centum_failover_log_t *log)
{
    if (!pair) return false;

    /* Only allow manual switchover if pair is healthy */
    if (!centum_redundancy_validate_pair_health(pair)) return false;

    return centum_redundancy_perform_failover(pair, REDUN_FAILOVER_MANUAL, log);
}

/*============================================================================
 * centum_redundancy_validate_pair_health
 *
 * Comprehensive health check of a redundancy pair:
 *   - Both members hardware healthy
 *   - Both members software healthy
 *   - Memory and database consistent on both sides
 *   - Sync state is valid
 *
 * L3 — Engineering Structure: Pair health diagnostics.
 *============================================================================*/
bool centum_redundancy_validate_pair_health(const centum_redundancy_pair_t *pair)
{
    if (!pair) return false;

    if (!pair->primary.hardware_healthy || !pair->standby.hardware_healthy) {
        return false;
    }
    if (!pair->primary.software_healthy || !pair->standby.software_healthy) {
        return false;
    }
    if (!centum_redundancy_check_sync_health(pair)) {
        return false;
    }

    return true;
}

/*============================================================================
 * centum_redundancy_calculate_availability
 *
 * Calculates system availability using the standard formula:
 *   Availability = MTBF / (MTBF + MTTR)
 *
 * For a dual-redundant system (Pair-and-Spare), the effective MTBF
 * is significantly higher because the system survives a single failure.
 *
 * For N redundant components in parallel (1oo2):
 *   MTBF_effective = MTBF^2 / (2 * MTTR)  (for repairable systems)
 *
 * L4 — Engineering Law: Availability calculation per IEC 61508.
 * Reference: Reliability block diagram analysis for 1oo2 architecture.
 *============================================================================*/
double centum_redundancy_calculate_availability(const centum_redundancy_config_t *redun)
{
    if (!redun) return 0.0;

    double mtbf = centum_redundancy_mtbf_hours(&redun->cpu_pair);
    double mttr = centum_redundancy_mttr_seconds(&redun->cpu_pair) / 3600.0;

    if (mtbf <= 0.0) return 0.0;

    /* For dual redundant pair, effective MTBF increases */
    double parallel_mtbf = (mtbf * mtbf) / (2.0 * mttr);
    double total_mtbf = parallel_mtbf;
    double total_mttr = mttr; /* Repair time doesn't change significantly */

    return total_mtbf / (total_mtbf + total_mttr);
}

/*============================================================================
 * centum_redundancy_mtbf_hours
 *
 * Estimates MTBF (Mean Time Between Failures) for a redundancy pair.
 * CENTUM VP KFCS2 CPU module has a typical MTBF of ~150,000 hours
 * (~17 years) based on Yokogawa reliability data.
 *
 * L4 — Engineering Law: Reliability prediction for industrial controllers.
 * Reference: MIL-HDBK-217F, Yokogawa CENTUM VP reliability report.
 *============================================================================*/
double centum_redundancy_mtbf_hours(const centum_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;

    /* Base MTBF values for CENTUM VP components (hours) */
    switch (pair->pair_type) {
        case REDUN_PAIR_CPU:    return 150000.0;  /* KFCS2 CPU */
        case REDUN_PAIR_POWER:  return 200000.0;  /* Power supply */
        case REDUN_PAIR_VNET:   return 100000.0;  /* Vnet/IP interface */
        case REDUN_PAIR_IO_BUS: return 120000.0;  /* I/O bus (ESB/ER) */
        case REDUN_PAIR_IO_MOD: return 180000.0;  /* I/O module */
        case REDUN_PAIR_FAN:    return 50000.0;   /* Cooling fan */
        default:                return 100000.0;
    }
}

/*============================================================================
 * centum_redundancy_mttr_seconds
 *
 * Estimates MTTR (Mean Time To Repair) for a redundancy pair.
 * CENTUM VP N-IO modules support hot-swap (no system downtime),
 * so effective MTTR is very low (~60 seconds for module replacement).
 * CPU repair requires controlled shutdown (~3600 seconds).
 *
 * L4 — Engineering Law: Maintainability metrics for DCS hardware.
 *============================================================================*/
double centum_redundancy_mttr_seconds(const centum_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;

    switch (pair->pair_type) {
        case REDUN_PAIR_CPU:    return 3600.0;  /* 1 hour (controlled shutdown) */
        case REDUN_PAIR_POWER:  return 900.0;   /* 15 min (hot-swap capable) */
        case REDUN_PAIR_VNET:   return 600.0;   /* 10 min (cable replacement) */
        case REDUN_PAIR_IO_BUS: return 300.0;   /* 5 min */
        case REDUN_PAIR_IO_MOD: return 60.0;    /* 1 min (hot-swap) */
        case REDUN_PAIR_FAN:    return 300.0;   /* 5 min */
        default:                return 600.0;
    }
}

/*============================================================================
 * centum_failover_log_init
 *
 * Initializes the failover event log. CENTUM VP stores failover events
 * in a circular buffer accessible from the HIS System Status Display.
 * Operators can review the last 256 failover events to diagnose
 * recurring issues.
 *
 * L3 — Engineering Structure: Failover event logging.
 *============================================================================*/
void centum_failover_log_init(centum_failover_log_t *log)
{
    if (!log) return;
    memset(log, 0, sizeof(centum_failover_log_t));
    log->event_count = 0;
    log->write_index = 0;
}

/*============================================================================
 * centum_failover_log_add
 *
 * Adds a failover event to the circular log buffer.
 * When the buffer is full, the oldest event is overwritten.
 *
 * L3 — Engineering Structure: Circular event buffer implementation.
 *============================================================================*/
void centum_failover_log_add(centum_failover_log_t *log, const centum_failover_event_t *event)
{
    if (!log || !event) return;

    memcpy(&log->events[log->write_index], event, sizeof(centum_failover_event_t));
    log->write_index = (log->write_index + 1) % CENTUM_REDUN_MAX_FAILOVER_LOG;
    if (log->event_count < CENTUM_REDUN_MAX_FAILOVER_LOG) {
        log->event_count++;
    }
}

/*============================================================================
 * centum_failover_log_get_latest
 *
 * Returns the most recent failover event. Returns NULL if the log
 * is empty (no failover events recorded).
 *
 * L3 — Engineering Structure: Event log retrieval.
 *============================================================================*/
const centum_failover_event_t *centum_failover_log_get_latest(
    const centum_failover_log_t *log)
{
    if (!log || log->event_count == 0) return NULL;

    uint16_t latest_idx;
    if (log->write_index == 0) {
        latest_idx = CENTUM_REDUN_MAX_FAILOVER_LOG - 1;
    } else {
        latest_idx = log->write_index - 1;
    }
    return &log->events[latest_idx];
}

/*============================================================================
 * centum_failover_log_print_summary
 *
 * Prints a summary of all recorded failover events to stdout.
 * Used for diagnostic purposes and troubleshooting.
 *
 * L3 — Engineering Structure: Event log reporting.
 *============================================================================*/
void centum_failover_log_print_summary(const centum_failover_log_t *log)
{
    if (!log) return;

    printf("=== CENTUM VP Failover Event Log ===\n");
    printf("Total events: %u\n", log->event_count);
    printf("----------------------------------\n");

    for (uint16_t i = 0; i < log->event_count; i++) {
        const centum_failover_event_t *evt = &log->events[i];
        printf("[%u] %s | Type: %s | %s -> %s | Success: %s | Switchover: %.1f ms\n",
               i + 1,
               evt->event_description,
               centum_failover_type_to_string(evt->failover_type),
               centum_redundancy_role_to_string(evt->prev_role),
               centum_redundancy_role_to_string(evt->new_role),
               evt->success ? "YES" : "NO",
               evt->switchover_time_ms);
    }
}

/*============================================================================
 * centum_redundancy_is_bumpless_possible
 *
 * Determines if a bumpless failover is possible. Bumpless failover
 * requires the standby to be fully synchronized (all dynamic data
 * matches primary) and both members healthy.
 *
 * L2 — Core Concept: Bumpless failover prerequisite check.
 *============================================================================*/
bool centum_redundancy_is_bumpless_possible(const centum_redundancy_pair_t *pair)
{
    if (!pair) return false;

    return (pair->standby.sync_state == REDUN_SYNC_STATE_SYNCHRONIZED) &&
           pair->primary.hardware_healthy &&
           pair->standby.hardware_healthy &&
           pair->primary.database_consistent &&
           pair->standby.database_consistent;
}

/*============================================================================
 * centum_redundancy_switchover_time_estimate
 *
 * Estimates the time required for a failover switchover.
 * CENTUM VP KFCS2 achieves <1 control scan cycle (typically <200ms)
 * for a synchronized standby to take over.
 *
 * The switchover time includes:
 *   - Fault detection (~10ms)
 *   - Role transition (~5ms)
 *   - Output takeover (~1ms)
 *   - HIS notification (~50ms)
 *
 * L5 — Algorithm: Real-time failover timing estimation.
 *============================================================================*/
double centum_redundancy_switchover_time_estimate(const centum_redundancy_pair_t *pair)
{
    if (!pair) return 999.0;

    if (centum_redundancy_is_bumpless_possible(pair)) {
        return 150.0; /* ms — typical for synchronized pair */
    } else if (pair->standby.sync_state == REDUN_SYNC_STATE_TRACKING) {
        return 500.0; /* ms — tracking but not fully synced */
    } else {
        return 2000.0; /* ms — requires full re-sync */
    }
}

/*============================================================================
 * String conversion utilities
 *============================================================================*/

const char *centum_redundancy_role_to_string(centum_redundancy_role_t role)
{
    switch (role) {
        case REDUN_ROLE_PRIMARY:  return "PRIMARY";
        case REDUN_ROLE_STANDBY:  return "STANDBY";
        case REDUN_ROLE_OFFLINE:  return "OFFLINE";
        case REDUN_ROLE_SYNCING:  return "SYNCING";
        case REDUN_ROLE_MAINT:    return "MAINT";
        case REDUN_ROLE_ISOLATED: return "ISOLATED";
        default:                  return "UNKNOWN";
    }
}

const char *centum_sync_state_to_string(centum_sync_state_t state)
{
    switch (state) {
        case REDUN_SYNC_STATE_IDLE:          return "IDLE";
        case REDUN_SYNC_STATE_COPYING:       return "COPYING";
        case REDUN_SYNC_STATE_EQUALIZING:    return "EQUALIZING";
        case REDUN_SYNC_STATE_TRACKING:      return "TRACKING";
        case REDUN_SYNC_STATE_SYNCHRONIZED:  return "SYNCHRONIZED";
        case REDUN_SYNC_STATE_MISMATCH:      return "MISMATCH";
        default:                             return "UNKNOWN";
    }
}

const char *centum_failover_type_to_string(centum_failover_type_t type)
{
    switch (type) {
        case REDUN_FAILOVER_MANUAL:    return "Manual";
        case REDUN_FAILOVER_AUTO:      return "Automatic";
        case REDUN_FAILOVER_SCHEDULED: return "Scheduled";
        case REDUN_FAILOVER_FAULT:     return "Fault";
        default:                       return "Unknown";
    }
}