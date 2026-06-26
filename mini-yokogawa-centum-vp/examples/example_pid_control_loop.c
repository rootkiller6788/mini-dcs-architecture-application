/**
 * @file example_pid_control_loop.c
 * @brief CENTUM VP PID Control Loop — End-to-End Example
 *
 * L6 — Canonical Problem: Temperature control using CENTUM VP PID block.
 * Demonstrates: PID initialization, tuning, execution loop, bumpless transfer,
 * anti-windup, alarm handling, and trending output.
 *
 * Scenario: Reactor temperature control
 *   - SV = 80.0 °C (setpoint)
 *   - Initial PV = 25.0 °C (room temperature)
 *   - Kp=3.0, Ti=120.0s, Td=10.0s (Ziegler-Nichols based)
 *   - Output limits: 0-100% steam valve
 *   - Run for 100 iterations at 200ms each (20 seconds simulated)
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "centum_vp_control.h"

int main(void)
{
    printf("========================================\n");
    printf(" CENTUM VP — PID Temperature Control\n");
    printf("========================================\n\n");

    /* Initialize PID block with CENTUM VP defaults */
    centum_pid_block_t pid;
    centum_pid_block_init(&pid);

    /* Configure for temperature control */
    centum_pid_block_set_tuning(&pid, 3.0, 120.0, 10.0, 0.0);
    pid.action = PID_ACT_REVERSE;  /* MV increases when PV < SV (heating) */
    pid.mv_high_limit = 100.0;
    pid.mv_low_limit = 0.0;
    pid.mv_change_limit = 20.0;    /* Max 20% per second */
    pid.sv_high_limit = 120.0;
    pid.sv_low_limit = 0.0;
    pid.scan_period_ms = 200;

    /* Set up alarms */
    pid.vh_high_alarm = 95.0;   /* High temperature alarm */
    pid.vl_low_alarm = 10.0;    /* Low temperature alarm */
    pid.dv_high_alarm = 15.0;   /* Deviation alarm: >15°C from SP */

    /* Set setpoint */
    centum_pid_block_set_sv(&pid, 80.0);

    /* Initially in MAN mode at 0% output */
    pid.mv = 0.0;

    /* Simulate process response (simple first-order with delay) */
    double pv = 25.0;         /* Current temperature (process variable) */
    double ambient = 25.0;    /* Ambient temperature */
    double process_gain = 3.5; /* °C per % valve opening */
    double time_constant = 50.0; /* seconds */
    double dt = 0.2;          /* 200ms scan interval */

    printf("Time(s)  Mode     SV(C)   PV(C)   MV(%%)   Alarms\n");
    printf("-------  -------  ------  ------  ------  ------\n");

    /* Phase 1: Manual warm-up */
    pid.mode = PID_MODE_MAN;
    pid.mv = 40.0;  /* Open steam valve to 40% */
    for (int i = 0; i < 30; i++) {
        /* Simulate process */
        double dpv = (process_gain * pid.mv - (pv - ambient)) / time_constant * dt;
        pv += dpv;

        centum_pid_block_execute(&pid, pv, dt);
        centum_pid_block_handle_alarms(&pid);

        if (i % 10 == 0) {
            printf("%7.1f  %-7s  %6.1f  %6.1f  %6.1f  %s\n",
                   i * dt, "MAN", pid.sv, pv, pid.mv,
                   pid.vh_hi_alarm_active ? "VH" : "OK");
        }
    }

    /* Phase 2: Switch to AUT — bumpless transfer */
    printf("\n*** Switching to AUT (Bumpless Transfer) ***\n\n");
    centum_pid_block_bumpless_transfer(&pid, pid.mv);
    centum_pid_block_set_mode(&pid, PID_MODE_AUT);

    /* Phase 3: Automatic control */
    for (int i = 0; i < 100; i++) {
        /* Simulate process */
        double dpv = (process_gain * pid.mv - (pv - ambient)) / time_constant * dt;
        pv += dpv;

        double mv = centum_pid_block_execute(&pid, pv, dt);
        centum_pid_block_handle_alarms(&pid);

        if (i % 10 == 0) {
            printf("%7.1f  %-7s  %6.1f  %6.1f  %6.1f  %s%s%s\n",
                   (i + 30) * dt, centum_pid_mode_to_string(pid.mode),
                   pid.sv, pv, mv,
                   pid.vh_hi_alarm_active ? "VH " : "",
                   pid.dv_hi_alarm_active ? "DV " : "",
                   pid.anti_windup_active ? "AW" : "OK");
        }
    }

    /* Final state */
    printf("\n========================================\n");
    printf(" Final State:\n");
    printf("   SV: %.1f C (target)\n", pid.sv);
    printf("   PV: %.1f C (actual)\n", pid.pv);
    printf("   MV: %.1f %% (valve position)\n", pid.mv);
    printf("   Error: %.1f C\n", fabs(pid.sv - pid.pv));
    printf("   Integral: %.2f\n", pid.integral);
    printf("   Anti-Windup: %s\n", pid.anti_windup_active ? "Active" : "Inactive");
    printf("   Bumpless: %s\n", pid.bumpless_active ? "Enabled" : "Disabled");
    printf("========================================\n");

    return 0;
}