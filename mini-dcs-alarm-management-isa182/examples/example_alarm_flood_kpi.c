/**
 * @file example_alarm_flood_kpi.c
 * @brief ISA-18.2 Alarm Flood Detection and KPI Analysis (L6, L7)
 *
 * Demonstrates:
 *   1. Alarm flood detection with rolling 10-minute window
 *   2. KPI calculation (alarms per day, peak rate, priority distribution)
 *   3. Chattering alarm detection
 *   4. EEMUA 191 benchmark assessment
 *   5. KPI report generation
 *
 * Simulates a severe process upset that generates an alarm flood.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "alarm_management_types.h"
#include "alarm_engine.h"
#include "alarm_kpi_metrics.h"

int main(void) {
    printf("\n========================================\n");
    printf(" ISA-18.2 ALARM FLOOD & KPI ANALYSIS\n");
    printf("========================================\n\n");

    /* Simulate alarm history over a 6-hour period */
    printf("Simulating 6 hours of plant operation...\n\n");

    isa18_kpi_counts_t kpi;
    time_t period_start = time(NULL) - 21600; /* 6 hours ago */
    isa18_kpi_init(&kpi, period_start, 2); /* 2 operators on shift */

    isa18_alarm_flood_detector_t flood;
    memset(&flood, 0, sizeof(flood));

    /* Scenario: 6 hours of operation */
    /* Hours 0-2: Normal (steady state, ~30 alarms/hour) */
    /* Hours 2-3: Process upset (flood: 45 alarms in 10 min) */
    /* Hours 3-4: Recovery with chattering alarms */
    /* Hours 4-6: Return to normal */

    time_t now = period_start;
    uint32_t total_alarms = 0;
    uint32_t flood_alarms = 0;
    uint32_t chattering_alarms = 0;

    /* Phase 1: Normal operation (hours 0-2) */
    printf("Phase 1: Normal operation (hours 0-2)\n");
    for (int min = 0; min < 120; min++) {
        int alarms_this_minute = (min % 10 == 0) ? 3 : 1;
        for (int a = 0; a < alarms_this_minute; a++) {
            now = period_start + min * 60 + a * 20;
            bool flood_started = isa18_flood_detector_update(
                &flood, now, ISA18_OVERMANAGEABLE_PEAK_10MIN);
            total_alarms++;
            if (flood.is_flood_active) flood_alarms++;
        }
    }
    printf("  Alarms generated: ~%u\n", total_alarms);
    printf("  Flood state: %s\n\n",
           flood.is_flood_active ? "ACTIVE" : "INACTIVE");

    /* Phase 2: Process upset (hours 2-3) - Alarm Flood */
    printf("Phase 2: Process upset - ALARM FLOOD (hours 2-3)\n");
    uint32_t phase2_start = total_alarms;
    for (int min = 0; min < 60; min++) {
        int alarms_this_minute;
        if (min < 5) {
            alarms_this_minute = 8;  /* Rapid escalation */
        } else if (min < 15) {
            alarms_this_minute = 12; /* Peak flood */
        } else if (min < 30) {
            alarms_this_minute = 6;  /* Subsiding */
        } else {
            alarms_this_minute = 4;  /* Recovery */
        }
        for (int a = 0; a < alarms_this_minute; a++) {
            now = period_start + 7200 + min * 60 + a * (60 / alarms_this_minute);
            bool flood_started = isa18_flood_detector_update(
                &flood, now, ISA18_OVERMANAGEABLE_PEAK_10MIN);
            total_alarms++;
            if (flood_started) {
                printf("  *** ALARM FLOOD DECLARED at t=+%d min ***\n",
                       120 + min);
            }
            if (flood.is_flood_active) flood_alarms++;
        }
    }
    printf("  Alarms in this phase: %u\n", total_alarms - phase2_start);
    printf("  Peak alarms in flood window: %u\n",
           flood.peak_alarms_in_flood);
    printf("  Flood state: %s\n\n",
           flood.is_flood_active ? "ACTIVE" : "INACTIVE");

    /* Phase 3: Recovery with chattering */
    printf("Phase 3: Recovery with chattering alarms (hours 3-4)\n");
    isa18_chattering_detector_t chatter;
    memset(&chatter, 0, sizeof(chatter));

    for (int min = 0; min < 60; min++) {
        int alarms_this_minute = (min % 3 == 0) ? 5 : 2;
        for (int a = 0; a < alarms_this_minute; a++) {
            now = period_start + 10800 + min * 60 + a * 10;
            isa18_flood_detector_update(
                &flood, now, ISA18_OVERMANAGEABLE_PEAK_10MIN);
            total_alarms++;

            /* Check for chattering */
            if (isa18_kpi_detect_chattering(&chatter, now)) {
                chattering_alarms++;
            }
        }
    }
    printf("  Chattering detected: %s (%u events)\n",
           chatter.is_chattering ? "YES" : "NO",
           chatter.total_chattering_events);

    /* End the flood */
    time_t flood_end_check = now + 601; /* 10+ minutes later */
    isa18_flood_detector_check(&flood, flood_end_check);
    printf("  Flood state after timeout: %s\n\n",
           flood.is_flood_active ? "ACTIVE" : "INACTIVE");

    /* Phase 4: Return to normal (hours 4-6) */
    printf("Phase 4: Return to normal (hours 4-6)\n");
    uint32_t phase4_start = total_alarms;
    for (int min = 0; min < 120; min++) {
        int alarms_this_minute = (min % 10 == 0) ? 2 : 1;
        for (int a = 0; a < alarms_this_minute; a++) {
            now = period_start + 14400 + min * 60 + a * 30;
            isa18_flood_detector_update(
                &flood, now, ISA18_OVERMANAGEABLE_PEAK_10MIN);
            total_alarms++;
        }
    }
    printf("  Alarms in this phase: %u\n", total_alarms - phase4_start);

    /* Populate KPI counters */
    kpi.alarms_per_day = total_alarms;
    kpi.critical_per_day = total_alarms * 5 / 100;
    kpi.high_per_day = total_alarms * 15 / 100;
    kpi.medium_per_day = total_alarms * 60 / 100;
    kpi.low_per_day = total_alarms * 20 / 100;
    kpi.peak_10min_rate = flood.peak_alarms_in_flood;
    kpi.chattering_alarms = chattering_alarms > 0 ? chattering_alarms / 3 : 0;
    kpi.active_alarm_count = 5;
    kpi.shelved_alarm_count = 2;
    kpi.stale_24h_count = 0;
    kpi.avg_response_time_sec = 180.0;
    kpi.alarm_rationalization_coverage = 85.0;

    /* Calculate and display KPIs */
    printf("\n========================================\n");
    printf(" KPI ANALYSIS RESULTS\n");
    printf("========================================\n");
    printf("  Total alarms in period:    %u\n", total_alarms);
    printf("  Alarms during flood:       %u (%.1f%%)\n",
           flood_alarms,
           100.0 * (double)flood_alarms / (double)total_alarms);
    printf("  Flood occurrences:         %u\n",
           flood.total_flood_events);
    printf("  Peak 10-min rate:          %u\n",
           flood.peak_alarms_in_flood);
    printf("  Flood duration:            %u sec\n",
           flood.flood_duration_sec);

    double apd = isa18_kpi_calc_alarms_per_day(&kpi, now);
    printf("\n  Alarms per day per op:     %.1f\n", apd);

    double crit, high, med, low;
    isa18_kpi_calc_priority_distribution(&kpi, &crit, &high, &med, &low);
    printf("  Priority distribution:\n");
    printf("    CRITICAL: %.1f%%\n", crit);
    printf("    HIGH:     %.1f%%\n", high);
    printf("    MEDIUM:   %.1f%%\n", med);
    printf("    LOW:      %.1f%%\n", low);

    /* EEMUA 191 Assessment */
    printf("\n--- EEMUA 191 Benchmark Assessment ---\n");
    int scores[7];
    isa18_kpi_assess_eemua_benchmark(&kpi, now, scores);
    const char *score_labels[] = {
        "Alarms/day/op", "Peak rate/10min", "Stale alarms",
        "Priority dist", "Chattering", "Response time",
        "Rationalization"
    };
    const char *score_ratings[] = {
        "UNACCEPTABLE","DEMANDING","MANAGEABLE","ACCEPTABLE","EXCELLENT"
    };
    for (int i = 0; i < 7; i++) {
        printf("  %-20s: %d/4 (%s)\n",
               score_labels[i], scores[i], score_ratings[scores[i]]);
    }

    double health = isa18_kpi_overall_health_score(&kpi, now);
    printf("\n  Composite Health Score: %.1f / 100\n", health);

    if (health >= 75.0) printf("  Status: HEALTHY\n");
    else if (health >= 50.0) printf("  Status: NEEDS IMPROVEMENT\n");
    else printf("  Status: CRITICAL - Immediate action required\n");

    /* KPI Report */
    printf("\n--- Generated KPI Report ---\n");
    char report[4096];
    isa18_kpi_generate_report(&kpi, now, report, sizeof(report));
    printf("%s", report);

    printf("Alarm flood and KPI analysis complete.\n\n");
    return 0;
}