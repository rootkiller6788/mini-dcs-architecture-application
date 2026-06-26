#include <stdio.h>
#include <string.h>
#include "../include/delta_v_control.h"

int main(void)
{
    printf("=== DeltaV PID Control Loop Simulation ===\n");
    printf("Reactor Temperature Control: 25C -> 80C setpoint\n\n");

    delta_v_pid_block_t pid;
    delta_v_pid_block_init(&pid, DELTAV_PID_STANDARD);
    delta_v_pid_block_set_gains(&pid, 2.5, 120.0, 10.0);
    pid.mode = DELTAV_PID_MAN;
    pid.pv = 25.0; pid.sp = 80.0; pid.out = 0.0;
    pid.out_low_limit = 0.0; pid.out_high_limit = 100.0;

    printf("Phase 1: Manual warmup (0-5 sec)\n");
    for (int i = 0; i < 5; i++) {
        pid.out = 50.0;
        pid.pv += (pid.out * 1.2 - pid.pv) * 0.1;
        printf("  t=%d.0s  PV=%.1fC  OUT=%.1f%%\n", i, pid.pv, pid.out);
    }

    printf("\nPhase 2: Bumpless transfer to AUTO\n");
    pid.pv = 32.0;
    pid.sp = 80.0;
    delta_v_pid_block_bumpless_transfer(&pid);
    delta_v_pid_block_set_mode(&pid, DELTAV_PID_AUT);

    printf("Phase 3: PID Control (5-50 sec)\n");
    for (int i = 5; i <= 50; i += 5) {
        for (int j = 0; j < 5; j++)
            delta_v_pid_block_calculate(&pid, 1.0);
        pid.pv += (pid.out * 1.2 - pid.pv) * 0.1;
        printf("  t=%2ds  PV=%.1fC  SP=%.0fC  OUT=%.1f%%  err=%.1f\n",
               i, pid.pv, pid.sp, pid.out, pid.sp - pid.pv);
    }

    printf("\nFinal: PV=%.1fC  SP=%.0fC  Error=%.1fC\n",
           pid.pv, pid.sp, pid.sp - pid.pv);
    printf("=== Simulation Complete ===\n");
    return 0;
}
