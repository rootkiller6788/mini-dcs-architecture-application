#include <stdio.h>
#include <string.h>
#include "../include/delta_v_redundancy.h"

int main(void)
{
    printf("=== DeltaV Controller Redundancy Failover Simulation ===\n\n");

    delta_v_redundancy_pair_t pair;
    delta_v_failover_log_t log;
    delta_v_redundancy_pair_init(&pair, DELTAV_REDUN_PAIR_CONTROLLER, 1);
    delta_v_failover_log_init(&log);

    printf("Initial: Primary=Healthy  Standby=Healthy  Pair=%s\n",
           pair.pair_healthy ? "OK" : "FAIL");

    double avail_single = 0.999;
    double avail_dual = delta_v_redundancy_calculate_availability(&pair);
    double nines = delta_v_redundancy_nines_availability(avail_dual);

    printf("\nAvailability Analysis:\n");
    printf("  Single controller: %.6f (%.1f nines)\n", avail_single,
           delta_v_redundancy_nines_availability(avail_single));
    printf("  Dual redundant:    %.6f (%.1f nines)\n", avail_dual, nines);

    printf("\nSimulating primary fault...\n");
    pair.primary.hardware_healthy = false;
    printf("  Primary: FAIL  Standby: OK\n");

    bool should_fo = delta_v_redundancy_should_trigger_failover(&pair);
    printf("  Auto-failover triggered: %s\n", should_fo ? "YES" : "NO");

    if (should_fo) {
        delta_v_redundancy_perform_failover(&pair, DELTAV_REDUN_FAILOVER_FAULT, &log);
        printf("  Failover complete. New Primary role: %s\n",
               delta_v_redundancy_role_to_string(pair.primary.role));
        printf("  New Standby role: %s\n",
               delta_v_redundancy_role_to_string(pair.standby.role));
    }

    printf("\nFailover log: %u events, %u successful\n",
           log.event_count, log.successful_failovers);
    printf("\nMTBF: %.0f hours  MTTR: %.0f seconds\n",
           delta_v_redundancy_mtbf_hours(&pair),
           delta_v_redundancy_mttr_seconds(&pair));

    printf("=== Simulation Complete ===\n");
    return 0;
}
