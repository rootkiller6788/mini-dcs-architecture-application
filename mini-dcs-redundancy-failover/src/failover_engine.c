/**
 * @file failover_engine.c
 * @brief DCS Failover Engine Implementation
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * Knowledge Coverage:
 *   L2 - Failover state machine, heartbeat monitoring
 *   L3 - Heartbeat protocol, event logging
 *   L5 - Bully leader election algorithm, quorum management
 *   L6 - Split-brain detection and resolution
 */

#include "failover_engine.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * L2: Failover Engine Initialization
 * ============================================================================
 * Knowledge: A failover engine monitors the health of modules in a
 * redundancy group and orchestrates the transition of the primary role
 * when a fault is detected. The heartbeat timeout should be > 3x the
 * heartbeat interval to avoid false failovers from transient network
 * delays (per IEC 61508 guidance on fault reaction time).
 */

int failover_engine_init(failover_engine_t *engine,
                         redundancy_group_t *group,
                         uint32_t heartbeat_ms,
                         uint32_t timeout_ms)
{
    if (!engine || !group) return -1;
    if (heartbeat_ms < 1 || timeout_ms < heartbeat_ms) return -1;

    memset(engine, 0, sizeof(*engine));
    engine->group = group;
    engine->heartbeat_interval_ms = heartbeat_ms;
    engine->heartbeat_timeout_ms = timeout_ms;
    engine->auto_failback_enabled = false;
    engine->failback_delay_ms = 10000;
    engine->split_brain_prevention = true;
    engine->max_missed_heartbeats = timeout_ms / heartbeat_ms + 1;
    engine->quorum_established = false;

    /* Initialize election priorities: lower slot = higher priority */
    for (uint8_t i = 0; i < group->n_modules; i++) {
        engine->election_priority[i] = REDUNDANCY_MAX_MODULES - i;
        engine->last_heartbeat_time[i] = 0;
        engine->missed_heartbeats[i] = 0;
    }

    return 0;
}

/* ============================================================================
 * L3: Heartbeat Processing
 * ============================================================================
 * Knowledge: Heartbeat messages are periodic signals that indicate
 * a module is alive. Each heartbeat carries a monotonically increasing
 * sequence number. Gaps in the sequence indicate lost messages.
 * A missed heartbeat counter tracks how many consecutive heartbeats
 * have been missed; exceeding the maximum triggers failover.
 */

int failover_process_heartbeat(failover_engine_t *engine,
                               const heartbeat_msg_t *msg)
{
    if (!engine || !msg) return -1;
    if (msg->module_slot >= engine->group->n_modules) return -1;

    uint8_t slot = msg->module_slot;
    redundancy_module_t *mod = &engine->group->modules[slot];

    /* Update heartbeat tracking */
    engine->last_heartbeat_time[slot] = msg->timestamp_ms;
    engine->missed_heartbeats[slot] = 0;

    /* Update module state from heartbeat */
    mod->heartbeat_counter = msg->sequence;
    mod->health = msg->health;
    mod->cpu_load = msg->cpu_load;
    mod->network_reachable = true;

    return 0;
}

/* ============================================================================
 * L5: Heartbeat Timeout Check
 * ============================================================================
 * Knowledge: Periodic check of all modules to detect heartbeat timeout.
 * A timeout occurs when current_time - last_heartbeat_time[slot]
 * exceeds the heartbeat_timeout_ms threshold.
 *
 * The check increments the missed_heartbeats counter. When this
 * exceeds max_missed_heartbeats, the module is declared faulty
 * and failover is triggered if it was the primary.
 */

int failover_check_timeouts(failover_engine_t *engine)
{
    if (!engine) return -1;

    uint64_t now = engine->group->total_uptime_ms;
    int timed_out = 0;

    for (uint8_t i = 0; i < engine->group->n_modules; i++) {
        redundancy_module_t *mod = &engine->group->modules[i];
        if (mod->health == MODULE_HEALTH_OFFLINE) continue;
        if (mod->health == MODULE_HEALTH_FAIL_SAFE) continue;

        uint64_t elapsed = now - engine->last_heartbeat_time[i];
        /* Handle wrap-around of total_uptime_ms */
        if (elapsed > engine->heartbeat_timeout_ms + 60000) {
            /* More than timeout + 1 minute suggests clock wrap, skip */
            engine->last_heartbeat_time[i] = now;
            continue;
        }

        if (engine->last_heartbeat_time[i] > 0
            && elapsed > engine->heartbeat_timeout_ms) {
            engine->missed_heartbeats[i]++;
            if (engine->missed_heartbeats[i] >= engine->max_missed_heartbeats) {
                /* Module has timed out */
                redundancy_module_set_health(engine->group, i,
                                             MODULE_HEALTH_FAULTY);
                timed_out++;
                failover_log_event(engine, FEV_HEARTBEAT_LOST,
                                  i, 0, "Heartbeat timeout");
            }
        }
    }

    /* Trigger failover if primary is now faulty */
    redundancy_module_t *primary =
        &engine->group->modules[engine->group->primary_index];
    if (primary->health != MODULE_HEALTH_HEALTHY
        && engine->group->group_healthy) {
        failover_execute(engine);
    }

    return timed_out;
}

/* ============================================================================
 * L2: Failover Execution
 * ============================================================================
 * Knowledge: Failover is the process of transferring the primary role
 * from a failed module to a healthy backup. The sequence must be
 * bumpless (no output disturbance) whenever possible.
 *
 * Failover sequence:
 *   1. Mark current primary as DEGRADED/FAULTY
 *   2. Identify the next best candidate (secondary or highest priority)
 *   3. Transfer state data from primary to new primary
 *   4. Promote new module to PRIMARY
 *   5. Demote old primary to STANDBY or OFFLINE
 *   6. Log the event
 *
 * Bumpless transfer requires the secondary to have received recent
 * state synchronization data, so its PID output matches the primary.
 */

int failover_execute(failover_engine_t *engine)
{
    if (!engine || !engine->group) return -1;

    redundancy_group_t *g = engine->group;
    failover_log_event(engine, FEV_FAILOVER_START,
                      g->primary_index, 0, "Failover initiated");

    /* Find best candidate for new primary */
    int best = failover_elect_primary(engine);
    if (best < 0) {
        failover_log_event(engine, FEV_QUORUM_LOST, 0, 0,
                          "No healthy module for failover");
        g->failover_state = FAILOVER_STATE_DEGRADED;
        return -1;
    }

    uint8_t old_primary = g->primary_index;
    redundancy_module_t *old_mod = &g->modules[old_primary];
    redundancy_module_t *new_mod = &g->modules[best];

    /* Demote old primary */
    old_mod->role = MODULE_ROLE_STANDBY;
    if (old_mod->health == MODULE_HEALTH_FAULTY) {
        old_mod->role = MODULE_ROLE_OFFLINE;
    }

    /* Promote new primary */
    new_mod->role = MODULE_ROLE_PRIMARY;
    g->primary_index = (uint8_t)best;

    /* Update secondary to next best if available */
    for (uint8_t i = 0; i < g->n_modules; i++) {
        if (i != best && g->modules[i].health == MODULE_HEALTH_HEALTHY) {
            g->modules[i].role = MODULE_ROLE_SECONDARY;
            g->secondary_index = i;
            break;
        }
    }

    g->failover_count++;
    g->last_failover_time_ms = g->total_uptime_ms;
    g->failover_state = FAILOVER_STATE_FAILED_OVER;

    failover_log_event(engine, FEV_FAILOVER_COMPLETE,
                      old_primary, (uint8_t)best, "Failover complete");
    return 0;
}

/* ============================================================================
 * L2: Failback Execution
 * ============================================================================
 * Knowledge: Failback is the voluntary return of the primary role to
 * the originally designated module after it has been repaired and
 * tested. This should only occur during planned maintenance windows
 * or when the recovered module has been verified healthy.
 */

int failover_execute_failback(failover_engine_t *engine, uint8_t target_slot)
{
    if (!engine || !engine->group) return -1;
    redundancy_group_t *g = engine->group;

    if (target_slot >= g->n_modules) return -1;
    if (g->modules[target_slot].health != MODULE_HEALTH_HEALTHY) return -1;

    failover_log_event(engine, FEV_FAILBACK_START,
                      g->primary_index, target_slot, "Failback initiated");

    uint8_t old_primary = g->primary_index;
    g->modules[old_primary].role = MODULE_ROLE_SECONDARY;
    g->modules[target_slot].role = MODULE_ROLE_PRIMARY;
    g->primary_index = target_slot;
    g->secondary_index = old_primary;

    failover_log_event(engine, FEV_FAILBACK_COMPLETE,
                      old_primary, target_slot, "Failback complete");
    return 0;
}

/* ============================================================================
 * L5: Leader Election — Priority-Based
 * ============================================================================
 * Knowledge: When the primary fails, a new primary must be elected.
 * Priority-based election selects the healthy module with the highest
 * configured priority. This is the simplest election algorithm and is
 * sufficient when there is a clear hierarchy (e.g., slot 0 > slot 1).
 */

int failover_elect_primary(const failover_engine_t *engine)
{
    if (!engine || !engine->group) return -1;

    int best_slot = -1;
    uint8_t best_priority = 0;

    for (uint8_t i = 0; i < engine->group->n_modules; i++) {
        const redundancy_module_t *mod = &engine->group->modules[i];
        if (mod->health == MODULE_HEALTH_HEALTHY
            && mod->primary_capable
            && mod->network_reachable) {
            if (engine->election_priority[i] > best_priority) {
                best_priority = engine->election_priority[i];
                best_slot = (int)i;
            }
        }
    }
    return best_slot;
}

/* ============================================================================
 * L5: Bully Leader Election Algorithm
 * ============================================================================
 * Knowledge: The Bully algorithm (Garcia-Molina, 1982) is a classic
 * distributed leader election algorithm. When a module detects that
 * the leader has failed:
 *
 *   1. It sends an ELECTION message to all higher-priority modules
 *   2. If no higher-priority module responds with ALIVE within timeout,
 *      it declares itself the leader and sends COORDINATOR to all
 *   3. If a higher-priority module responds, it defers to that module
 *
 * The algorithm guarantees that the highest-priority surviving module
 * becomes the leader.
 *
 * Reference: Garcia-Molina, "Elections in a Distributed Computing
 *            System", IEEE Trans. Computers, C-31(1), 1982
 */

int failover_bully_election(failover_engine_t *engine, uint8_t initiator_slot)
{
    if (!engine || !engine->group) return -1;
    redundancy_group_t *g = engine->group;

    if (initiator_slot >= g->n_modules) return -1;

    failover_log_event(engine, FEV_ELECTION_START,
                      initiator_slot, 0, "Bully election started");

    uint8_t initiator_priority = engine->election_priority[initiator_slot];
    bool higher_alive = false;

    /* Check if any higher-priority modules are healthy */
    for (uint8_t i = 0; i < g->n_modules; i++) {
        if (i == initiator_slot) continue;
        if (g->modules[i].health != MODULE_HEALTH_HEALTHY) continue;
        if (engine->election_priority[i] > initiator_priority) {
            higher_alive = true;
            break;
        }
    }

    int winner;
    if (higher_alive) {
        /* Defer to the highest priority healthy module */
        winner = failover_elect_primary(engine);
    } else {
        /* No higher module alive, initiator wins */
        winner = (int)initiator_slot;
    }

    if (winner >= 0) {
        failover_log_event(engine, FEV_ELECTION_WON,
                          initiator_slot, (uint8_t)winner,
                          "Election complete");
    }

    return winner;
}

/* ============================================================================
 * L6: Split-Brain Detection and Resolution
 * ============================================================================
 * Knowledge: Split-brain is a dangerous condition in redundant systems
 * where two modules both believe they are the primary. This can occur
 * when a network partition breaks the heartbeat connection between
 * modules that are both actually healthy.
 *
 * Detection: Count modules with role == PRIMARY. If > 1, split-brain
 * exists.
 *
 * Resolution: Force the module with lower election priority to STANDBY.
 * The module with the highest priority retains the PRIMARY role.
 *
 * In safety-critical systems, the resolution must be deterministic
 * and guaranteed to converge in bounded time.
 */

int failover_detect_split_brain(failover_engine_t *engine)
{
    if (!engine || !engine->group) return -1;
    redundancy_group_t *g = engine->group;

    uint8_t primary_count = 0;
    uint8_t primary_slots[REDUNDANCY_MAX_MODULES];

    for (uint8_t i = 0; i < g->n_modules; i++) {
        if (g->modules[i].role == MODULE_ROLE_PRIMARY) {
            primary_slots[primary_count++] = i;
        }
    }

    if (primary_count <= 1) return 0;

    /* Split-brain detected: resolve by keeping highest priority */
    failover_log_event(engine, FEV_SPLIT_BRAIN_DETECT, 0, 0,
                       "Split-brain condition detected");

    uint8_t best = primary_slots[0];
    for (uint8_t i = 1; i < primary_count; i++) {
        uint8_t slot = primary_slots[i];
        if (engine->election_priority[slot]
            > engine->election_priority[best]) {
            /* Demote previous best */
            g->modules[best].role = MODULE_ROLE_STANDBY;
            best = slot;
        } else {
            g->modules[slot].role = MODULE_ROLE_STANDBY;
        }
    }

    g->primary_index = best;
    g->failover_state = FAILOVER_STATE_SPLIT_BRAIN;
    failover_log_event(engine, FEV_SPLIT_BRAIN_RESOLVED, 0, best,
                       "Split-brain resolved by priority");

    return primary_count;
}

/* ============================================================================
 * L2: Quorum Management
 * ============================================================================
 * Knowledge: Quorum-based systems require a majority of nodes to agree
 * on the system state before taking action. Quorum prevents split-brain
 * scenarios where two partitions might otherwise make conflicting decisions.
 *
 * Quorum formula: A group has quorum if the number of healthy AND
 * network-reachable modules >= ceil(N / 2).
 *
 * Without quorum, the safest action is to enter FAIL_SAFE state.
 */

bool failover_quorum_check(failover_engine_t *engine)
{
    if (!engine || !engine->group) return false;
    redundancy_group_t *g = engine->group;

    uint8_t reachable = 0;
    for (uint8_t i = 0; i < g->n_modules; i++) {
        if (g->modules[i].health == MODULE_HEALTH_HEALTHY
            && g->modules[i].network_reachable) {
            reachable++;
        }
    }

    uint8_t quorum_needed = (g->n_modules / 2) + 1;
    engine->quorum_established = (reachable >= quorum_needed);

    if (!engine->quorum_established && g->group_healthy) {
        failover_log_event(engine, FEV_QUORUM_LOST, 0, 0, "Quorum lost");
    }

    return engine->quorum_established;
}

/* ============================================================================
 * L3: Event Logging
 * ============================================================================
 * Knowledge: Diagnostic event logging provides an audit trail of
 * failover activity. A circular buffer stores the most recent events,
 * preventing memory exhaustion while preserving forensic data for
 * root cause analysis.
 */

int failover_log_event(failover_engine_t *engine,
                       failover_event_type_t type,
                       uint8_t from_module, uint8_t to_module,
                       const char *desc)
{
    if (!engine) return -1;

    uint32_t idx = engine->event_count % FAILOVER_MAX_EVENTS;
    failover_event_t *evt = &engine->event_log[idx];

    evt->type = type;
    evt->timestamp_ms = engine->group ? engine->group->total_uptime_ms : 0;
    evt->from_module = from_module;
    evt->to_module = to_module;

    if (desc) {
        strncpy(evt->description, desc, sizeof(evt->description) - 1);
        evt->description[sizeof(evt->description) - 1] = '\0';
    } else {
        evt->description[0] = '\0';
    }

    engine->event_count++;
    return 0;
}

const failover_event_t *failover_last_event(const failover_engine_t *engine)
{
    if (!engine || engine->event_count == 0) return NULL;
    uint32_t idx = (engine->event_count - 1) % FAILOVER_MAX_EVENTS;
    return &engine->event_log[idx];
}

/* ============================================================================
 * L2: Failover Diagnostics
 * ============================================================================
 * Knowledge: Failover switch time is a critical KPI for high-availability
 * systems. It measures the latency from fault detection to restoration
 * of service. Lower switch times mean less process disruption.
 *
 * T_switch = T_detect + T_sync + T_activate
 *
 * Bumpless transfer requires synchronized state, meaning T_sync ≈ 0
 * at the moment of failover because the secondary already has the state.
 */

uint32_t failover_switch_time_ms(const failover_engine_t *engine)
{
    if (!engine) return 0;
    /* T_detect = heartbeat_timeout_ms (worst case detection latency)
     * T_sync = 10ms (estimated state transfer on backplane)
     * T_activate = 10ms (CPU context switch to primary role)
     * Total worst-case: timeout + 20ms
     */
    return engine->heartbeat_timeout_ms + 20;
}

bool failover_bumpless_possible(const failover_engine_t *engine)
{
    if (!engine || !engine->group) return false;
    redundancy_group_t *g = engine->group;

    /* Bumpless requires secondary to be HEALTHY with recent sync */
    if (g->secondary_index >= g->n_modules) return false;
    redundancy_module_t *sec = &g->modules[g->secondary_index];
    if (sec->health != MODULE_HEALTH_HEALTHY) return false;
    if (sec->role != MODULE_ROLE_SECONDARY) return false;

    /* Check if sync sequence matches primary */
    redundancy_module_t *pri = &g->modules[g->primary_index];
    return (sec->sync_sequence == pri->sync_sequence);
}

double failover_observed_mttr(const failover_engine_t *engine)
{
    if (!engine || engine->event_count == 0) return 0.0;

    /* Collect pairs of FEV_FAILOVER_START and FEV_FAILOVER_COMPLETE */
    uint64_t total_repair_time = 0;
    uint32_t repair_events = 0;

    for (uint32_t i = 0; i < engine->event_count - 1; i++) {
        const failover_event_t *e = &engine->event_log[i % FAILOVER_MAX_EVENTS];
        if (e->type == FEV_FAILOVER_START) {
            /* Find the matching COMPLETE event */
            for (uint32_t j = i + 1; j < engine->event_count; j++) {
                const failover_event_t *ec =
                    &engine->event_log[j % FAILOVER_MAX_EVENTS];
                if (ec->type == FEV_FAILOVER_COMPLETE) {
                    total_repair_time +=
                        (ec->timestamp_ms > e->timestamp_ms)
                        ? (ec->timestamp_ms - e->timestamp_ms) : 0;
                    repair_events++;
                    break;
                }
            }
        }
    }

    return (repair_events > 0)
           ? (double)total_repair_time / (double)repair_events
           : 0.0;
}

int failover_dump_events(const failover_engine_t *engine,
                         char *buffer, size_t bufsize)
{
    if (!engine || !buffer || bufsize == 0) return -1;

    size_t offset = 0;
    uint32_t count = engine->event_count;
    uint32_t start = (count > FAILOVER_MAX_EVENTS)
                     ? (count - FAILOVER_MAX_EVENTS) : 0;
    uint32_t n_show = (count - start);
    if (n_show > 50) n_show = 50;  /* Limit output */

    offset += snprintf(buffer + offset, bufsize - offset,
                       "Failover Event Log (%u events, showing last %u):\n",
                       count, n_show);

    for (uint32_t i = start; i < count && offset < bufsize - 1; i++) {
        const failover_event_t *e =
            &engine->event_log[i % FAILOVER_MAX_EVENTS];
        offset += snprintf(buffer + offset, bufsize - offset,
                           "[%u] t=%llu type=%d from=%u to=%u desc=%s\n",
                           i, (unsigned long long)e->timestamp_ms,
                           (int)e->type, e->from_module, e->to_module,
                           e->description);
    }

    buffer[bufsize - 1] = '\0';
    return (int)offset;
}
