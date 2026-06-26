/**
 * @file example_batch_reactor_control.c
 * @brief L6 Canonical Problem: ISA-88 batch control for a reactor process.
 *
 * This example demonstrates ISA-88 procedural control:
 *   1. Recipe: "Polymer Batch Grade A"
 *   2. Operations: CHARGE → HEAT → REACT → COOL → TRANSFER
 *   3. Each operation has defined phases with parameters
 *
 * The batch state machine and phase logic from dcs_isa88_batch.c
 * are used to execute the recipe and track progress.
 */

#include "dcs_types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Bring in the ISA-88 batch types and functions defined in dcs_isa88_batch.c */
/* (These types are local to the .c file; we reproduce the key ones here) */

typedef enum {
    BATCH_IDLE, BATCH_RUNNING, BATCH_COMPLETE,
    BATCH_HELD, BATCH_STOPPED, BATCH_ABORTED
} batch_state_e;

typedef enum {
    PHASE_IDLE, PHASE_RUNNING, PHASE_COMPLETE,
    PHASE_HELD, PHASE_STOPPED, PHASE_ABORTED
} phase_state_e;

typedef struct {
    int         phase_id;
    char        phase_name[32];
    char        phase_type[32];
    phase_state_e state;
    double      setpoint_value;
    double      duration_min;
    double      elapsed_min;
    double      completion_pct;
} batch_phase_t;

typedef struct {
    int            operation_id;
    char           operation_name[32];
    int            num_phases;
    batch_phase_t *phases;
    int            current_phase;
    int            is_complete;
} batch_operation_t;

typedef struct {
    int               recipe_id;
    char              recipe_name[48];
    char              product_code[16];
    int               num_operations;
    batch_operation_t *operations;
    int               target_unit_id;
    double            batch_size;
    batch_state_e     batch_state;
    double            total_duration_min;
    double            elapsed_min;
} batch_recipe_t;

/* Phase state machine */
static void execute_phase(batch_phase_t *phase, double dt_min)
{
    switch (phase->state) {
        case PHASE_IDLE:
            phase->state = PHASE_RUNNING;
            phase->elapsed_min = 0.0;
            phase->completion_pct = 0.0;
            break;
        case PHASE_RUNNING:
            phase->elapsed_min += dt_min;
            if (phase->duration_min > 0.0) {
                phase->completion_pct = phase->elapsed_min / phase->duration_min * 100.0;
                if (phase->completion_pct >= 100.0) {
                    phase->completion_pct = 100.0;
                    phase->state = PHASE_COMPLETE;
                }
            }
            break;
        case PHASE_COMPLETE:
        case PHASE_STOPPED:
        case PHASE_ABORTED:
            break;
        case PHASE_HELD:
            break;
    }
}

static const char *state_name(batch_state_e s)
{
    switch (s) {
        case BATCH_IDLE:    return "IDLE";
        case BATCH_RUNNING: return "RUNNING";
        case BATCH_COMPLETE: return "COMPLETE";
        case BATCH_HELD:    return "HELD";
        case BATCH_STOPPED: return "STOPPED";
        case BATCH_ABORTED: return "ABORTED";
        default:            return "UNKNOWN";
    }
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  ISA-88 Batch Control — Polymer Reactor R-101       ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Step 1: Create the recipe */
    batch_recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    recipe.recipe_id = 1001;
    snprintf(recipe.recipe_name, 48, "Polymer Batch Grade A");
    snprintf(recipe.product_code, 16, "POLY-A-001");
    recipe.target_unit_id = 101;
    recipe.batch_size = 5000.0;  /* 5000 kg */
    recipe.batch_state = BATCH_IDLE;

    /* Define operations and phases */
    recipe.num_operations = 5;
    recipe.operations = (batch_operation_t *)calloc(5, sizeof(batch_operation_t));

    /* Operation 1: CHARGE */
    {
        batch_operation_t *op = &recipe.operations[0];
        op->operation_id = 1;
        snprintf(op->operation_name, 32, "CHARGE");
        op->num_phases = 3;
        op->phases = (batch_phase_t *)calloc(3, sizeof(batch_phase_t));

        batch_phase_t *p = op->phases;
        p[0].phase_id = 1; snprintf(p[0].phase_name, 32, "Add Solvent");
        snprintf(p[0].phase_type, 32, "CHARGE"); p[0].setpoint_value = 2000.0;
        p[0].duration_min = 15.0;

        p[1].phase_id = 2; snprintf(p[1].phase_name, 32, "Add Monomer");
        snprintf(p[1].phase_type, 32, "CHARGE"); p[1].setpoint_value = 2500.0;
        p[1].duration_min = 20.0;

        p[2].phase_id = 3; snprintf(p[2].phase_name, 32, "Add Catalyst");
        snprintf(p[2].phase_type, 32, "CHARGE"); p[2].setpoint_value = 50.0;
        p[2].duration_min = 10.0;
    }

    /* Operation 2: HEAT */
    {
        batch_operation_t *op = &recipe.operations[1];
        op->operation_id = 2;
        snprintf(op->operation_name, 32, "HEAT");
        op->num_phases = 2;
        op->phases = (batch_phase_t *)calloc(2, sizeof(batch_phase_t));

        batch_phase_t *p = op->phases;
        p[0].phase_id = 4; snprintf(p[0].phase_name, 32, "Heat to 80C");
        snprintf(p[0].phase_type, 32, "HEAT"); p[0].setpoint_value = 80.0;
        p[0].duration_min = 25.0;

        p[1].phase_id = 5; snprintf(p[1].phase_name, 32, "Temperature Soak");
        snprintf(p[1].phase_type, 32, "HOLD"); p[1].setpoint_value = 80.0;
        p[1].duration_min = 10.0;
    }

    /* Operation 3: REACT */
    {
        batch_operation_t *op = &recipe.operations[2];
        op->operation_id = 3;
        snprintf(op->operation_name, 32, "REACT");
        op->num_phases = 2;
        op->phases = (batch_phase_t *)calloc(2, sizeof(batch_phase_t));

        batch_phase_t *p = op->phases;
        p[0].phase_id = 6; snprintf(p[0].phase_name, 32, "Polymerization");
        snprintf(p[0].phase_type, 32, "REACT"); p[0].setpoint_value = 85.0;
        p[0].duration_min = 120.0;

        p[1].phase_id = 7; snprintf(p[1].phase_name, 32, "Conversion Check");
        snprintf(p[1].phase_type, 32, "CHECK"); p[1].setpoint_value = 95.0;
        p[1].duration_min = 5.0;
    }

    /* Operation 4: COOL */
    {
        batch_operation_t *op = &recipe.operations[3];
        op->operation_id = 4;
        snprintf(op->operation_name, 32, "COOL");
        op->num_phases = 1;
        op->phases = (batch_phase_t *)calloc(1, sizeof(batch_phase_t));

        batch_phase_t *p = op->phases;
        p[0].phase_id = 8; snprintf(p[0].phase_name, 32, "Cool to 40C");
        snprintf(p[0].phase_type, 32, "COOL"); p[0].setpoint_value = 40.0;
        p[0].duration_min = 30.0;
    }

    /* Operation 5: TRANSFER */
    {
        batch_operation_t *op = &recipe.operations[4];
        op->operation_id = 5;
        snprintf(op->operation_name, 32, "TRANSFER");
        op->num_phases = 2;
        op->phases = (batch_phase_t *)calloc(2, sizeof(batch_phase_t));

        batch_phase_t *p = op->phases;
        p[0].phase_id = 9; snprintf(p[0].phase_name, 32, "Transfer to Storage");
        snprintf(p[0].phase_type, 32, "XFER"); p[0].setpoint_value = 5000.0;
        p[0].duration_min = 20.0;

        p[1].phase_id = 10; snprintf(p[1].phase_name, 32, "Line Flush");
        snprintf(p[1].phase_type, 32, "FLUSH"); p[1].setpoint_value = 500.0;
        p[1].duration_min = 10.0;
    }

    /* Calculate total duration */
    for (int i = 0; i < recipe.num_operations; i++) {
        for (int j = 0; j < recipe.operations[i].num_phases; j++) {
            recipe.total_duration_min += recipe.operations[i].phases[j].duration_min;
        }
    }

    printf("Recipe: %s (Product: %s)\n", recipe.recipe_name, recipe.product_code);
    printf("Batch Size: %.0f kg, Unit: R-101\n", recipe.batch_size);
    printf("Total Operations: %d, Total Duration: %.0f min (%.1f hrs)\n\n",
           recipe.num_operations, recipe.total_duration_min,
           recipe.total_duration_min / 60.0);

    /* Print recipe structure */
    printf("Procedural Control Hierarchy:\n");
    for (int i = 0; i < recipe.num_operations; i++) {
        batch_operation_t *op = &recipe.operations[i];
        printf("  Operation %d: %s (%d phases)\n",
               op->operation_id, op->operation_name, op->num_phases);
        for (int j = 0; j < op->num_phases; j++) {
            batch_phase_t *p = &op->phases[j];
            printf("    Phase %d.%d: %-24s  SP=%.0f  %5.0f min\n",
                   op->operation_id, p->phase_id,
                   p->phase_name, p->setpoint_value, p->duration_min);
        }
    }

    /* Step 2: Execute the batch (simulate with time steps) */
    printf("\n=== Batch Execution Simulation ===\n\n");

    /* Start the batch */
    recipe.batch_state = BATCH_RUNNING;
    printf("[T=0] BATCH STARTED\n\n");

    double dt = 5.0;  /* 5 minute time steps */
    double total_time = 0.0;
    int batch_running = 1;

    while (batch_running) {
        total_time += dt;
        recipe.elapsed_min = total_time;

        /* Find current operation */
        int active_op_idx = -1;
        for (int i = 0; i < recipe.num_operations; i++) {
            if (!recipe.operations[i].is_complete) {
                active_op_idx = i;
                break;
            }
        }

        if (active_op_idx < 0) {
            recipe.batch_state = BATCH_COMPLETE;
            batch_running = 0;
            break;
        }

        batch_operation_t *op = &recipe.operations[active_op_idx];
        batch_phase_t *phase = &op->phases[op->current_phase];

        execute_phase(phase, dt);

        /* Check phase completion */
        if (phase->state == PHASE_COMPLETE) {
            printf("[T=%.0f min] Phase %s COMPLETE (%.0f%% of batch)\n",
                   total_time, phase->phase_name,
                   recipe.elapsed_min / recipe.total_duration_min * 100.0);

            op->current_phase++;
            if (op->current_phase >= op->num_phases) {
                op->is_complete = 1;
                printf("[T=%.0f min] Operation %s COMPLETE\n",
                       total_time, op->operation_name);
            }
        }
    }

    /* Step 3: Batch completion */
    printf("\n[BATCH COMPLETE]\n");
    printf("  State:             %s\n", state_name(recipe.batch_state));
    printf("  Total Time:        %.0f min (%.1f hrs)\n",
           recipe.elapsed_min, recipe.elapsed_min / 60.0);
    printf("  Expected Duration: %.0f min\n", recipe.total_duration_min);
    printf("  Efficiency:        %.1f%%\n\n",
           recipe.total_duration_min / recipe.elapsed_min * 100.0);

    /* Step 4: Cleanup */
    for (int i = 0; i < recipe.num_operations; i++) {
        free(recipe.operations[i].phases);
    }
    free(recipe.operations);

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Batch Complete — Ready for Next Batch              ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
