#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../include/delta_v_batch.h"

int main(void)
{
    printf("=== DeltaV Batch Reactor ISA-88 Simulation ===\n\n");

    delta_v_recipe_t recipe;
    delta_v_recipe_init(&recipe, "Polymerization", 500.0);

    delta_v_unit_procedure_t up;
    memset(&up, 0, sizeof(up));
    strcpy(up.unit_name, "Reactor_R101");
    up.unit_id = 101;

    delta_v_operation_t op;
    memset(&op, 0, sizeof(op));
    strcpy(op.operation_name, "Charge_and_React");

    delta_v_phase_t phases[6];
    const char *pnames[] = {"CHARGE_A","CHARGE_B","HEAT","REACT","COOL","TRANSFER"};
    double pdur[] = {300, 200, 600, 1800, 400, 300};
    for (int i = 0; i < 6; i++) {
        memset(&phases[i], 0, sizeof(delta_v_phase_t));
        strcpy(phases[i].phase_name, pnames[i]);
        phases[i].phase_duration_sec = pdur[i];
    }

    for (int i = 0; i < 6; i++)
        delta_v_recipe_add_phase(&recipe, 0, 0, &phases[i]);
    delta_v_recipe_add_operation(&recipe, 0, &op);
    delta_v_recipe_add_unit_procedure(&recipe, &up);

    printf("Recipe: %s (Master batch: %.0f L)\n", recipe.recipe_name, recipe.master_batch_size);
    double cycle_time = delta_v_batch_estimate_cycle_time(&recipe);
    printf("Estimated cycle time: %.0f seconds (%.1f hours)\n\n", cycle_time, cycle_time / 3600.0);

    delta_v_batch_execution_t exec;
    delta_v_batch_execution_init(&exec, &recipe, "BATCH-2026-0001");
    exec.batch_scale_factor = 1.5;

    printf("Batch ID: %s\n", exec.batch_id);
    printf("Scale: %.1f x (%.0f L)\n\n", exec.batch_scale_factor, recipe.master_batch_size * exec.batch_scale_factor);

    printf("Issuing START command...\n");
    delta_v_batch_issue_command(&exec, DELTAV_BATCH_CMD_START);
    printf("State: %s\n", delta_v_batch_state_to_string(exec.state));

    for (int p = 0; p < 6; p++) {
        printf("  Phase %d/%d: %s (%us)\n", p+1, 6, pnames[p], (unsigned)pdur[p]);
        exec.elapsed_seconds += pdur[p];
        delta_v_batch_advance_phase(&exec);
    }

    exec.actual_yield_kg = 710.0;
    printf("\nReaction complete. Actual yield: %.1f kg\n", exec.actual_yield_kg);
    printf("Yield: %.1f%%\n", delta_v_batch_calc_yield(&exec));

    delta_v_batch_issue_command(&exec, DELTAV_BATCH_CMD_STOP);
    printf("Final state: %s\n", delta_v_batch_state_to_string(exec.state));

    delta_v_batch_generate_ebr(&exec);
    printf("\n%s", exec.electronic_batch_record);

    printf("=== Batch Simulation Complete ===\n");
    return 0;
}
