/**
 * @file example_redundancy_failover.c
 * @brief CENTUM VP Redundancy — Pair-and-Spare Failover Demonstration
 *
 * L6 — Canonical Problem: DCS redundancy failover scenario.
 * Demonstrates: Pair-and-Spare initialization, role assignment, sync,
 * manual switchover, fault-triggered failover, and system availability.
 *
 * Scenario: FCS CPU pair operating in normal mode; simulate CPU fault
 * and demonstrate automatic failover to standby, then operator switchback.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "centum_vp_redundancy.h"

static void print_pair_status(const centum_redundancy_pair_t *pair)
{
    printf("  Pair %u (%s):\n", pair->pair_id,
           pair->pair_type == REDUN_PAIR_CPU ? "CPU" : "OTHER");
    printf("    Primary:   Role=%s Sync=%s HW=%s\n",
           centum_redundancy_role_to_string(pair->primary.role),
           centum_sync_state_to_string(pair->primary.sync_state),
           pair->primary.hardware_healthy ? "OK" : "FAULT");
    printf("    Standby:   Role=%s Sync=%s HW=%s\n",
           centum_redundancy_role_to_string(pair->standby.role),
           centum_sync_state_to_string(pair->standby.sync_state),
           pair->standby.hardware_healthy ? "OK" : "FAULT");
    printf("    Pair Healthy: %s  Failover Count: %u\n",
           pair->pair_healthy ? "YES" : "NO", pair->failover_count);
    printf("    Bumpless Possible: %s\n",
           centum_redundancy_is_bumpless_possible(pair) ? "YES" : "NO");
}

int main(void)
{
    printf("========================================\n");
    printf(" CENTUM VP — Pair-and-Spare Failover\n");
    printf("========================================\n\n");

    centum_failover_log_t log;
    centum_failover_log_init(&log);

    /* Initialize a CPU redundancy pair */
    centum_redundancy_pair_t cpu_pair;
    centum_redundancy_pair_init(&cpu_pair, REDUN_PAIR_CPU, 1);

    printf("Step 1: Initialize CPU Pair\n");
    print_pair_status(&cpu_pair);

    /* Set primary as PRIMARY role and healthy */
    printf("\nStep 2: Bring Primary Online\n");
    cpu_pair.primary.hardware_healthy = true;
    cpu_pair.primary.software_healthy = true;
    cpu_pair.primary.memory_consistent = true;
    cpu_pair.primary.database_consistent = true;
    cpu_pair.primary.role = REDUN_ROLE_PRIMARY;
    cpu_pair.primary.sync_state = REDUN_SYNC_STATE_SYNCHRONIZED;

    /* Set standby as STANDBY role, healthy, and synchronized */
    printf("Step 3: Bring Standby Online and Synchronize\n");
    cpu_pair.standby.hardware_healthy = true;
    cpu_pair.standby.software_healthy = true;
    cpu_pair.standby.memory_consistent = true;
    cpu_pair.standby.database_consistent = true;
    centum_redundancy_set_role(&cpu_pair.standby, REDUN_ROLE_STANDBY);
    cpu_pair.standby.sync_state = REDUN_SYNC_STATE_SYNCHRONIZED;

    cpu_pair.pair_healthy = centum_redundancy_validate_pair_health(&cpu_pair);
    cpu_pair.uptime_synchronized_seconds = 3600; /* 1 hour of sync'ed operation */
    print_pair_status(&cpu_pair);

    /* Check availability */
    printf("\nStep 4: System Availability Analysis\n");
    double mtbf = centum_redundancy_mtbf_hours(&cpu_pair);
    double mttr = centum_redundancy_mttr_seconds(&cpu_pair);
    double switchover = centum_redundancy_switchover_time_estimate(&cpu_pair);
    printf("  CPU MTBF: %.0f hours (%.1f years)\n", mtbf, mtbf / 8760.0);
    printf("  CPU MTTR: %.0f seconds (%.1f minutes)\n", mttr, mttr / 60.0);
    printf("  Switchover Time: %.1f ms\n", switchover);
    printf("  Bumpless: %s\n",
           centum_redundancy_is_bumpless_possible(&cpu_pair) ? "YES" : "NO");

    /* Simulate primary CPU fault and automatic failover */
    printf("\nStep 5: SIMULATE PRIMARY CPU FAULT\n");
    cpu_pair.primary.hardware_healthy = false;
    printf("  [FAULT] Primary CPU hardware failure detected!\n");

    bool failover_ok = centum_redundancy_perform_failover(
        &cpu_pair, REDUN_FAILOVER_FAULT, &log);
    printf("  Automatic failover: %s\n", failover_ok ? "SUCCESS" : "FAILED");
    print_pair_status(&cpu_pair);

    /* Operator schedules maintenance and manual switchback */
    printf("\nStep 6: Repair and Manual Switchback\n");
    printf("  [MAINT] Technician replaces faulty CPU module\n");
    cpu_pair.standby.hardware_healthy = true;
    cpu_pair.standby.role = REDUN_ROLE_STANDBY;
    cpu_pair.standby.sync_state = REDUN_SYNC_STATE_SYNCHRONIZED;

    printf("  [OPERATOR] Manual switchover requested\n");
    bool switchback = centum_redundancy_manual_switchover(&cpu_pair, &log);
    printf("  Manual switchback: %s\n", switchback ? "SUCCESS" : "FAILED");
    print_pair_status(&cpu_pair);

    /* Print failover event log */
    printf("\n========================================\n");
    printf(" Failover Event Log\n");
    printf("========================================\n");
    centum_failover_log_print_summary(&log);

    /* Availability summary */
    centum_redundancy_config_t redun;
    centum_redundancy_config_init(&redun);
    redun.cpu_pair = cpu_pair;
    redun.system_redundant = true;
    double availability = centum_redundancy_calculate_availability(&redun);
    printf("\nSystem Availability: %.6f%% (%.4f nines)\n",
           availability * 100.0, -log10(1.0 - availability));

    return 0;
}