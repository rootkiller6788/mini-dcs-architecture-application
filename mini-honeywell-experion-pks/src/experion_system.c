/**
 * @file experion_system.c
 * @brief Honeywell Experion PKS System Implementation
 *
 * Implements system lifecycle: initialization, node registration,
 * activation, mode transitions, health checks, and clock synchronization.
 *
 * L1-L4 coverage: System architecture, PTP time sync, state machine
 */

#include "../include/experion_system.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ==========================================================================
 * L1 - System Initialization
 * ========================================================================== */

int experion_system_init(ExperionSystem *sys, const char *name, uint32_t id)
{
    if (!sys || !name) return -1;

    memset(sys, 0, sizeof(ExperionSystem));
    sys->system_id = id;
    strncpy(sys->system_name, name, sizeof(sys->system_name) - 1);
    sys->system_name[sizeof(sys->system_name) - 1] = '\0';

    /* Initialize domain with safe defaults */
    sys->domain.domain_id = id;
    strncpy(sys->domain.domain_name, name, sizeof(sys->domain.domain_name) - 1);
    sys->domain.nodes_online = 0;
    sys->domain.nodes_total = 0;
    sys->domain.fte_enabled = false;
    sys->domain.fte_slot_a = 0;
    sys->domain.fte_slot_b = 1;

    /* Default scan period: 250ms (typical Experion regulatory control) */
    sys->scan_period_ms = 250;
    sys->mode = XMODE_INITIALIZING;

    /* Initialize node registry to empty */
    for (uint32_t i = 0; i < EXN_MAX_DOMAIN_NODES; i++) {
        sys->node_registry[i] = 0;
        sys->node_types[i] = EXN_NODE_UNKNOWN;
    }

    /* Time sync defaults */
    sys->time_sync.sntp_enabled = true;
    sys->time_sync.ptp_enabled = false;
    sys->time_sync.gps_reference = false;
    sys->time_sync.ntp_poll_interval_s = 64;
    sys->time_sync.ptp_sync_interval_s = 1;
    sys->time_sync.max_clock_drift_us = 100;

    /* Capacity defaults (small system) */
    sys->capacity.max_analog_points = 500;
    sys->capacity.max_digital_points = 1000;
    sys->capacity.max_accum_points = 100;
    sys->capacity.max_regulatory_cv = 200;
    sys->capacity.max_sequence_modules = 50;
    sys->capacity.max_device_ctrl = 100;
    sys->capacity.max_logic_blocks = 200;
    sys->capacity.max_custom_blocks = 50;

    sys->redundancy_active = false;
    sys->safety_integrated = false;
    sys->uptime_hours = 0;

    return 0;
}

/* ==========================================================================
 * L1 - Node Registration
 * ========================================================================== */

int experion_system_register_node(ExperionSystem *sys, uint32_t node_id,
                                   ExperionNodeType node_type)
{
    if (!sys) return -1;
    if (sys->domain.nodes_total >= EXN_MAX_DOMAIN_NODES) return -1;

    /* Check for duplicate node ID */
    for (uint32_t i = 0; i < sys->domain.nodes_total; i++) {
        if (sys->node_registry[i] == node_id) {
            return -1; /* Duplicate ID */
        }
    }

    uint32_t idx = sys->domain.nodes_total;
    sys->node_registry[idx] = node_id;
    sys->node_types[idx] = (uint32_t)node_type;
    sys->domain.nodes_total++;

    return 0;
}

/* ==========================================================================
 * L2 - System Activation (State Machine)
 * ========================================================================== */

/** Mode transition table.
 *  Maps (current_mode, target_mode) -> allowed.
 *  Follows Experion PKS mode transition rules.
 *
 *  Allowed transitions:
 *   INITIALIZING -> RUN, MAINTENANCE, SIMULATION
 *   RUN -> HOLD, SHUTDOWN, FAILOVER, EMERGENCY_STOP
 *   HOLD -> RUN, SHUTDOWN, EMERGENCY_STOP, MAINTENANCE
 *   SHUTDOWN -> INITIALIZING
 *   FAILOVER -> RUN, EMERGENCY_STOP
 *   EMERGENCY_STOP -> MAINTENANCE (after reset)
 *   MAINTENANCE -> INITIALIZING, RUN
 *   SIMULATION -> INITIALIZING
 */
static bool mode_transition_allowed(ExperionSystemMode current,
                                     ExperionSystemMode target)
{
    switch (current) {
    case XMODE_INITIALIZING:
        return (target == XMODE_RUN || target == XMODE_MAINTENANCE ||
                target == XMODE_SIMULATION);
    case XMODE_RUN:
        return (target == XMODE_HOLD || target == XMODE_SHUTDOWN ||
                target == XMODE_FAILOVER || target == XMODE_EMERGENCY_STOP);
    case XMODE_HOLD:
        return (target == XMODE_RUN || target == XMODE_SHUTDOWN ||
                target == XMODE_EMERGENCY_STOP || target == XMODE_MAINTENANCE);
    case XMODE_SHUTDOWN:
        return (target == XMODE_INITIALIZING);
    case XMODE_FAILOVER:
        return (target == XMODE_RUN || target == XMODE_EMERGENCY_STOP);
    case XMODE_EMERGENCY_STOP:
        return (target == XMODE_MAINTENANCE);
    case XMODE_MAINTENANCE:
        return (target == XMODE_INITIALIZING || target == XMODE_RUN);
    case XMODE_SIMULATION:
        return (target == XMODE_INITIALIZING);
    default:
        return false;
    }
}

int experion_system_set_mode(ExperionSystem *sys, ExperionSystemMode mode)
{
    if (!sys) return -1;
    if (!mode_transition_allowed(sys->mode, mode)) return -1;

    sys->mode = mode;
    (void)sys; /* suppress unused warning for sys after mode set */

    /* Mode entry actions */
    switch (mode) {
    case XMODE_RUN:
        sys->domain.domain_start_time = time(NULL);
        break;
    case XMODE_EMERGENCY_STOP:
        sys->redundancy_active = false;
        break;
    case XMODE_SHUTDOWN:
        /* Persistence would happen here */
        break;
    default:
        break;
    }

    return 0;
}

/* ==========================================================================
 * L2 - System Activation
 * ========================================================================== */

int experion_system_activate(ExperionSystem *sys)
{
    if (!sys) return -1;

    /* Pre-activation checks */
    if (sys->domain.nodes_total == 0) {
        return -2; /* No nodes configured */
    }

    /* Validate at least one ESVT server is configured */
    bool has_esvt = false;
    for (uint32_t i = 0; i < sys->domain.nodes_total; i++) {
        ExperionNodeType nt = (ExperionNodeType)sys->node_types[i];
        if (nt == EXN_NODE_ESVT || nt == EXN_NODE_ESVT_REDUNDANT) {
            has_esvt = true;
            if (sys->domain.primary_esvt_id == 0) {
                sys->domain.primary_esvt_id = sys->node_registry[i];
            }
        }
    }
    if (!has_esvt) return -3; /* No server configured */

    return experion_system_set_mode(sys, XMODE_RUN);
}

/* ==========================================================================
 * L2 - Health Check
 * ========================================================================== */

bool experion_system_health_check(const ExperionSystem *sys)
{
    if (!sys) return false;

    /* System must be in a healthy mode */
    if (sys->mode == XMODE_EMERGENCY_STOP || sys->mode == XMODE_SHUTDOWN) {
        return false;
    }

    /* At least one ESVT must be online */
    if (sys->domain.primary_esvt_id == 0 && sys->domain.backup_esvt_id == 0) {
        return false;
    }

    /* Node count must be consistent */
    if (sys->domain.nodes_online > sys->domain.nodes_total) {
        return false;
    }

    return true;
}

/* ==========================================================================
 * L2 - Point Count Query
 * ========================================================================== */

int experion_system_get_point_count(const ExperionSystem *sys,
                                     ExperionServerCapacity *capacity)
{
    if (!sys || !capacity) return -1;

    memcpy(capacity, &sys->capacity, sizeof(ExperionServerCapacity));

    uint32_t total = sys->capacity.max_analog_points +
                     sys->capacity.max_digital_points +
                     sys->capacity.max_accum_points +
                     sys->capacity.max_regulatory_cv +
                     sys->capacity.max_sequence_modules +
                     sys->capacity.max_device_ctrl +
                     sys->capacity.max_logic_blocks +
                     sys->capacity.max_custom_blocks;
    return (int)total;
}

/* ==========================================================================
 * L3 - PTP Clock Offset Calculation (IEEE 1588)
 * ========================================================================== */

/**
 * PTP offset calculation using the standard 4-message exchange.
 *
 * Master                          Slave
 *   | --- SYNC (t1) ------------> |
 *   |                              | t2 = receive time
 *   | <-- DELAY_REQ (t3) -------- |
 *   | t4 = receive time           |
 *
 * Path delay = (t2 - t1 + t4 - t3) / 2
 * Offset from master = (t2 - t1) - path_delay
 *                    = (t2 - t1 - (t4 - t3)) / 2
 *
 * Positive offset means slave clock is ahead of master.
 *
 * Reference: IEEE 1588-2008 Section 7.3.4
 * Course: MIT 2.171 Digital Control — clock synchronization
 */
int64_t experion_clock_offset_ns(int64_t t1, int64_t t2, int64_t t3, int64_t t4)
{
    /* Guard against invalid timestamps */
    if (t1 <= 0 || t2 <= 0 || t3 <= 0 || t4 <= 0) return INT64_MAX;
    if (t2 < t1 || t4 < t3) return INT64_MAX;

    /* offset = (t2 - t1 - (t4 - t3)) / 2 */
    int64_t forward_delay = t2 - t1;
    int64_t reverse_delay = t4 - t3;
    int64_t offset = (forward_delay - reverse_delay) / 2;

    return offset;
}

/* ==========================================================================
 * L2 - System Shutdown
 * ========================================================================== */

int experion_system_shutdown(ExperionSystem *sys)
{
    if (!sys) return -1;

    /* Persist system state before shutdown (note: real system persists to ERDB) */
    sys->uptime_hours += (uint32_t)(sys->total_scan_cycles * sys->scan_period_ms / 3600000ULL);

    return experion_system_set_mode(sys, XMODE_SHUTDOWN);
}