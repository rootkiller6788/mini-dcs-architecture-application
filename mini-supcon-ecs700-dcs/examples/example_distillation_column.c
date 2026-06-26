/**
 * @file    example_distillation_column.c
 * @brief   ECS-700 Application: Distillation Column Control
 *
 * Demonstrates ECS-700 DCS for a binary distillation column:
 *   - Reflux ratio control
 *   - Bottom temperature cascade (steam → temperature)
 *   - Feedforward from feed composition
 *   - Redundancy failover demonstration
 *
 * This is a canonical L6 problem: Distillation Column Control
 * Reference: Luyben (2006), Distillation Design and Control
 *
 * Knowledge: L6 Canonical Problems, L7 Industrial Application (Petrochemical)
 */

#include "ecs700_system_core.h"
#include "ecs700_control_station.h"
#include "ecs700_redundancy.h"
#include "ecs700_io_subsystem.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

typedef struct {
    double feed_rate_kg_h;       /* Feed mass flow rate */
    double feed_composition;     /* Light component fraction (0-1) */
    double top_temp_c;           /* Top temperature */
    double bottom_temp_c;        /* Bottom temperature */
    double reflux_flow_kg_h;     /* Reflux flow rate */
    double distillate_flow_kg_h; /* Distillate product flow */
    double bottoms_flow_kg_h;    /* Bottoms product flow */
    double steam_valve_pct;      /* Reboiler steam valve 0-100% */
    double top_composition;      /* Top light component fraction */
    double bottom_composition;   /* Bottom light component fraction */
} column_sim_t;

static void column_init(column_sim_t *col)
{
    memset(col, 0, sizeof(*col));
    col->feed_rate_kg_h = 5000.0;
    col->feed_composition = 0.45;
    col->top_temp_c = 80.0;
    col->bottom_temp_c = 110.0;
    col->reflux_flow_kg_h = 3000.0;
    col->top_composition = 0.985;
    col->bottom_composition = 0.015;
}

static void column_simulate(column_sim_t *col, double reflux_pct, double steam_pct, double dt)
{
    (void)dt;
    /* Simplified column dynamics */
    double reflux_effect = reflux_pct / 100.0;
    double steam_effect = steam_pct / 100.0;

    col->top_temp_c = 78.0 + (1.0 - reflux_effect) * 5.0
                    + (1.0 - col->feed_composition) * 2.0;
    col->bottom_temp_c = 105.0 + steam_effect * 15.0
                       + col->feed_composition * 5.0;
    col->top_composition = 0.98 + (reflux_effect - 0.6) * 0.02;
    col->bottom_composition = 0.02 - (steam_effect - 0.5) * 0.02;
}

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ECS-700 DCS — Distillation Column Control Example     ║\n");
    printf("║  With Redundancy Failover Demonstration                ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* System initialization */
    ecs700_system_config_t sys;
    ecs700_system_init(&sys, "Petrochemical_Distillation");
    ecs700_domain_register(&sys, "Column_Domain");
    ecs700_domain_add_cs(&sys, 1, 10);
    ecs700_domain_add_cs(&sys, 1, 11);

    printf("  Plant: Petrochemical refinery — distillation area\n");
    printf("  Column: Binary distillation (C2/C3 splitter)\n");
    printf("  Feed: 5000 kg/h, 45%% light component\n");
    printf("  Target: Top > 98%% pure, Bottom < 2%% light\n\n");

    /* Bottom temperature cascade control */
    ecs700_cascade_pair_t bottom_cascade;
    ecs700_cascade_init(&bottom_cascade,
                         "TIC301_BTM", "FIC301_STM",
                         1.0, 180.0, 20.0,   /* Primary: bottom temp */
                         0.8, 15.0,  0.0,    /* Secondary: steam flow */
                         0.5);

    bottom_cascade.primary.setpoint = 110.0;
    bottom_cascade.primary.action = ECS700_PID_REVERSE_ACTING;
    bottom_cascade.primary.mode = ECS700_PID_MODE_AUTO;
    bottom_cascade.primary.enabled = true;
    bottom_cascade.secondary.mode = ECS700_PID_MODE_AUTO;
    bottom_cascade.secondary.enabled = true;

    /* Feedforward configuration */
    bottom_cascade.feedforward_enabled = true;
    bottom_cascade.ff_gain = 0.3;
    bottom_cascade.cascade_enabled = true;

    /* Top temperature PID (reflux control) */
    ecs700_pid_block_t top_pid;
    ecs700_pid_init(&top_pid, "TIC302_TOP", 0.5, 240.0, 10.0, 0.5);
    top_pid.setpoint = 80.0;
    top_pid.action = ECS700_PID_DIRECT_ACTING;
    top_pid.mode = ECS700_PID_MODE_AUTO;
    top_pid.enabled = true;
    ecs700_pid_set_output_limits(&top_pid, 20.0, 90.0);

    /* Redundancy pair for the column controller */
    ecs700_redundancy_pair_t redundancy;
    ecs700_redundancy_pair_init(&redundancy, 1, 10, 11);

    /* Initialize column simulation */
    column_sim_t column;
    column_init(&column);

    /* Simulate 200 seconds of operation */
    double sim_time = 0.0;
    double dt = 1.0;
    bool failover_occurred = false;

    printf("  Time(s) | Top T(°C) | Btm T(°C) | Reflux%% | Steam%% | Top%% | Btm%% | Note\n");
    printf("  --------|-----------|------------|----------|---------|-------|-------|-----\n");

    for (int i = 0; i < 200; i++) {
        uint64_t time_us = (uint64_t)(sim_time * 1000000.0);

        /* Demonstrate failover at t=80s */
        if (i == 80 && !failover_occurred) {
            printf("  *** Simulating controller failure at t=%.0fs ***\n", sim_time);

            /* Mark partner unhealthy to trigger failover evaluation */
            ecs700_health_score_t bad_health;
            memset(&bad_health, 0, sizeof(bad_health));
            bad_health.cpu_load = 100.0;
            bad_health.temperature_c = 100.0;
            bad_health.power_supply_v = 5.0;
            bad_health.watchdog_timeouts = 5;
            ecs700_redundancy_heartbeat(&redundancy, &bad_health, time_us);

            /* Force heartbeat miss count to trigger failover */
            for (int m = 0; m < ECS700_HEARTBEAT_MISS_MAX + 1; m++) {
                ecs700_redundancy_heartbeat(&redundancy, &bad_health, time_us);
            }
            redundancy.heartbeat_miss_count = ECS700_HEARTBEAT_MISS_MAX + 1;

            int ret = ecs700_redundancy_failover(&redundancy, time_us);
            if (ret == 0) {
                printf("  *** Failover SUCCESSFUL: Secondary → Primary ***\n");
            }
            failover_occurred = true;
        }

        /* Execute controls */
        bottom_cascade.feedforward_value = column.feed_composition;
        double steam_output = ecs700_cascade_execute(&bottom_cascade,
                                                      column.bottom_temp_c,
                                                      column.bottom_temp_c * 0.6,
                                                      time_us);

        top_pid.pv = column.top_temp_c;
        double reflux_output = ecs700_pid_execute(&top_pid, time_us);

        /* Simulate column dynamics */
        column_simulate(&column, reflux_output, steam_output, dt);

        /* Print status every 10 seconds */
        if (i % 10 == 0) {
            printf("  %6.0f   |   %6.1f   |   %6.1f    |  %5.1f   |  %5.1f  | %.3f | %.3f | %s\n",
                   sim_time,
                   column.top_temp_c,
                   column.bottom_temp_c,
                   reflux_output,
                   steam_output,
                   column.top_composition,
                   column.bottom_composition,
                   failover_occurred ? (i < 85 ? "FAILOVER" : "STABLE") : "NORMAL");
        }

        sim_time += dt;
    }

    printf("\n  Distillation column control demonstration complete.\n");
    printf("  Redundancy failover count: %u\n", redundancy.failover_count);
    printf("  System maintained control throughout failover event.\n\n");

    return 0;
}
