/**
 * @file    example_boiler_control.c
 * @brief   ECS-700 Application: Industrial Boiler Control with Burner Management
 *
 * Demonstrates ECS-700 DCS for a power plant boiler:
 *   - Three-element drum level control (feedforward + cascade)
 *   - Combustion control (fuel-air ratio)
 *   - Burner Management System (BMS) interlocks
 *   - I/O subsystem with NAMUR NE43 fault detection
 *   - SCnet data exchange for operator station
 *
 * This is a canonical L6 problem: Boiler Control
 * Reference: Dukelow (1991), The Control of Boilers
 *
 * Knowledge: L6 Canonical Problems, L7 Industrial Application (Power Generation)
 */

#include "ecs700_system_core.h"
#include "ecs700_control_station.h"
#include "ecs700_redundancy.h"
#include "ecs700_io_subsystem.h"
#include "ecs700_communication.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

typedef struct {
    double drum_level_pct;        /* Drum water level 0-100% */
    double steam_flow_kg_h;       /* Steam output flow */
    double feedwater_flow_kg_h;   /* Feedwater input flow */
    double drum_pressure_mpa;     /* Drum pressure */
    double fuel_flow_kg_h;        /* Fuel flow rate */
    double air_flow_kg_h;         /* Combustion air flow */
    double excess_o2_pct;         /* Excess O2 in flue gas */
    double furnace_pressure_pa;   /* Furnace draft pressure */
    double burner_status;         /* 0=off, 1=on */
} boiler_sim_t;

static void boiler_init(boiler_sim_t *b)
{
    memset(b, 0, sizeof(*b));
    b->drum_level_pct = 50.0;
    b->steam_flow_kg_h = 10000.0;
    b->feedwater_flow_kg_h = 10000.0;
    b->drum_pressure_mpa = 4.0;
    b->fuel_flow_kg_h = 500.0;
    b->air_flow_kg_h = 7500.0;
    b->excess_o2_pct = 3.5;
    b->furnace_pressure_pa = -50.0;
    b->burner_status = 1.0;
}

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ECS-700 DCS — Industrial Boiler Control Example       ║\n");
    printf("║  Three-Element Drum Level + Burner Management          ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* System initialization */
    ecs700_system_config_t sys;
    ecs700_system_init(&sys, "Power_Plant_Boiler_1");
    ecs700_domain_register(&sys, "Boiler_Domain");
    ecs700_domain_add_cs(&sys, 1, 1);

    printf("  Plant: Coal-fired power plant, 300 MW unit\n");
    printf("  Boiler: Natural circulation, drum type\n");
    printf("  Steam: 10000 kg/h at 4.0 MPa\n\n");

    /* Three-element drum level control */
    ecs700_cascade_pair_t drum_control;
    ecs700_cascade_init(&drum_control,
                         "LIC401_DRUM", "FIC401_FW",
                         0.8, 120.0, 0.0,    /* Primary: drum level (PI only) */
                         1.5, 10.0,  0.0,    /* Secondary: feedwater flow */
                         0.5);

    drum_control.primary.setpoint = 50.0;   /* 50% drum level */
    drum_control.primary.action = ECS700_PID_DIRECT_ACTING;
    drum_control.primary.mode = ECS700_PID_MODE_AUTO;
    drum_control.primary.enabled = true;
    drum_control.secondary.mode = ECS700_PID_MODE_AUTO;
    drum_control.secondary.enabled = true;

    /* Feedforward from steam flow for three-element control */
    drum_control.feedforward_enabled = true;
    drum_control.ff_gain = 1.0;   /* One-to-one steam/feedwater ratio */
    drum_control.cascade_enabled = true;

    /* Fuel-air ratio control */
    ecs700_pid_block_t o2_control;
    ecs700_pid_init(&o2_control, "AIC401_O2", 0.3, 60.0, 0.0, 1.0);
    o2_control.setpoint = 3.5;   /* 3.5% excess O2 */
    o2_control.action = ECS700_PID_DIRECT_ACTING;
    o2_control.mode = ECS700_PID_MODE_AUTO;
    o2_control.enabled = true;
    ecs700_pid_set_output_limits(&o2_control, 5000.0, 10000.0);

    /* Furnace pressure control */
    ecs700_pid_block_t pressure_control;
    ecs700_pid_init(&pressure_control, "PIC401_FURN", 0.5, 30.0, 0.0, 0.5);
    pressure_control.setpoint = -50.0;  /* -50 Pa slight vacuum */
    pressure_control.action = ECS700_PID_REVERSE_ACTING;
    pressure_control.mode = ECS700_PID_MODE_AUTO;
    pressure_control.enabled = true;

    /* BMS interlocks */
    ecs700_interlock_t low_water_level;
    memset(&low_water_level, 0, sizeof(low_water_level));
    low_water_level.interlock_id = 101;
    strcpy(low_water_level.cause_tag, "LIC401_DRUM.PV");
    strcpy(low_water_level.effect_tag, "BURNER.CMD");
    low_water_level.trigger_condition = 2;  /* Less than */
    low_water_level.trigger_value = 20.0;   /* 20% → trip */
    low_water_level.safe_output = 0.0;      /* Shut off burner */
    low_water_level.requires_reset = true;

    ecs700_interlock_t high_pressure;
    memset(&high_pressure, 0, sizeof(high_pressure));
    high_pressure.interlock_id = 102;
    strcpy(high_pressure.cause_tag, "PI401.PV");
    strcpy(high_pressure.effect_tag, "BURNER.CMD");
    high_pressure.trigger_condition = 0;    /* Greater than */
    high_pressure.trigger_value = 4.5;       /* 4.5 MPa → trip */
    high_pressure.safe_output = 0.0;
    high_pressure.requires_reset = true;

    /* I/O subsystem setup */
    ecs700_io_module_t ai_module;
    ecs700_io_module_init(&ai_module, 1, ECS700_MODULE_AI711, 1, "Boiler_AI");

    ecs700_io_channel_t drum_level_ch;
    ecs700_io_channel_init(&drum_level_ch, 0, ECS700_SIGNAL_AI_4_20MA,
                            0.0, 100.0, "%", "LIC401");
    ai_module.channels[0] = drum_level_ch;
    ai_module.num_enabled_channels = 4;

    /* SCnet data exchange setup */
    ecs700_rt_data_block_t rt_blocks[4];
    ecs700_rt_data_init(&rt_blocks[0], "LIC401", 1);
    ecs700_rt_data_init(&rt_blocks[1], "FIC401", 1);
    ecs700_rt_data_init(&rt_blocks[2], "AIC401", 1);
    ecs700_rt_data_init(&rt_blocks[3], "PIC401", 1);

    /* Initialize boiler simulation */
    boiler_sim_t boiler;
    boiler_init(&boiler);

    printf("  Simulation: 200 seconds of boiler operation\n");
    printf("  Controls: 3-element drum level, O2 trim, furnace pressure\n");
    printf("  Interlocks: Low water level (20%%), High pressure (4.5 MPa)\n\n");

    printf("  Time(s) | Level%% | Steam t/h | FW t/h | O2%% | Press MPa | Status\n");
    printf("  --------|--------|-----------|--------|------|-----------|-------\n");

    double sim_time = 0.0;
    bool boiler_tripped = false;

    for (int i = 0; i < 200; i++) {
        uint64_t time_us = (uint64_t)(sim_time * 1000000.0);

        /* Simulate a steam demand increase at t=100s */
        if (i == 100) {
            boiler.steam_flow_kg_h = 12000.0;  /* 20% load increase */
            printf("  *** Steam demand increased to 12000 kg/h ***\n");
        }

        /* Execute 3-element drum level control */
        drum_control.feedforward_value = boiler.steam_flow_kg_h;
        double fw_output = ecs700_cascade_execute(&drum_control,
                                                   boiler.drum_level_pct,
                                                   boiler.feedwater_flow_kg_h,
                                                   time_us);

        /* Execute O2 trim control */
        o2_control.pv = boiler.excess_o2_pct;
        double air_output = ecs700_pid_execute(&o2_control, time_us);

        /* Execute furnace pressure control */
        pressure_control.pv = boiler.furnace_pressure_pa;
        ecs700_pid_execute(&pressure_control, time_us);

        /* Check BMS interlocks */
        bool low_water_trip = ecs700_interlock_evaluate(&low_water_level,
                                                         boiler.drum_level_pct);
        bool high_press_trip = ecs700_interlock_evaluate(&high_pressure,
                                                          boiler.drum_pressure_mpa);

        if ((low_water_trip || high_press_trip) && !boiler_tripped) {
            printf("  *** BMS INTERLOCK TRIP at t=%.1fs ***\n", sim_time);
            if (low_water_trip) printf("       Reason: Low drum water level\n");
            if (high_press_trip) printf("       Reason: High drum pressure\n");
            boiler.burner_status = 0.0;
            boiler_tripped = true;
        }

        /* Update SCnet data exchange */
        ecs700_rt_data_update(&rt_blocks[0], boiler.drum_level_pct,
                               ECS700_IO_QUALITY_GOOD, time_us);
        ecs700_rt_data_update(&rt_blocks[1], boiler.feedwater_flow_kg_h,
                               ECS700_IO_QUALITY_GOOD, time_us);
        ecs700_rt_data_update(&rt_blocks[2], boiler.excess_o2_pct,
                               ECS700_IO_QUALITY_GOOD, time_us);
        ecs700_rt_data_update(&rt_blocks[3], boiler.drum_pressure_mpa,
                               ECS700_IO_QUALITY_GOOD, time_us);

        /* Simulate boiler dynamics */
        boiler.feedwater_flow_kg_h = fw_output;
        boiler.air_flow_kg_h = air_output;
        boiler.drum_level_pct = 50.0 + (fw_output - boiler.steam_flow_kg_h / 100.0) * 0.01;

        /* NAMUR NE43 fault simulation: inject open-wire at t=150 */
        if (i == 150) {
            uint16_t fault_raw = 0;  /* Simulate 0 mA = open wire */
            ecs700_io_process_input(&drum_level_ch, fault_raw, time_us);
        }

        /* Clamp physics */
        if (boiler.drum_level_pct > 100.0) boiler.drum_level_pct = 100.0;
        if (boiler.drum_level_pct < 0.0) boiler.drum_level_pct = 0.0;

        /* Print status */
        if (i % 10 == 0) {
            printf("  %6.0f   |  %4.1f  |   %6.1f  |  %5.1f  | %3.1f |    %5.2f   | %s\n",
                   sim_time,
                   boiler.drum_level_pct,
                   boiler.steam_flow_kg_h / 1000.0,
                   boiler.feedwater_flow_kg_h / 1000.0,
                   boiler.excess_o2_pct,
                   boiler.drum_pressure_mpa,
                   boiler_tripped ? "TRIPPED" : "RUNNING");
        }

        sim_time += 1.0;
    }

    /* Health report */
    ecs700_system_health_t health;
    ecs700_collect_health(&sys, &health);
    printf("\n  System Health Summary:\n");
    printf("    Active domains:       %u\n", health.active_domains);
    printf("    Primary controllers:  %u\n", health.primary_controllers);
    printf("    SCnet A utilization:  %.1f%%\n", health.scnet_a_utilization);
    printf("    BMS interlock events: %d\n", boiler_tripped ? 1 : 0);

    printf("\n  Boiler control demonstration complete.\n");
    printf("  DCS maintained safe operation with BMS protection.\n\n");

    return 0;
}
