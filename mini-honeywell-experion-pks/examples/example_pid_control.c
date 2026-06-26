/**
 * @file example_pid_control.c
 * @brief Example: PID temperature control on C300 controller
 * L6: Temperature control canonical problem
 * Course: MIT 2.171, Stanford ENGR205
 */
#include "../include/experion_system.h"
#include "../include/c300_controller.h"
#include "../include/control_blocks.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== Example: PID Temperature Control on C300 ===\n\n");
    /* Setup */
    ExperionSystem sys;
    experion_system_init(&sys, "REACTOR_UNIT", 100);
    experion_system_register_node(&sys, 1, EXN_NODE_ESVT);
    experion_system_register_node(&sys, 2, EXN_NODE_C300);
    experion_system_activate(&sys);
    printf("System: %s activated\n", sys.system_name);
    /* C300 Controller */
    C300Controller ctrl;
    c300_init(&ctrl, 2, "C300_REACTOR", 250);
    ctrl.online = true;
    c300_configure_io_slot(&ctrl, 0, C3IO_AI_8CH_TC);
    c300_configure_channel(&ctrl, 0, 0, "TIC001.PV", 0.0, 300.0, "degC");
    c300_configure_io_slot(&ctrl, 1, C3IO_AO_8CH_420MA);
    c300_configure_channel(&ctrl, 1, 0, "TIC001.OP", 0.0, 100.0, "%");
    printf("C300 %s ready\n\n", ctrl.controller_name);
    /* PID Block */
    PIDControlBlock pid;
    pid_block_init(&pid, 1, "TIC001");
    pid_set_tuning(&pid, 2.5, 120.0, 30.0);
    pid_set_limits(&pid, 0.0, 300.0, 0.0, 100.0);
    printf("PID TIC001: Kc=%.1f Ti=%.0fs Td=%.0fs\n",
           pid.params.kc, pid.params.ti_sec, pid.params.td_sec);
    /* Startup in Manual, then switch to Auto */
    pid_set_mode(&pid, PID_MANUAL);
    pid.state.op = 20.0;
    pid_bumpless_transfer(&pid, pid.state.op);
    pid_set_mode(&pid, PID_AUTO);
    pid.state.sp = 180.0;
    printf("SP=%.1f degC, Starting control...\n", pid.state.sp);
    printf("Time(s)   PV       SP       OP       Error\n");
    printf("--------  -------  -------  -------  -------\n");
    double temperature = 25.0;
    for (int i = 0; i < 40; i++) {
        double op;
        pid_execute(&pid, temperature, 1.0, &op);
        if (i % 5 == 0) {
            printf("%8d  %7.1f  %7.1f  %7.1f  %7.1f\n",
                   i, temperature, pid.state.sp, op, pid.state.error);
        }
        /* Simulate plant: first-order process, gain=3, tau=50s */
        double dt = 1.0;
        double alpha = 1.0 - exp(-dt / 50.0);
        temperature += alpha * (3.0 * op + 25.0 - temperature);
        /* Disturbance at t=20s */
        if (i == 20) {
            temperature -= 15.0;
            printf("  *** Disturbance at t=20: -15 degC ***\n");
        }
    }
    printf("\nFinal: PV=%.1f degC, SP=%.1f degC, error=%.1f degC\n",
           temperature, pid.state.sp, fabs(temperature - pid.state.sp));
    printf("=== Example Complete ===\n");
    return 0;
}
