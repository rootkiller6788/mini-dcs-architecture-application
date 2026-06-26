/**
 * @file    ecs700_redundancy.c
 * @brief   SUPCON ECS-700 Redundancy and Failover Implementation
 *
 * Implements 1:1 hot standby redundancy, heartbeat protocol,
 * controller health scoring, data synchronization, and failover
 * state machine per IEC 62439-3 HSR requirements.
 *
 * Knowledge Coverage:
 *   L1: Redundancy structs, health metrics, path health
 *   L2: Heartbeat protocol, failover sequence, data sync
 *   L3: Redundancy state machine, event logging
 *   L4: Availability calculation, PFD computation, IEC 61508
 *
 * References:
 *   - IEC 62439-3 (2016), "Industrial communication networks — HSR/PRP"
 *   - IEC 61508-6 (2010), "Functional safety — Guidelines on application"
 *   - Gruhn & Cheddie (2006), "Safety Instrumented Systems"
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#include "ecs700_redundancy.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>

/* ============================================================================
 * L1/L2: Redundancy Pair Management
 * ============================================================================
 */

void ecs700_redundancy_pair_init(ecs700_redundancy_pair_t *pair,
                                  uint16_t pair_id,
                                  uint16_t primary_node_id,
                                  uint16_t secondary_node_id)
{
    if (pair == NULL) {
        return;
    }

    memset(pair, 0, sizeof(*pair));

    pair->pair_id = pair_id;
    pair->primary_node_id = primary_node_id;
    pair->secondary_node_id = secondary_node_id;
    pair->mode = ECS700_REDUNDANCY_1V1_HOT;
    pair->sync_in_progress = false;
    pair->partner_healthy = false;  /* Start assuming partner unknown */
    pair->sync_sequence = 0;
    pair->failover_count = 0;
    pair->failover_in_progress = false;
    pair->grace_period_end = 0;

    /* Initialize local health as nominal */
    pair->local_health.cpu_load = 30.0;
    pair->local_health.memory_available_mb = 256.0;
    pair->local_health.scnet_a_errors = 0;
    pair->local_health.scnet_b_errors = 0;
    pair->local_health.sbus_a_errors = 0;
    pair->local_health.sbus_b_errors = 0;
    pair->local_health.watchdog_timeouts = 0;
    pair->local_health.memory_ecc_errors = 0;
    pair->local_health.temperature_c = 40.0;
    pair->local_health.power_supply_v = 24.0;
    pair->local_health.uptime_ms = 0;
    pair->local_health.io_module_faults = 0;
    pair->local_health.system_time_drift_us = 0.0;
}

void ecs700_redundancy_heartbeat(ecs700_redundancy_pair_t *pair,
                                  const ecs700_health_score_t *partner_health,
                                  uint64_t current_time_us)
{
    if (pair == NULL || partner_health == NULL) {
        return;
    }

    /* Update partner health data from heartbeat message */
    memcpy(&pair->partner_health, partner_health, sizeof(ecs700_health_score_t));

    /* Reset heartbeat miss counter */
    pair->heartbeat_miss_count = 0;

    /* Update partner health status */
    double partner_score = ecs700_compute_health_score(partner_health);
    pair->partner_healthy = (partner_score >= 50.0);  /* 50% threshold */

    /* Update last heartbeat reception timestamps */
    if (current_time_us > 0) {
        pair->last_heartbeat_primary = current_time_us;
        pair->last_heartbeat_secondary = current_time_us;
    }
}

int ecs700_redundancy_failover(ecs700_redundancy_pair_t *pair,
                                uint64_t current_time_us)
{
    /**
     * Failover Execution Sequence:
     *
     * Trigger: partner heartbeat timeout detected
     * (heartbeat_miss_count >= ECS700_HEARTBEAT_MISS_MAX)
     *
     * Step 1: Verify local health is adequate for primary role
     * Step 2: Check failover grace period (prevent oscillation)
     * Step 3: Execute failover:
     *         - Switch I/O ownership to local controller
     *         - Activate last synchronized control state
     *         - Begin partner recovery monitoring
     *         - Broadcast new primary status on SCnet
     * Step 4: Log failover event for diagnostic analysis
     *
     * Failover time budget: < 20 ms (per IEC 62439-3 HSR)
     *   - Fault detection:     3 × 50 ms = 150 ms worst case
     *   - Decision logic:      < 1 ms
     *   - State activation:    < 1 ms
     *   - I/O switchover:      < 10 ms (relay-based)
     *   - Network reconfiguration: < 8 ms
     */

    if (pair == NULL) {
        return -1;
    }

    if (pair->failover_in_progress) {
        return -2;  /* Failover already in progress */
    }

    /* Check grace period: prevent failover oscillation
     * (rapid primary↔secondary switching) */
    if (pair->grace_period_end > current_time_us) {
        return -3;  /* Within grace period, suppress failover */
    }

    /* Verify local controller health is sufficient */
    double local_score = ecs700_compute_health_score(&pair->local_health);
    if (local_score < 40.0) {
        return -4;  /* Local health inadequate for primary role */
    }

    /* Execute failover */
    pair->failover_in_progress = true;
    pair->last_failover_time = current_time_us;
    pair->failover_count++;

    /* Swap primary/secondary roles */
    uint16_t old_primary = pair->primary_node_id;
    pair->primary_node_id = pair->secondary_node_id;
    pair->secondary_node_id = old_primary;

    /* Start grace period (5 seconds) to prevent oscillation */
    pair->grace_period_end = current_time_us + ECS700_FAILOVER_GRACE_US;

    /* Reset partner health (unknown until next heartbeat) */
    pair->partner_healthy = false;
    pair->heartbeat_miss_count = 0;

    /* Log the failover event */
    ecs700_redundancy_log_event(pair, ECS700_REDUN_EVENT_FAILOVER_COMPLETE,
                                 "Failover completed successfully");

    pair->failover_in_progress = false;

    return 0;
}

int ecs700_redundancy_sync_state(ecs700_redundancy_pair_t *pair,
                                  const uint8_t *state_data,
                                  uint32_t state_size,
                                  uint64_t current_time_us)
{
    /**
     * Control State Synchronization:
     *
     * The primary controller periodically synchronizes its complete
     * control state to the secondary. This ensures the secondary can
     * take over with zero control disturbance during failover.
     *
     * Synchronized data includes:
     *   - All PID block states (SP, PV, OP, integral, derivative)
     *   - SFC step states and transition conditions
     *   - Interlock states
     *   - I/O output values (to hold during switchover)
     *   - Alarm states
     *   - Configuration checksums (to verify consistency)
     *
     * Synchronization strategy:
     *   - Full sync: All data (at startup, after configuration change)
     *   - Incremental sync: Changed data only (each scan, reduces network load)
     *
     * Network bandwidth for sync:
     *   Typical full sync: ~100 KB (512 loops × ~200 bytes)
     *   Typical incremental: ~2 KB (10% changed per scan)
     *   At 100 Mbps SCnet: ~0.2 ms for incremental sync
     */

    if (pair == NULL || state_data == NULL || state_size == 0) {
        return -1;
    }

    /* Size check: maximum reasonable sync data */
    if (state_size > (10 * 1024 * 1024)) {
        return -2;  /* Unreasonably large sync data */
    }

    pair->sync_in_progress = true;
    pair->last_sync_time = current_time_us;
    pair->sync_sequence++;
    pair->bytes_synced += state_size;

    /* In a real system, the state data would be deserialized and
     * applied to the secondary controller's control engine.
     * For this simulation, we track the synchronization metadata. */

    pair->sync_in_progress = false;

    return 0;
}

/* ============================================================================
 * L4: Health Scoring Algorithm
 * ============================================================================
 */

double ecs700_compute_health_score(const ecs700_health_score_t *health)
{
    /**
     * Weighted Health Score Calculation:
     *
     *   Component         | Weight | Criteria
     *   ------------------|--------|---------------------------
     *   CPU Load          | 25%    | < 60% = full, 60-80% = degraded, > 80% = poor
     *   Memory Available  | 20%    | > 256 MB = full, 128-256 = degraded, < 128 = poor
     *   Network Paths     | 20%    | 0 errors = full, errors reduce score
     *   I/O Health        | 15%    | 0 faults = full, faults reduce score
     *   Power Supply      | 10%    | 24V ± 5% = full
     *   Temperature       | 10%    | < 65°C = full, 65-85°C = degraded, > 85°C = poor
     *
     * Score interpretation:
     *   90-100: Excellent — fully capable primary
     *   70-89:  Good — capable primary with minor issues
     *   50-69:  Degraded — may need maintenance
     *   30-49:  Poor — should not be primary
     *   0-29:   Critical — requires immediate attention
     */

    if (health == NULL) {
        return 0.0;
    }

    double score = 0.0;

    /* 1. CPU Load (25%) — linear degradation above 60% */
    double cpu_component;
    if (health->cpu_load <= 60.0) {
        cpu_component = 25.0;  /* Full marks */
    } else if (health->cpu_load <= 80.0) {
        cpu_component = 25.0 * (80.0 - health->cpu_load) / 20.0;
    } else if (health->cpu_load <= 95.0) {
        cpu_component = 25.0 * (95.0 - health->cpu_load) / 15.0 * 0.5;
    } else {
        cpu_component = 0.0;
    }
    score += (cpu_component > 0.0) ? cpu_component : 0.0;

    /* 2. Memory Available (20%) */
    double mem_component;
    if (health->memory_available_mb >= 256.0) {
        mem_component = 20.0;
    } else if (health->memory_available_mb >= 128.0) {
        mem_component = 20.0 * (health->memory_available_mb - 128.0) / 128.0;
    } else if (health->memory_available_mb >= 64.0) {
        mem_component = 10.0 * health->memory_available_mb / 64.0;
    } else {
        mem_component = 0.0;
    }
    score += (mem_component > 0.0) ? mem_component : 0.0;

    /* 3. Network Path Health (20%) — weighted by path importance */
    uint32_t total_net_errors = health->scnet_a_errors + health->scnet_b_errors
                               + health->sbus_a_errors + health->sbus_b_errors;
    double net_component;
    if (total_net_errors == 0) {
        net_component = 20.0;
    } else if (total_net_errors < 10) {
        net_component = 18.0;
    } else if (total_net_errors < 50) {
        net_component = 15.0;
    } else if (total_net_errors < 100) {
        net_component = 10.0;
    } else if (total_net_errors < 500) {
        net_component = 5.0;
    } else {
        net_component = 0.0;
    }
    score += net_component;

    /* 4. I/O Subsystem Health (15%) */
    double io_component;
    if (health->io_module_faults == 0) {
        io_component = 15.0;
    } else if (health->io_module_faults <= 2) {
        io_component = 10.0;
    } else if (health->io_module_faults <= 5) {
        io_component = 5.0;
    } else {
        io_component = 0.0;
    }
    score += io_component;

    /* 5. Power Supply Health (10%) — 24V ± 10% tolerance */
    double ps_component;
    double v_error = fabs(health->power_supply_v - 24.0);
    if (v_error <= 1.2) {
        ps_component = 10.0;
    } else if (v_error <= 2.4) {
        ps_component = 10.0 * (2.4 - v_error) / 1.2;
    } else {
        ps_component = 0.0;
    }
    score += (ps_component > 0.0) ? ps_component : 0.0;

    /* 6. Temperature (10%) */
    double temp_component;
    if (health->temperature_c <= 65.0) {
        temp_component = 10.0;
    } else if (health->temperature_c <= 85.0) {
        temp_component = 10.0 * (85.0 - health->temperature_c) / 20.0;
    } else {
        temp_component = 0.0;
    }
    score += (temp_component > 0.0) ? temp_component : 0.0;

    /* Apply penalties for critical faults */
    if (health->watchdog_timeouts > 0) {
        score -= health->watchdog_timeouts * 10.0;
    }
    if (health->memory_ecc_errors > 0) {
        score -= health->memory_ecc_errors * 5.0;
    }

    /* Clamp to [0, 100] */
    if (score < 0.0) {
        score = 0.0;
    } else if (score > 100.0) {
        score = 100.0;
    }

    return score;
}

/* ============================================================================
 * L2: Network Path Health Monitoring
 * ============================================================================
 */

void ecs700_path_health_init(ecs700_path_health_t *path,
                              uint16_t path_id, const char *name)
{
    if (path == NULL) {
        return;
    }

    memset(path, 0, sizeof(*path));

    path->path_id = path_id;
    path->link_up = true;
    path->degraded = false;

    if (name != NULL) {
        strncpy(path->path_name, name, sizeof(path->path_name) - 1);
        path->path_name[sizeof(path->path_name) - 1] = '\0';
    }
}

void ecs700_path_health_update(ecs700_path_health_t *path,
                                uint32_t sent, uint32_t received,
                                uint32_t errors, double latency_us)
{
    if (path == NULL) {
        return;
    }

    path->packets_sent += sent;
    path->packets_received += received;
    path->packets_lost += (sent - received);
    path->packet_errors += errors;

    /* Packet loss rate: percentage of sent packets that were lost */
    if (path->packets_sent > 0) {
        path->packet_loss_rate = (double)path->packets_lost
                               / (double)path->packets_sent * 100.0;
    }

    /* Update latency statistics (exponential moving average) */
    if (latency_us > 0.0) {
        if (path->average_latency_us == 0.0) {
            path->average_latency_us = latency_us;
        } else {
            /* EMA with α=0.1 for smoothing */
            path->average_latency_us = 0.9 * path->average_latency_us
                                       + 0.1 * latency_us;
        }

        if (latency_us > path->max_latency_us) {
            path->max_latency_us = latency_us;
        }

        /* Jitter: EMA of absolute deviation from average */
        double deviation = fabs(latency_us - path->average_latency_us);
        if (path->jitter_us == 0.0) {
            path->jitter_us = deviation;
        } else {
            path->jitter_us = 0.9 * path->jitter_us + 0.1 * deviation;
        }
    }

    /* Assess path health */
    if (path->packet_loss_rate > (100.0 - ECS700_PATH_HEALTH_FAILED)) {
        path->link_up = false;
        path->degraded = true;
    } else if (path->packet_loss_rate > (100.0 - ECS700_PATH_HEALTH_DEGRADED)) {
        path->link_up = true;
        path->degraded = true;
    } else {
        path->link_up = true;
        path->degraded = false;
    }
}

/* ============================================================================
 * L3: Event Logging
 * ============================================================================
 */

void ecs700_redundancy_log_event(ecs700_redundancy_pair_t *pair,
                                  ecs700_redundancy_event_type_t type,
                                  const char *reason)
{
    if (pair == NULL) {
        return;
    }

    /* In a real DCS, events are:
     *   1. Stamped with GPS-synchronized time
     *   2. Prioritized (alarm vs. event vs. diagnostic)
     *   3. Stored in circular buffer (SOE — Sequence of Events)
     *   4. Broadcast to operator stations via SCnet
     *   5. Archived to historian for long-term analysis
     *
     * Event types trigger specific diagnostic responses:
     *   - FAILOVER_START/COMPLETE: Alert maintenance, log for RCA
     *   - HEARTBEAT_MISS: Increment diagnostic counter, start timer
     *   - SYNC_FAILED: Trigger automatic retry, degrade readiness
     */

    /* For this implementation, event logging is a structured record
     * that would be consumed by the DCS diagnostic subsystem. */
    (void)type;
    (void)reason;
}

/* ============================================================================
 * L4: Reliability Engineering — Availability and PFD
 * ============================================================================
 */

double ecs700_compute_availability(double mtbf_hours, double mttr_hours)
{
    /**
     * System Availability with 1:1 Hot Standby:
     *
     * Single controller availability:
     *   A_single = MTBF / (MTBF + MTTR)
     *
     * For two independent controllers in 1:1 hot standby:
     *   A_redundant = 1 - (1 - A_single)^2
     *               = 2*A_single - A_single^2
     *
     * This assumes:
     *   - Independent failure modes (no common cause failures)
     *   - Perfect failover detection (100% coverage)
     *   - Instant failover (0 downtime during switchover)
     *
     * Realistic ECS-700 values:
     *   MTBF(CS) = 150,000 hours (≈ 17 years continuous operation)
     *   MTTR(CS) = 4 hours (replace module, reload config)
     *   A_single = 150000/(150000+4) = 0.9999733
     *   A_redundant = 1 - (1-0.9999733)^2 = 0.9999999993
     *   → Approximately 7-nines availability
     *
     * With common cause factor β = 0.02 (2% of failures affect both):
     *   A_redundant_ccf = 1 - (1-A_single)^2 - β * (1-A_single)
     *   This reduces availability but remains well above 5-nines.
     */

    if (mtbf_hours <= 0.0) {
        return 0.0;
    }
    if (mttr_hours < 0.0) {
        mttr_hours = 0.0;
    }

    /* Single controller availability */
    double a_single = mtbf_hours / (mtbf_hours + mttr_hours);

    /* 1:1 Redundant availability (independent failures) */
    double a_redundant = 1.0 - (1.0 - a_single) * (1.0 - a_single);

    /* Clamp to [0, 1] */
    if (a_redundant < 0.0) {
        a_redundant = 0.0;
    } else if (a_redundant > 1.0) {
        a_redundant = 1.0;
    }

    return a_redundant;
}

double ecs700_compute_pfd_avg(double lambda_d, double t1_hours)
{
    /**
     * PFDavg Calculation for 1oo2 Architecture (IEC 61508-6):
     *
     * For 1oo2 (1-out-of-2) voted architecture:
     *   PFD_avg = (λ_D * T1)^2 / 3
     *
     * Where:
     *   λ_D = dangerous undetected failure rate per hour
     *   T1  = proof test interval in hours
     *
     * For a control station:
     *   λ_D = 1/MTBF_dangerous ≈ 1/150000 h⁻¹ = 6.67E-6 h⁻¹
     *   T1  = 8760 h (1 year proof test interval)
     *
     *   PFD_avg = (6.67E-6 * 8760)^2 / 3 = (0.0584)^2 / 3 = 0.00114
     *
     * SIL comparison (IEC 61508-1):
     *   SIL 1: PFD_avg ∈ [1E-2, 1E-1)
     *   SIL 2: PFD_avg ∈ [1E-3, 1E-2)
     *   SIL 3: PFD_avg ∈ [1E-4, 1E-3)
     *   SIL 4: PFD_avg ∈ [1E-5, 1E-4)
     *
     * The computed PFD_avg = 0.00114 puts this configuration in SIL 2 range.
     * To achieve SIL 3, either:
     *   - Reduce proof test interval to < 1 year
     *   - Improve dangerous failure rate (better diagnostics)
     *   - Use 2oo3 voting architecture
     */

    if (lambda_d <= 0.0 || t1_hours <= 0.0) {
        return 1.0;  /* Degenerate: assume worst case */
    }

    /* (λ_D * T1)^2 / 3 */
    double lambda_t1 = lambda_d * t1_hours;
    double pfd = (lambda_t1 * lambda_t1) / 3.0;

    /* Clamp: PFD cannot exceed 1.0 */
    if (pfd > 1.0) {
        pfd = 1.0;
    }

    return pfd;
}
