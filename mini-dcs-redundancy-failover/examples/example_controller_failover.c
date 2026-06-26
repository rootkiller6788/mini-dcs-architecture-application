/**
 * @file example_controller_failover.c
 * @brief End-to-End Example: Controller Redundancy Failover Simulation
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * L6 Canonical Problem: Dual-redundant DCS controller failover scenario.
 *
 * Scenario:
 *   A 1oo2 redundant DCS controller pair (FCS0101, FCS0102) controls
 *   a reactor temperature PID loop. At t=5000ms, the primary controller
 *   experiences a simulated memory fault. The failover engine detects
 *   the heartbeat loss and promotes the secondary to primary, achieving
 *   bumpless transfer. At t=15000ms, the failed controller is repaired
 *   and a failback operation returns it to primary status.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "redundancy_core.h"
#include "failover_engine.h"
#include "voting_mechanism.h"
#include "availability_model.h"

static void print_group_status(const redundancy_group_t *g)
{
    printf("  Group %s: %s, State=%d, Failovers=%u\n",
           g->group_id,
           g->group_healthy ? "HEALTHY" : "DEGRADED",
           (int)g->failover_state,
           g->failover_count);
    for (uint8_t i = 0; i < g->n_modules; i++) {
        const redundancy_module_t *m = &g->modules[i];
        const char *role_str;
        switch (m->role) {
            case MODULE_ROLE_PRIMARY:   role_str = "PRIMARY";   break;
            case MODULE_ROLE_SECONDARY: role_str = "SECONDARY"; break;
            case MODULE_ROLE_STANDBY:   role_str = "STANDBY";   break;
            case MODULE_ROLE_OFFLINE:   role_str = "OFFLINE";   break;
            default: role_str = "UNKNOWN";
        }
        const char *health_str;
        switch (m->health) {
            case MODULE_HEALTH_HEALTHY:  health_str = "HEALTHY";  break;
            case MODULE_HEALTH_FAULTY:   health_str = "FAULTY";   break;
            case MODULE_HEALTH_OFFLINE:  health_str = "OFFLINE";  break;
            default: health_str = "OTHER";
        }
        printf("    Slot %u [%s]: Role=%s Health=%s\n",
               i, m->module_id, role_str, health_str);
    }
}

int main(void)
{
    printf("\n=============================================\n");
    printf(" Controller Redundancy Failover Simulation\n");
    printf("=============================================\n\n");

    /* Setup: 1oo2 redundant controller pair */
    redundancy_group_t group;
    redundancy_group_init(&group, REDUNDANCY_1OO2, 2, 1, "REACTOR_CTRL");

    redundancy_group_add_module(&group, 0, "FCS0101");  /* Primary */
    redundancy_group_add_module(&group, 1, "FCS0102");  /* Secondary */

    failover_engine_t engine;
    failover_engine_init(&engine, &group, 100, 500);

    printf("=== Initial State ===\n");
    print_group_status(&group);
    printf("  Failover switch time estimate: %u ms\n",
           failover_switch_time_ms(&engine));
    printf("  Bumpless transfer possible: %s\n",
           failover_bumpless_possible(&engine) ? "YES" : "NO");

    /* Simulate time progression with heartbeats */
    printf("\n=== Normal Operation (t=0..5000ms) ===\n");
    for (uint64_t t = 0; t < 5000; t += 100) {
        group.total_uptime_ms = t;
        heartbeat_msg_t hb;
        memset(&hb, 0, sizeof(hb));
        hb.sequence = (uint32_t)(t / 100);
        hb.timestamp_ms = t;
        hb.health = MODULE_HEALTH_HEALTHY;

        hb.module_slot = 0;
        failover_process_heartbeat(&engine, &hb);
        hb.module_slot = 1;
        failover_process_heartbeat(&engine, &hb);
    }
    printf("  System stable, both modules healthy.\n");

    /* Simulate primary fault */
    printf("\n=== Primary Fault at t=5000ms ===\n");
    group.total_uptime_ms = 5000;
    redundancy_module_set_health(&group, 0, MODULE_HEALTH_FAULTY);
    failover_log_event(&engine, FEV_PRIMARY_FAULT, 0, 0,
                       "Memory parity error detected on FCS0101");
    printf("  FCS0101 experienced memory fault!\n");
    print_group_status(&group);

    /* Allow heartbeat timeout to trigger */
    printf("\n=== Failover Trigger (t=5600ms) ===\n");
    group.total_uptime_ms = 5600;
    /* Simulate missing heartbeats from slot 0 */
    for (uint64_t t = 5000; t < 5600; t += 100) {
        group.total_uptime_ms = t;
        heartbeat_msg_t hb;
        memset(&hb, 0, sizeof(hb));
        hb.sequence = (uint32_t)(t / 100);
        hb.timestamp_ms = t;
        hb.health = MODULE_HEALTH_HEALTHY;
        hb.module_slot = 1;
        failover_process_heartbeat(&engine, &hb);
    }
    failover_check_timeouts(&engine);

    int failover_rc = failover_execute(&engine);
    printf("  Failover result: %s\n", (failover_rc == 0) ? "SUCCESS" : "FAILED");
    print_group_status(&group);

    /* Compute availability */
    double single_avail = availability_from_mtbf_mttr(100000.0, 24.0);
    double dual_avail = redundancy_k_of_n_availability(1, 2, single_avail);
    printf("\n=== Availability Analysis ===\n");
    printf("  Single controller: %f (%d nines)\n",
           single_avail, availability_nines(single_avail));
    printf("  1oo2 redundant:    %f (%d nines)\n",
           dual_avail, availability_nines(dual_avail));

    /* Failback simulation */
    printf("\n=== Repair and Failback (t=15000ms) ===\n");
    group.total_uptime_ms = 15000;
    /* Module 0 repaired */
    redundancy_module_set_health(&group, 0, MODULE_HEALTH_HEALTHY);
    group.modules[0].network_reachable = true;
    printf("  FCS0101 repaired and tested OK.\n");

    failover_execute_failback(&engine, 0);
    printf("  Failback executed.\n");
    print_group_status(&group);

    /* Dump event log */
    printf("\n=== Failover Event Log ===\n");
    char log_buf[4096];
    failover_dump_events(&engine, log_buf, sizeof(log_buf));
    printf("%s", log_buf);

    /* PFD calculation for safety assessment */
    double pfd_single = availability_pfd_single_channel(1e-6, 8760.0);
    double pfd_1oo2 = availability_pfd_1oo2(1e-6, 8760.0, 0.05);
    printf("\n=== Safety Assessment (IEC 61508) ===\n");
    printf("  PFD single-channel: %.2e (SIL %d)\n",
           pfd_single, availability_sil_from_pfd(pfd_single));
    printf("  PFD 1oo2:           %.2e (SIL %d)\n",
           pfd_1oo2, availability_sil_from_pfd(pfd_1oo2));

    printf("\n=============================================\n");
    printf(" Simulation Complete.\n");
    printf("=============================================\n\n");
    return 0;
}
