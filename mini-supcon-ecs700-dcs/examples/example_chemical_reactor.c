/**
 * @file    example_chemical_reactor.c
 * @brief   ECS-700 Application: Exothermic Batch Reactor Control
 *
 * Demonstrates ECS-700 DCS capabilities for a chemical reactor:
 *   - Cascade temperature control (jacket → reactor)
 *   - PID with anti-windup for heating/cooling switching
 *   - Interlock for over-temperature protection
 *   - SFC for batch sequence (charge → heat → react → cool → discharge)
 *
 * This is a canonical L6 problem: Reactor Temperature Control
 * References:
 *   - Seborg, Edgar, Mellichamp (2016), Process Dynamics and Control
 *   - Luyben (2007), Chemical Reactor Design and Control
 *
 * Knowledge: L6 Canonical Problems, L7 Industrial Application
 */

#include "ecs700_system_core.h"
#include "ecs700_control_station.h"
#include "ecs700_redundancy.h"
#include "ecs700_io_subsystem.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/* Reactor simulation parameters */
#define REACTOR_VOLUME_L      5000.0
#define REACTOR_HEAT_CAPACITY 4200.0   /* J/kg·K (water-like) */
#define REACTION_HEAT_KW      150.0    /* Exothermic heat release */
#define COOLING_DUTY_MAX_KW   200.0    /* Maximum cooling capacity */
#define JACKET_VOLUME_L       500.0
#define AMBIENT_TEMP_C        25.0

typedef struct {
    double reactor_temp_c;       /* Reactor temperature (primary PV) */
    double jacket_temp_c;        /* Jacket temperature (secondary PV) */
    double cooling_valve_pct;    /* Cooling water valve 0-100% (final output) */
    double feed_temp_c;          /* Feed temperature */
    double feed_rate_kg_s;       /* Feed mass flow rate */
    double reaction_progress;    /* 0.0-1.0 conversion */
    double jacket_setpoint_c;    /* Jacket temperature setpoint (from primary PID) */
} reactor_sim_t;

static void reactor_init(reactor_sim_t *rx)
{
    memset(rx, 0, sizeof(*rx));
    rx->reactor_temp_c = AMBIENT_TEMP_C;
    rx->jacket_temp_c = AMBIENT_TEMP_C;
    rx->feed_temp_c = 50.0;
    rx->feed_rate_kg_s = 2.0;
    rx->reaction_progress = 0.0;
}

static void reactor_simulate(reactor_sim_t *rx, double cooling_pct, double dt)
{
    /* Simplified energy balance:
     * dT/dt = (Q_reaction - Q_cooling + Q_feed) / (mass * Cp) */

    double reactor_mass = REACTOR_VOLUME_L;  /* Assume density ~1 kg/L */
    double q_reaction = rx->reaction_progress * REACTION_HEAT_KW * 1000.0;  /* Watts */
    double q_cooling = (cooling_pct / 100.0) * COOLING_DUTY_MAX_KW * 1000.0;
    double q_feed = rx->feed_rate_kg_s * REACTOR_HEAT_CAPACITY
                  * (rx->feed_temp_c - rx->reactor_temp_c);

    /* Reactor temperature dynamics */
    double dT_reactor = (q_reaction - q_cooling + q_feed)
                      / (reactor_mass * REACTOR_HEAT_CAPACITY) * dt;
    rx->reactor_temp_c += dT_reactor;

    /* Jacket temperature follows cooling water valve */
    double dT_jacket = (rx->reactor_temp_c - rx->jacket_temp_c)
                     * 0.1 * dt;  /* Heat transfer lag */
    rx->jacket_temp_c += dT_jacket;

    /* Reaction progress (simplified first-order kinetics) */
    double rate = 0.01 * (1.0 - rx->reaction_progress);  /* 100s time constant */
    if (rx->reactor_temp_c > 50.0) {
        rate *= 1.0 + 0.05 * (rx->reactor_temp_c - 50.0); /* Temperature acceleration */
    }
    rx->reaction_progress += rate * dt;
    if (rx->reaction_progress > 1.0) rx->reaction_progress = 1.0;
}

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ECS-700 DCS — Chemical Reactor Control Example        ║\n");
    printf("║  Cascade Temperature Control with Batch Sequencing     ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Initialize ECS-700 system */
    ecs700_system_config_t sys_config;
    ecs700_system_init(&sys_config, "Chemical_Plant_Reactor_Area");
    ecs700_domain_register(&sys_config, "Reactor_Domain");
    ecs700_domain_add_cs(&sys_config, 1, 1);

    /* Configure cascade control: Primary(Reactor T) → Secondary(Jacket T) */
    ecs700_cascade_pair_t cascade;
    ecs700_cascade_init(&cascade, "TIC201", "TIC202",
                         1.5, 120.0, 15.0,   /* Primary: slow reactor temp */
                         2.0, 30.0,  0.0,    /* Secondary: faster jacket temp */
                         0.5);                /* 500 ms sample time */

    /* Configure primary (reactor temperature) */
    cascade.primary.setpoint = 80.0;          /* Target: 80°C */
    cascade.primary.action = ECS700_PID_REVERSE_ACTING;
    ecs700_pid_set_output_limits(&cascade.primary, 20.0, 120.0);
    cascade.primary.pv_alarm_hi = 90.0;
    cascade.primary.pv_alarm_hihi = 95.0;
    cascade.primary.mode = ECS700_PID_MODE_AUTO;
    cascade.primary.enabled = true;

    /* Configure secondary (jacket temperature) */
    cascade.secondary.action = ECS700_PID_REVERSE_ACTING;
    ecs700_pid_set_output_limits(&cascade.secondary, 0.0, 100.0);
    cascade.secondary.mode = ECS700_PID_MODE_AUTO;
    cascade.secondary.enabled = true;

    /* Configure over-temperature interlock */
    ecs700_interlock_t overtemperature;
    memset(&overtemperature, 0, sizeof(overtemperature));
    overtemperature.interlock_id = 1;
    strcpy(overtemperature.cause_tag, "TIC201.PV");
    strcpy(overtemperature.effect_tag, "TIC202.OP");
    overtemperature.trigger_condition = 0;  /* Greater than */
    overtemperature.trigger_value = 95.0;   /* 95°C trip */
    overtemperature.safe_output = 100.0;    /* Full cooling on trip */
    overtemperature.requires_reset = true;

    cascade.cascade_enabled = true;

    /* Setup SFC for batch sequence */
    ecs700_sfc_step_t steps[4];
    ecs700_sfc_step_init(&steps[0], 1, "CHARGE", true);
    ecs700_sfc_step_init(&steps[1], 2, "HEAT", false);
    ecs700_sfc_step_init(&steps[2], 3, "REACT", false);
    ecs700_sfc_step_init(&steps[3], 4, "COOL", false);

    /* SFC transitions with dwell times */
    steps[0].num_transitions = 1;
    steps[0].next_step_ids[0] = 2;
    steps[0].max_dwell_time_ms = 30000;  /* 30s max charge */
    steps[1].num_transitions = 1;
    steps[1].next_step_ids[0] = 3;
    steps[2].num_transitions = 1;
    steps[2].next_step_ids[0] = 4;
    steps[2].min_dwell_time_ms = 60000;  /* React for at least 60s */

    /* Initialize reactor simulation */
    reactor_sim_t reactor;
    reactor_init(&reactor);

    printf("  Batch Reactor Control Simulation\n");
    printf("  Target temperature: 80.0°C\n");
    printf("  Over-temperature trip: 95.0°C\n");
    printf("  SFC Sequence: CHARGE → HEAT → REACT → COOL\n\n");

    /* Simulation loop: 100 steps at 1 second intervals */
    double sim_time = 0.0;
    double cooling_valve = 0.0;

    printf("  Time(s)  |  SFC Step  |  Reactor T  |  Jacket T  |  Cooling %%  |  Progress\n");
    printf("  ----------|------------|-------------|------------|-------------|----------\n");

    for (int i = 0; i < 100; i++) {
        /* Execute SFC batch sequence */
        uint16_t active_steps;
        ecs700_sfc_execute(steps, 4, &active_steps);

        /* Determine current SFC step */
        const char *sfc_name = "IDLE";
        for (int s = 0; s < 4; s++) {
            if (steps[s].active) {
                sfc_name = steps[s].step_name;
                break;
            }
        }

        /* Set reactor temperature setpoint based on SFC phase */
        if (steps[0].active) {
            cascade.primary.setpoint = 30.0;  /* Charge: ambient */
        } else if (steps[1].active) {
            cascade.primary.setpoint = 80.0;  /* Heat: ramp to reaction temp */
        } else if (steps[2].active) {
            cascade.primary.setpoint = 80.0;  /* React: maintain */
        } else if (steps[3].active) {
            cascade.primary.setpoint = 30.0;  /* Cool: return to ambient */
        }

        /* Execute cascade control */
        uint64_t time_us = (uint64_t)(sim_time * 1000000.0);
        cooling_valve = ecs700_cascade_execute(&cascade,
                                                reactor.reactor_temp_c,
                                                reactor.jacket_temp_c,
                                                time_us);

        /* Check over-temperature interlock */
        bool interlocked = ecs700_interlock_evaluate(&overtemperature,
                                                      reactor.reactor_temp_c);
        if (interlocked) {
            cooling_valve = overtemperature.safe_output;
        }

        /* Simulate reactor dynamics */
        reactor_simulate(&reactor, cooling_valve, 1.0);

        /* Print status every 5 seconds */
        if (i % 5 == 0) {
            printf("  %8.1f  |  %-10s |  %9.1f°C |  %9.1f°C |  %9.1f%% |  %4.0f%%\n",
                   sim_time, sfc_name,
                   reactor.reactor_temp_c,
                   reactor.jacket_temp_c,
                   cooling_valve,
                   reactor.reaction_progress * 100.0);
        }

        /* Check PID alarms */
        ecs700_pid_check_alarms(&cascade.primary);
        if (cascade.primary.alarm_state & 0x01) {
            printf("  *** REACTOR OVER-TEMPERATURE ALARM at t=%.1fs ***\n", sim_time);
        }

        sim_time += 1.0;
    }

    printf("\n  Simulation complete.\n");
    printf("  Final reactor temperature: %.1f°C\n", reactor.reactor_temp_c);
    printf("  Final reaction progress: %.0f%%\n", reactor.reaction_progress * 100.0);
    printf("  Cascade pair successfully demonstrated.\n\n");

    return 0;
}
