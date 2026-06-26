/**
 * @file example_alarm_engine_live.c
 * @brief ISA-18.2 Live Alarm Engine Simulation (L6)
 *
 * Simulates a running alarm system with 6 process variables over
 * 30 time steps. Demonstrates:
 *   - Real-time alarm detection (high, low, deviation, bad PV)
 *   - 5-state alarm state machine transitions
 *   - Operator acknowledgment simulation
 *   - Alarm shelving and unshelving
 *   - Annunciator display with active alarms
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "alarm_management_types.h"
#include "alarm_engine.h"
#include "alarm_shelving_suppression.h"

int main(void) {
    printf("\n========================================\n");
    printf(" ISA-18.2 LIVE ALARM ENGINE SIMULATION\n");
    printf("========================================\n\n");

    isa18_alarm_config_t configs[6];
    isa18_alarm_system_runtime_t runtime;
    double process_values[6];

    /* Configure 6 alarms */
    const char *tags[] = {"TIC-101","PIC-201","LIC-301","FIC-401","AIC-501","TIC-102"};
    const char *desc[] = {
        "Reactor temperature H", "Discharge pressure H",
        "Tank level L", "Feed flow L",
        "O2 analyzer H", "Reactor temp HH"
    };
    double setpoints[] = {80.0, 120.0, 60.0, 50.0, 8.0, 95.0};
    isa18_alarm_type_t types[] = {
        ISA18_TYPE_HIGH, ISA18_TYPE_HIGH, ISA18_TYPE_LOW,
        ISA18_TYPE_LOW, ISA18_TYPE_HIGH, ISA18_TYPE_HI_HI
    };

    for (int i = 0; i < 6; i++) {
        isa18_alarm_config_init(&configs[i], i+1, tags[i]);
        strncpy(configs[i].description, desc[i], sizeof(configs[i].description)-1);
        configs[i].alarm_type = types[i];
        configs[i].setpoint = setpoints[i];
        configs[i].deadband = 2.0;
        configs[i].on_delay_ms = 0;
        configs[i].is_rationalized = true;
        configs[i].priority = (i == 5) ? ISA18_PRIORITY_CRITICAL :
                             (i < 2) ? ISA18_PRIORITY_HIGH :
                             ISA18_PRIORITY_MEDIUM;
    }

    isa18_engine_runtime_init(&runtime, configs, 6);

    /* Simulate 30 time steps with process disturbances */
    printf("Simulating 30 time steps (1s = 1 scan)...\n");
    printf("%-4s %-8s %-8s %-8s %-8s %-8s %-8s | %-12s %-4s\n",
           "Step","TIC-101","PIC-201","LIC-301","FIC-401","AIC-501","TIC-102",
           "Events","Active");
    printf("---- -------- -------- -------- -------- -------- -------- | ------------ ----\n");

    /* Predefined process scenarios */
    double pv_data[30][6] = {
        {75,110,70,60,5,85},   /* 0: normal */
        {78,115,68,58,6,88},   /* 1: normal */
        {82,118,65,55,7,90},   /* 2: TIC-101 trips HIGH */
        {85,122,62,52,8,92},   /* 3: PIC-201 trips HIGH */
        {83,125,60,48,9,94},   /* 4: FIC-401 trips LOW */
        {81,123,58,45,10,96},  /* 5: TIC-102 trips HI_HI */
        {79,121,55,44,11,97},  /* 6: all active */
        {78,120,57,46,9,95},   /* 7: start recovery */
        {76,118,62,50,7,93},   /* 8 */
        {74,115,65,52,6,91},   /* 9 */
        {73,112,68,55,5,89},   /* 10 */
        {72,110,70,58,5,87},   /* 11: mostly normal */
        {75,112,68,60,6,88},   /* 12 */
        {78,115,65,62,7,90},   /* 13 */
        {82,118,62,64,8,92},   /* 14: TIC-101 trips again */
        {85,122,60,66,9,94},   /* 15 */
        {88,125,58,68,10,96},  /* 16 */
        {90,128,55,70,11,98},  /* 17: operator flood situation */
        {88,130,52,72,12,100}, /* 18 */
        {85,128,50,74,11,99},  /* 19: LIC-301, FIC-401 both low */
        {82,126,52,76,10,97},  /* 20 */
        {80,124,55,78,9,95},   /* 21 */
        {78,122,58,75,8,93},   /* 22 */
        {76,120,62,70,7,91},   /* 23: recovery */
        {74,118,65,65,6,89},   /* 24 */
        {72,116,68,60,5,87},   /* 25 */
        {75,114,70,58,6,88},   /* 26 */
        {78,112,72,55,7,90},   /* 27 */
        {80,110,75,52,8,92},   /* 28 */
        {82,112,78,50,9,94}    /* 29: TIC-101 trips */
    };

    time_t sim_time = time(NULL);
    uint32_t total_acks = 0;
    uint32_t total_shelves = 0;

    for (int step = 0; step < 30; step++) {
        for (int i = 0; i < 6; i++) process_values[i] = pv_data[step][i];

        uint32_t transitions = isa18_engine_scan(
            &runtime, process_values, 6, sim_time + step);

        /* Operator actions at certain steps */
        if (step == 5) {
            /* Acknowledge TIC-101 */
            isa18_engine_acknowledge(&configs[0], "OP_SMITH", sim_time + step);
            total_acks++;
        }
        if (step == 7) {
            /* Acknowledge PIC-201 and TIC-102 */
            isa18_engine_acknowledge(&configs[1], "OP_SMITH", sim_time + step);
            isa18_engine_acknowledge(&configs[5], "OP_SMITH", sim_time + step);
            total_acks += 2;
        }
        if (step == 17) {
            /* Shelve AIC-501 (nuisance during startup) */
            isa18_alarm_shelve_t sv[10];
            uint32_t sc = runtime.shelved_count;
            uint32_t sid = isa18_shelve_alarm(
                &configs[4], runtime.shelved, &sc,
                10, "O2 analyzer under calibration",
                "OP_SMITH", "SUP_JONES", 3600, sim_time + step);
            if (sid > 0) {
                runtime.shelved_count = sc;
                total_shelves++;
            }
        }

        /* Display step summary */
        char state_chars[7][4]; /* 6 alarms + null */
        for (int i = 0; i < 6; i++) {
            switch(configs[i].current_state) {
            case ISA18_ALARM_STATE_NORMAL:       strcpy(state_chars[i], " - "); break;
            case ISA18_ALARM_STATE_ACTIVE_UNACK: strcpy(state_chars[i], "!U!"); break;
            case ISA18_ALARM_STATE_ACTIVE_ACK:   strcpy(state_chars[i], " A "); break;
            case ISA18_ALARM_STATE_RTN_UNACK:    strcpy(state_chars[i], " R "); break;
            case ISA18_ALARM_STATE_CLEARED:      strcpy(state_chars[i], " C "); break;
            }
        }

        printf("%-4d %-8s %-8s %-8s %-8s %-8s %-8s | %-12u %-4u\n",
               step, state_chars[0], state_chars[1], state_chars[2],
               state_chars[3], state_chars[4], state_chars[5],
               transitions, runtime.active_alarms);

        if (step == 29) {
            /* Un-shelve AIC-501 */
            isa18_unshelve_alarm(&configs[4], runtime.shelved,
                                &runtime.shelved_count, "OP_SMITH",
                                sim_time + step);
        }
    }

    printf("\n========================================\n");
    printf(" SIMULATION SUMMARY\n");
    printf("========================================\n");
    printf("  Total state transitions: monitored across 30 steps\n");
    printf("  Operator acknowledgements: %u\n", total_acks);
    printf("  Shelving operations: %u\n", total_shelves);
    printf("  Flood events detected: %u\n",
           runtime.flood_detector.total_flood_events);
    printf("  Final active alarms: %u\n", runtime.active_alarms);

    printf("\n  Legend:  -  =NORMAL  !U!=ACTIVE_UNACK   A =ACTIVE_ACK\n");
    printf("           R  =RTN_UNACK   C =CLEARED\n");
    printf("\nLive engine simulation complete.\n\n");
    return 0;
}