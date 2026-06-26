/**
 * example_las_schedule.c ? LAS CD Schedule Design and Macrocycle Simulation
 *
 * Demonstrates LAS scheduling: designing a CD schedule for a segment
 * with AI, PID, and AO blocks distributed across 3 devices, then
 * simulating one macrocycle to verify timing feasibility.
 *
 * Knowledge: L6 (Canonical Problem ? LAS Schedule Design)
 */

#include "ff_h1_datalink.h"
#include "ff_h1_device.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("??????????????????????????????????????????????????\n");
    printf("  LAS Schedule Design & Macrocycle Simulation      \n");
    printf("  Segment: 3-device flow control loop               \n");
    printf("  Macrocycle: 500 ms (standard for flow control)   \n");
    printf("??????????????????????????????????????????????????\n\n");

    /* Initialize LAS context */
    ff_las_context_t las;
    ff_las_init(&las, 0x10);  /* LAS at address 0x10 */
    las.state = FF_LAS_STATE_ACTIVE;

    /* Configure macrocycle: 500ms = 500,000 ?s */
    las.cd_schedule.macrocycle_us = 500000;
    las.token_pass_overhead_us = 200;

    /* Add devices to Live List */
    printf("??? Live List ???\n");
    ff_live_list_add(&las.live_list, 0x10, FF_DEVICE_CLASS_LINK_MASTER);
    printf("  0x10: LAS + Link Master (this device)\n");

    ff_live_list_add(&las.live_list, 0x20, FF_DEVICE_CLASS_BASIC);
    printf("  0x20: Flow Transmitter (AI block)\n");

    ff_live_list_add(&las.live_list, 0x30, FF_DEVICE_CLASS_BASIC);
    printf("  0x30: Valve Positioner (AO block)\n");

    int op_count = ff_live_list_count_operational(&las.live_list);
    printf("  Total operational: %d devices\n\n", op_count);

    /* Design CD Schedule:
     *
     * Time (?s)    Device    Buffer    Action
     * 0            0x20      0         AI reads PV, publishes to bus
     * 10000        0x10      1         PID reads PV, computes, publishes CO
     * 20000        0x30      0         AO receives CO, positions valve
     * 30000        0x10      2         PID publishes back-calculation status
     *
     * Total CD time: ~4 ? 500?s = 2000?s < 500,000?s macrocycle
     * Remaining ~498ms available for unscheduled (acyclic) communication.
     */

    printf("??? CD Schedule Design ???\n");

    ff_cd_entry_t e_ai = {0x20, 0, 0, 1500};
    ff_las_cd_add(&las, &e_ai);
    printf("  Entry 1: AI publish @ t=0?s (device 0x20, buf 0)\n");

    ff_cd_entry_t e_pid1 = {0x10, 1, 10000, 1500};
    ff_las_cd_add(&las, &e_pid1);
    printf("  Entry 2: PID read PV @ t=10,000?s (device 0x10, buf 1)\n");

    ff_cd_entry_t e_ao = {0x30, 0, 20000, 1500};
    ff_las_cd_add(&las, &e_ao);
    printf("  Entry 3: AO write CO @ t=20,000?s (device 0x30, buf 0)\n");

    ff_cd_entry_t e_pid2 = {0x10, 2, 30000, 1500};
    ff_las_cd_add(&las, &e_pid2);
    printf("  Entry 4: PID status @ t=30,000?s (device 0x10, buf 2)\n");

    /* Run one macrocycle */
    printf("\n??? Simulating Macrocycle ???\n");

    int executed = ff_las_run_macrocycle(&las);
    printf("  CD entries executed: %d / %zu\n", executed, las.cd_schedule.count);
    printf("  Macrocycle count: %u\n", las.macrocycle_count);
    printf("  CD overruns: %u\n", las.cd_overruns);

    /* Analyze schedule utilization */
    double util = ff_las_cd_utilization(&las);
    printf("\n??? Schedule Analysis ???\n");
    printf("  CD utilization: %.2f%%\n", util * 100.0);
    printf("  Macrocycle: %u ?s\n", las.cd_schedule.macrocycle_us);
    printf("  CD traffic: ~2000 ?s (4 entries ? 500 ?s)\n");

    int has_idle = ff_las_has_idle_time(&las, 100000); /* 100ms idle needed? */
    printf("  Has ?100ms idle for acyclic traffic: %s\n",
           has_idle ? "YES" : "NO");

    printf("\n??? Token Passing ???\n");
    printf("  Remaining macrocycle idle: ~%u ?s\n",
           las.idle_time_accumulated_us);
    printf("  Token passes per cycle: ~%zu (estimate)\n",
           (size_t)(las.idle_time_accumulated_us / 500));

    printf("\n??? LAS Health ???\n");
    if (las.cd_overruns == 0 && executed == (int)las.cd_schedule.count) {
        printf("  ? LAS operating normally\n");
        printf("  All CD entries executed, no overruns.\n");
    } else {
        printf("  ??  LAS has issues: %u overruns\n", las.cd_overruns);
    }

    printf("\n??????????????????????????????????????????????????\n");

    return 0;
}