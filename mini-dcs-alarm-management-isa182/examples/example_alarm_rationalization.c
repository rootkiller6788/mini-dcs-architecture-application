/**
 * @file example_alarm_rationalization.c
 * @brief ISA-18.2 Alarm Rationalization Example (L6)
 *
 * Demonstrates the complete ISA-18.2 alarm rationalization workflow:
 *   1. Create a Master Alarm Database (MAD)
 *   2. Add alarm candidates (potential alarms)
 *   3. Run rationalization: assign priority, class, severity, urgency
 *   4. Apply rationalization outcomes to alarm configurations
 *   5. Validate MAD consistency
 *   6. Export MAD summary and rationalization coverage
 *
 * This implements ISA-18.2 §9 and demonstrates the B->C->D lifecycle
 * stages (Identification -> Rationalization -> Detailed Design).
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "alarm_management_types.h"
#include "alarm_rationalization.h"

int main(void) {
    printf("\n========================================\n");
    printf(" ISA-18.2 ALARM RATIONALIZATION EXAMPLE\n");
    printf("========================================\n\n");

    /* Step 1: Initialize Master Alarm Database */
    isa18_master_alarm_database_t mad;
    isa18_mad_init(&mad, "Ethylene Cracker Unit", "PHIL-CRK-2024-Rev3");
    printf("Master Alarm Database created for: %s\n", mad.site_name);
    printf("Philosophy document: %s\n\n", mad.philosophy_doc_ref);

    /* Step 2: Add alarm candidates */
    const char *tags[] = {
        "TIC-101", "TIC-102", "PIC-201", "FIC-301",
        "LIC-401", "AIC-501", "TIC-103", "PIC-202"
    };
    const char *descriptions[] = {
        "Reactor bed temperature high",
        "Reactor outlet temperature high",
        "Compressor discharge pressure high",
        "Ethylene feed flow low",
        "Quench tower level low",
        "Oxygen analyzer high",
        "Regenerator temperature high",
        "Compressor suction pressure low"
    };

    int num_alarms = sizeof(tags) / sizeof(tags[0]);
    uint32_t alarm_ids[8];

    for (int i = 0; i < num_alarms; i++) {
        isa18_alarm_config_t alarm;
        isa18_alarm_config_init(&alarm, 0, tags[i]);
        strncpy(alarm.description, descriptions[i],
                sizeof(alarm.description) - 1);
        isa18_alarm_config_set_high(&alarm, 80.0 + i * 5.0, 2.0);
        alarm_ids[i] = isa18_mad_add_alarm(&mad, &alarm);
        printf("  Added alarm #%u: %-8s - %s\n",
               alarm_ids[i], tags[i], descriptions[i]);
    }
    printf("\n  Total alarms in MAD: %u (0 rationalized)\n\n", mad.alarm_count);

    /* Step 3: Perform rationalization for each alarm */
    printf("--- Rationalization Workshop ---\n");
    isa18_rationalization_record_t records[8];

    /* TIC-101: Critical safety alarm (reactor runaway) */
    printf("\n  Rationalizing TIC-101 (Reactor Temperature):\n");
    isa18_rationalization_init_record(&records[0], 1, alarm_ids[0], "TIC-101");
    isa18_rationalization_team_add_member(&records[0], "J. Smith (Process Eng)");
    isa18_rationalization_team_add_member(&records[0], "A. Jones (Operator)");
    isa18_rationalization_team_add_member(&records[0], "M. Lee (Safety Eng)");

    isa18_alarm_priority_t pri0 = isa18_assign_priority_matrix(
        ISA18_SEVERITY_CRITICAL, ISA18_URGENCY_IMMEDIATE);
    isa18_rationalization_set_outcome(&records[0],
        ISA18_CLASS_ALARM, pri0,
        ISA18_SEVERITY_CRITICAL, ISA18_URGENCY_IMMEDIATE,
        true, "Reactor bed overheating can cause runaway exotherm. "
              "Requires immediate feed cut and quench activation.");
    printf("    Result: %s, %s, %s (justified: %s)\n",
           isa18_priority_to_string(records[0].priority),
           isa18_alarm_class_to_string(records[0].alarm_class),
           isa18_severity_to_string(records[0].severity),
           records[0].is_justified ? "YES" : "NO");
    printf("    Team: %u members\n", records[0].team_count);

    /* PIC-201: High priority (compressor trip risk) */
    isa18_rationalization_init_record(&records[1], 2, alarm_ids[2], "PIC-201");
    isa18_rationalization_team_add_member(&records[1], "J. Smith (Process Eng)");
    isa18_alarm_priority_t pri1 = isa18_assign_priority_matrix(
        ISA18_SEVERITY_SEVERE, ISA18_URGENCY_PROMPT);
    isa18_rationalization_set_outcome(&records[1],
        ISA18_CLASS_ALARM, pri1,
        ISA18_SEVERITY_SEVERE, ISA18_URGENCY_PROMPT,
        true, "Compressor overpressure can cause mechanical damage.");
    printf("\n  Rationalizing PIC-201: %s\n",
           isa18_priority_to_string(records[1].priority));

    /* TIC-102: Medium priority */
    isa18_rationalization_init_record(&records[2], 3, alarm_ids[1], "TIC-102");
    isa18_alarm_priority_t pri2 = isa18_assign_priority_matrix(
        ISA18_SEVERITY_MAJOR, ISA18_URGENCY_RAPID);
    isa18_rationalization_set_outcome(&records[2],
        ISA18_CLASS_ALARM, pri2,
        ISA18_SEVERITY_MAJOR, ISA18_URGENCY_RAPID,
        true, "Outlet temperature affects downstream separation efficiency.");
    printf("\n  Rationalizing TIC-102: %s\n",
           isa18_priority_to_string(records[2].priority));

    /* FIC-301: Classified as ALERT (not an alarm) */
    isa18_rationalization_init_record(&records[3], 4, alarm_ids[3], "FIC-301");
    isa18_alarm_priority_t pri3 = isa18_assign_priority_matrix(
        ISA18_SEVERITY_MODERATE, ISA18_URGENCY_NON_URGENT);
    isa18_rationalization_set_outcome(&records[3],
        ISA18_CLASS_ALERT, pri3,
        ISA18_SEVERITY_MODERATE, ISA18_URGENCY_NON_URGENT,
        false, "Low feed flow is compensated by ratio controller. "
               "Reclassified as ALERT - no operator action required.");
    printf("\n  Rationalizing FIC-301: %s (reclassified to ALERT)\n",
           isa18_alarm_class_to_string(records[3].alarm_class));

    /* Step 4: Apply rationalization outcomes */
    printf("\n--- Applying Rationalization Outcomes ---\n");
    for (int i = 0; i < 4; i++) {
        isa18_alarm_config_t *alarm = isa18_mad_find_by_id(
            &mad, records[i].alarm_id);
        if (alarm) {
            isa18_rationalization_apply_to_alarm(&records[i], alarm);
            printf("  Applied to %s: priority=%s, rationalized=%s\n",
                   alarm->tag_name,
                   isa18_priority_to_string(alarm->priority),
                   alarm->is_rationalized ? "YES" : "NO");
        }
    }

    /* Step 5: Validate MAD */
    printf("\n--- MAD Validation ---\n");
    char errors[20][ISA18_MAX_MESSAGE_LEN];
    uint32_t err_count;
    isa18_mad_validate(&mad, &err_count, errors, 20);
    if (err_count == 0) {
        printf("  MAD validation: PASS (no errors)\n");
    } else {
        printf("  MAD validation: %u errors found\n", err_count);
        for (uint32_t i = 0; i < err_count && i < 5; i++) {
            printf("    - %s\n", errors[i]);
        }
    }

    /* Step 6: Summary statistics */
    printf("\n========================================\n");
    printf(" RATIONALIZATION SUMMARY\n");
    printf("========================================\n");
    printf("  Total alarms:            %u\n", mad.alarm_count);
    printf("  Rationalized:            %u\n", mad.rationalized_count);
    printf("  Unrationalized:          %u\n",
           isa18_mad_get_unrationalized_count(&mad));
    printf("  Coverage:                %.1f%%\n",
           isa18_mad_calc_rationalization_coverage(&mad));
    printf("  Alarms reclassified:     1 (FIC-301 -> ALERT)\n");

    uint32_t crit, high, med, low;
    isa18_mad_count_by_priority(&mad, &crit, &high, &med, &low);
    printf("  Priority distribution:\n");
    printf("    CRITICAL: %u\n", crit);
    printf("    HIGH:     %u\n", high);
    printf("    MEDIUM:   %u\n", med);
    printf("    LOW:      %u\n", low);

    printf("\n  Next steps:\n");
    printf("    1. Complete rationalization for remaining %u alarms\n",
           isa18_mad_get_unrationalized_count(&mad));
    printf("    2. Configure detailed design (D: setpoints, deadbands)\n");
    printf("    3. Implement in DCS (E: download to controllers)\n");
    printf("    4. Begin operation monitoring (H: KPI tracking)\n");

    printf("\nRationalization example complete.\n\n");
    return 0;
}