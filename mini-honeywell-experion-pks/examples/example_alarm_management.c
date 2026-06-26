/**
 * @file example_alarm_management.c
 * @brief Example: ISA-18.2 Alarm Management and ISA-101 HMI
 * L6: Alarm management canonical problem
 * L4: ISA-18.2 / ISA-101 standards
 * Course: Purdue ECE 602, RWTH Aachen Industrial Control
 */
#include "../include/hmiweb_display.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void)
{
    printf("=== Example: ISA-18.2 Alarm Management & ISA-101 HMI ===\n\n");
    /* Initialize station */
    ExperionStation stn;
    station_init(&stn, 1, "EST_CRACKER_OP1");
    station_connect(&stn, 1);
    printf("Station: %s connected\n", stn.station_name);
    /* Operator login */
    operator_login(&stn, "OP_ANNA", "secure123");
    printf("User: %s, Level=%d, Logged=%s\n",
           stn.current_user.user_name, (int)stn.current_user.level,
           stn.current_user.logged_in?"YES":"NO");
    printf("Access: VIEW=%s OP=%s ENG=%s\n",
           operator_has_access(&stn.current_user, HMI_SEC_VIEW)?"Y":"N",
           operator_has_access(&stn.current_user, HMI_SEC_OPERATOR)?"Y":"N",
           operator_has_access(&stn.current_user, HMI_SEC_ENGINEER)?"Y":"N");
    /* Alarm lifecycle (ISA-18.2) */
    printf("\n--- ISA-18.2 Alarm Lifecycle ---\n");
    HMIAlarmSummary *alarms = &stn.alarm_summary;
    alarms->alarm_flood_threshold = 10;
    HMIAlarmRecord rec;
    memset(&rec, 0, sizeof(rec));
    strcpy(rec.tag, "PIC001");
    strcpy(rec.description, "Reactor pressure high");
    rec.priority = HMI_ALARM_PRI_HIGH;
    rec.requires_ack = true;
    rec.state = HMI_ALARM_STATE_UNACK_ACTIVE;
    rec.alarm_value = 15.7;
    rec.alarm_limit = 15.0;
    alarm_add(alarms, &rec);
    printf("ALARM: %s - %s (Pri=%d) - UNACK_ACTIVE\n",
           rec.tag, rec.description, (int)rec.priority);
    printf("  Unacknowledged: %d\n", alarms->unacknowledged_count);
    /* Acknowledge */
    alarm_acknowledge(alarms, 0, "OP_ANNA");
    printf("  ACK by OP_ANNA -> ACK_ACTIVE\n");
    /* Highest priority check */
    HMIAlarmPriority highest;
    alarm_get_highest_priority(alarms, &highest);
    printf("  Highest active priority: %d\n", (int)highest);
    /* Shelve and unshelve */
    alarm_shelve(alarms, 0, 30);
    printf("  Shelved for 30min, shelved_count=%d\n", alarms->shelved_count);
    alarm_unshelve(alarms, 0);
    printf("  Unshelved, shelved_count=%d\n", alarms->shelved_count);
    /* ISA-101 Faceplate */
    printf("\n--- ISA-101 High-Performance HMI Faceplate ---\n");
    HMIFaceplate fp;
    memset(&fp, 0, sizeof(fp));
    strcpy(fp.tag, "PIC001");
    strcpy(fp.description, "Reactor Pressure Control");
    strcpy(fp.eu, "bar");
    fp.pv_hi = 30.0; fp.pv_lo = 0.0;
    fp.op_hi = 100.0; fp.op_lo = 0.0;
    fp.sp_hi_limit = 25.0; fp.sp_lo_limit = 5.0;
    fp.deviation_hi = 2.0;
    /* Normal operation */
    faceplate_update(&fp, 12.0, 12.0, 45.0, PID_AUTO, XQUAL_GOOD);
    faceplate_isa101_color(&fp);
    printf("Normal:   PV=%.1f %s SP=%.1f OP=%.1f%% [%s]\n",
           fp.pv, fp.eu, fp.sp, fp.op, fp.op_bar_graph);
    printf("  ISA-101 colors: PV=%d (black=normal) OP=%d (black=normal)\n",
           (int)fp.pv_color, (int)fp.op_color);
    /* Deviation alarm */
    faceplate_update(&fp, 16.5, 12.0, 70.0, PID_AUTO, XQUAL_GOOD);
    faceplate_isa101_color(&fp);
    printf("Deviation: PV=%.1f, dev=%.1f > %.1f, color=%d (alarm)\n",
           fp.pv, fabs(fp.pv-fp.sp), fp.deviation_hi, (int)fp.pv_color);
    /* Manual mode (operator-entered, blue) */
    faceplate_set_mode(&fp, PID_MANUAL, HMI_SEC_OPERATOR);
    faceplate_set_op(&fp, 65.0, HMI_SEC_OPERATOR);
    faceplate_update(&fp, 12.0, 12.0, 65.0, PID_MANUAL, XQUAL_GOOD);
    faceplate_isa101_color(&fp);
    printf("Manual:   OP=%.1f%% (operator), color=%d (blue)\n",
           fp.op, (int)fp.op_color);
    /* Compliance check */
    printf("  ISA-101 compliant: %s\n",
           isa101_verify_colors(&fp)?"YES":"NO");
    /* Logout */
    operator_logout(&stn);
    printf("\nOperator logged out.\n");
    printf("=== Alarm Management Example Complete ===\n");
    return 0;
}
