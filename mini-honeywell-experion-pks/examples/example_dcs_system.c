/**
 * @file example_dcs_system.c
 * @brief Example: DCS architecture with redundant controllers and FTE
 * L6: DCS architecture canonical problem
 * Course: CMU 24-677, RWTH Aachen Industrial Control
 */
#include "../include/experion_system.h"
#include "../include/c300_controller.h"
#include "../include/dcs_redundancy.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=== Example: DCS Architecture with Redundancy ===\n\n");
    /* System with redundant controllers */
    ExperionSystem sys;
    experion_system_init(&sys, "CRACKER_DCS", 1000);
    experion_system_register_node(&sys, 1, EXN_NODE_ESVT);
    experion_system_register_node(&sys, 2, EXN_NODE_ESVT_REDUNDANT);
    experion_system_register_node(&sys, 10, EXN_NODE_C300);
    experion_system_register_node(&sys, 11, EXN_NODE_C300_REDUNDANT);
    experion_system_register_node(&sys, 20, EXN_NODE_EST);
    experion_system_activate(&sys);
    printf("System: %s, %d nodes\n", sys.system_name, sys.domain.nodes_total);
    /* C300 Controllers */
    C300Controller ctrl_a, ctrl_b;
    c300_init(&ctrl_a, 10, "C300_A", 100);
    ctrl_a.online = true;
    c300_init(&ctrl_b, 11, "C300_B", 100);
    ctrl_b.online = true;
    printf("C300-A: %s, C300-B: %s (redundant pair)\n",
           ctrl_a.controller_name, ctrl_b.controller_name);
    /* Redundancy Manager */
    RedundancyManager rm;
    redundancy_init(&rm, 1, RED_MOD_C300, 10, 11);
    redundancy_set_role(&rm, RED_ROLE_PRIMARY);
    printf("C300-A is PRIMARY, C300-B is BACKUP\n");
    /* FTE Network */
    FTENetworkStatus fte;
    memset(&fte, 0, sizeof(fte));
    fte_status_update(&fte, FTE_PATH_A, true, 100000, 100000);
    fte.paths[0].link_speed_mbps = 100;
    fte.paths[0].avg_latency_us = 250;
    fte_status_update(&fte, FTE_PATH_B, true, 80000, 80000);
    fte.paths[1].link_speed_mbps = 100;
    fte.paths[1].avg_latency_us = 300;
    bool fte_ok;
    fte_check_redundancy(&fte, &fte_ok);
    printf("FTE: %d paths active, redundant=%s\n", fte.active_paths, fte_ok?"YES":"NO");
    /* Normal heartbeats */
    printf("\n--- Normal Operation: Heartbeats ---\n");
    RedundancyHeartbeat hb;
    for (int i = 0; i < 5; i++) {
        redundancy_send_heartbeat(&rm, &hb);
        printf("Heartbeat #%d: seq=%d role=%d\n",
               i+1, hb.sequence_number, (int)hb.sender_role);
    }
    /* Failover simulation */
    printf("\n--- Primary Failure Simulation ---\n");
    for (int i = 0; i < 6; i++) {
        rm.missed_heartbeats++;
        printf("Missed heartbeat #%d\n", rm.missed_heartbeats);
    }
    if (rm.missed_heartbeats >= 5) {
        redundancy_trigger_failover(&rm, FAILOVER_HEARTBEAT_LOST);
        printf("*** FAILOVER: C300-B now PRIMARY ***\n");
    }
    /* Bumpless transfer */
    printf("\n--- Bumpless Transfer ---\n");
    BumplessTransfer bt;
    bumpless_transfer_init(&bt, 2.0);
    bumpless_transfer_start(&bt, 45.0, 55.0);
    printf("Transfer: tracked=45.0, computed=55.0\n");
    for (double t = 0.0; t <= 2.5; t += 0.5) {
        double op = bumpless_transfer_update(&bt, 55.0, 0.5);
        printf("  t=%.1fs: blended OP=%.1f%%\n", t, op);
    }
    printf("Transfer %s\n", bt.in_transition?"active":"COMPLETE");
    printf("=== DCS Architecture Example Complete ===\n");
    return 0;
}
