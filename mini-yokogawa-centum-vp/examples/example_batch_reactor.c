/**
 * @file example_batch_reactor.c
 * @brief CENTUM VP Batch — ISA-88 Reactor Batch Process
 *
 * L6 — Canonical Problem: Batch reactor control using CENTUM VP Batch Package.
 * Demonstrates: Recipe definition (ISA-88 procedure model), formula scaling,
 * batch execution with phases (CHARGE, HEAT, REACT, COOL, TRANSFER), and
 * electronic batch record generation.
 *
 * Scenario: Chemical reactor producing Product-X
 *   - Recipe: REACTOR-MASTER-001
 *   - Phases: CHARGE_A, CHARGE_B, HEAT, REACT, COOL, TRANSFER
 *   - Scale from 500L to 750L batch size
 */

#include <stdio.h>
#include <string.h>
#include "centum_vp_batch.h"

static const char *phase_names[] = {
    "CHARGE_A",     /* Charge Reactant A, 200 kg */
    "CHARGE_B",     /* Charge Reactant B, 50 kg */
    "HEAT_TO_80C",  /* Heat to 80C over 30 minutes */
    "REACT_60MIN",  /* Maintain at 80C for 60 minutes */
    "COOL_TO_30C",  /* Cool to 30C over 20 minutes */
    "TRANSFER_TO_STORAGE"  /* Pump product to storage tank */
};

int main(void)
{
    printf("========================================\n");
    printf(" CENTUM VP — Batch Reactor Process\n");
    printf("========================================\n\n");

    centum_batch_manager_t mgr;
    centum_batch_manager_init(&mgr);

    /* Step 1: Define phases */
    printf("Step 1: Defining ISA-88 Phases\n");
    for (int i = 0; i < 6; i++) {
        centum_batch_phase_t phase;
        memset(&phase, 0, sizeof(phase));
        strncpy(phase.phase_name, phase_names[i], sizeof(phase.phase_name) - 1);
        snprintf(phase.phase_tag, sizeof(phase.phase_tag), "PH%03d", i + 1);
        phase.phase_type = PHASE_TYPE_BASIC;
        phase.state = BATCH_STATE_IDLE;
        phase.max_duration_sec = (i == 0) ? 600u :  /* CHARGE_A: 10 min */
                                 (i == 1) ? 300u :  /* CHARGE_B: 5 min */
                                 (i == 2) ? 1800u : /* HEAT: 30 min */
                                 (i == 3) ? 3600u : /* REACT: 60 min */
                                 (i == 4) ? 1200u : /* COOL: 20 min */
                                 600u;               /* TRANSFER: 10 min */

        if (i >= 2 && i <= 4) { /* Temperature phases */
            centum_batch_parameter_t param;
            memset(&param, 0, sizeof(param));
            strncpy(param.name, "TEMPERATURE", sizeof(param.name) - 1);
            param.type = PARAM_TYPE_FLOAT;
            param.value.f_val = (i == 2) ? 80.0 : (i == 4) ? 30.0 : 80.0;
            param.reportable = true;
            param.key_parameter = true;
            centum_batch_add_parameter(&phase, &param);
        }
        centum_batch_add_phase(&mgr, &phase);
        printf("  Phase %d: %-20s Duration: %us\n",
               i + 1, phase.phase_name, phase.max_duration_sec);
    }
    printf("  Total phases defined: %u\n", mgr.phase_count);

    /* Step 2: Define operations (grouping phases) */
    printf("\nStep 2: Defining Operations\n");
    centum_batch_operation_t op;
    memset(&op, 0, sizeof(op));
    strcpy(op.operation_name, "CHARGE_MATERIALS");
    op.phase_count = 2;
    op.phase_indices[0] = 0; /* CHARGE_A */
    op.phase_indices[1] = 1; /* CHARGE_B */
    op.sequential = true;
    centum_batch_add_operation(&mgr, &op);
    printf("  Operation 1: CHARGE_MATERIALS (2 phases)\n");

    memset(&op, 0, sizeof(op));
    strcpy(op.operation_name, "REACTION");
    op.phase_count = 2;
    op.phase_indices[0] = 2; /* HEAT_TO_80C */
    op.phase_indices[1] = 3; /* REACT_60MIN */
    op.sequential = true;
    centum_batch_add_operation(&mgr, &op);
    printf("  Operation 2: REACTION (2 phases)\n");

    memset(&op, 0, sizeof(op));
    strcpy(op.operation_name, "FINISHING");
    op.phase_count = 2;
    op.phase_indices[0] = 4; /* COOL_TO_30C */
    op.phase_indices[1] = 5; /* TRANSFER */
    op.sequential = true;
    centum_batch_add_operation(&mgr, &op);
    printf("  Operation 3: FINISHING (2 phases)\n");

    /* Step 3: Define unit procedure */
    printf("\nStep 3: Defining Unit Procedure\n");
    centum_batch_unit_procedure_t up;
    memset(&up, 0, sizeof(up));
    strcpy(up.unit_proc_name, "REACTOR_PROCEDURE");
    strcpy(up.assigned_unit_tag, "R-101");
    up.operation_count = 3;
    up.operation_indices[0] = 0;
    up.operation_indices[1] = 1;
    up.operation_indices[2] = 2;
    centum_batch_add_unit_procedure(&mgr, &up);
    printf("  Unit Procedure: REACTOR_PROCEDURE on R-101\n");

    /* Step 4: Define procedure */
    printf("\nStep 4: Defining Procedure\n");
    centum_batch_procedure_t proc;
    memset(&proc, 0, sizeof(proc));
    strcpy(proc.procedure_name, "MAIN_PROCEDURE");
    proc.level = ISA88_PROCEDURE;
    proc.unit_proc_count = 1;
    proc.unit_proc_indices[0] = 0;
    proc.max_cycle_time_sec = 14400; /* 4 hours */
    centum_batch_add_procedure(&mgr, &proc);
    printf("  Procedure: MAIN_PROCEDURE\n");

    /* Step 5: Define master recipe with formula */
    printf("\nStep 5: Creating Master Recipe\n");
    centum_batch_recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    strcpy(recipe.recipe_name, "Product-X Reactor Batch");
    strcpy(recipe.recipe_id, "PX-REACTOR-001");
    strcpy(recipe.product_name, "Product-X");
    strcpy(recipe.product_grade, "USP");
    strcpy(recipe.author, "Process Engineering");
    strcpy(recipe.description, "Standard reactor batch for Product-X");
    recipe.recipe_version = 2;
    recipe.created_date = time(NULL);
    recipe.procedure_count = 1;
    recipe.procedure_indices[0] = 0;
    recipe.target_batch_size = 500.0;
    recipe.min_batch_size = 300.0;
    recipe.max_batch_size = 1000.0;
    strcpy(recipe.batch_size_units, "L");
    recipe.released = true;

    /* Add formula items */
    centum_formula_item_t fi;
    memset(&fi, 0, sizeof(fi));
    strcpy(fi.formula_id, "RM-001");
    strcpy(fi.material_name, "Reactant A");
    fi.target_quantity = 200.0;
    fi.min_quantity = 190.0;
    fi.max_quantity = 210.0;
    strcpy(fi.quantity_units, "kg");
    centum_batch_add_formula_item(&recipe, &fi);

    memset(&fi, 0, sizeof(fi));
    strcpy(fi.formula_id, "RM-002");
    strcpy(fi.material_name, "Reactant B");
    fi.target_quantity = 50.0;
    fi.min_quantity = 47.5;
    fi.max_quantity = 52.5;
    strcpy(fi.quantity_units, "kg");
    centum_batch_add_formula_item(&recipe, &fi);

    centum_batch_add_recipe(&mgr, &recipe);
    printf("  Recipe: %s (v%u) for %s\n",
           recipe.recipe_id, recipe.recipe_version, recipe.product_name);
    printf("  Target Batch Size: %.0f L\n", recipe.target_batch_size);
    printf("  Formula Items: %u\n", recipe.formula_item_count);
    printf("  Valid: %s\n", centum_batch_validate_recipe(&recipe) ? "YES" : "NO");

    /* Step 6: Scale recipe for larger batch */
    printf("\nStep 6: Scale Recipe from 500L to 750L\n");
    bool scaled = centum_batch_scale_recipe(&recipe, 750.0);
    printf("  Scaling: %s\n", scaled ? "SUCCESS" : "FAILED");
    printf("  New Batch Size: %.0f L\n", recipe.target_batch_size);
    printf("  Reactant A: %.0f kg (was 200 kg)\n", recipe.formula[0].target_quantity);
    printf("  Reactant B: %.0f kg (was 50 kg)\n", recipe.formula[1].target_quantity);

    /* Step 7: Start batch execution */
    printf("\nStep 7: Start Batch Execution\n");
    bool started = centum_batch_start(&mgr, "BATCH-2026-001", "PX-REACTOR-001");
    printf("  Batch Start: %s\n", started ? "SUCCESS" : "FAILED");
    printf("  Initial State: %s\n",
           centum_batch_state_to_string(centum_batch_get_state(&mgr)));

    /* Step 8: Simulate batch execution */
    printf("\nStep 8: Simulating Batch Execution (10 cycles)\n");
    for (int cycle = 0; cycle < 10; cycle++) {
        centum_batch_execute(&mgr, 30.0); /* 30-second steps */
        printf("  Cycle %2d: State=%-12s Progress=%.1f%%\n",
               cycle + 1,
               centum_batch_state_to_string(centum_batch_get_state(&mgr)),
               centum_batch_progress(&mgr));

        if (centum_batch_is_complete(&mgr)) {
            printf("  *** BATCH COMPLETE ***\n");
            break;
        }
    }

    /* Step 9: Generate electronic batch record */
    printf("\nStep 9: Electronic Batch Record (EBR)\n");
    char ebr[2048];
    centum_batch_generate_report(&mgr, ebr, sizeof(ebr));
    printf("%s", ebr);

    /* Step 10: Check equipment availability */
    printf("\nStep 10: Equipment Availability Check\n");
    bool avail = centum_batch_check_equipment_availability(&mgr, "R-101");
    printf("  Reactor R-101 available: %s\n", avail ? "YES" : "NO (busy)");

    printf("\n========================================\n");
    printf(" Batch Process Demonstration Complete\n");
    printf("========================================\n");

    return 0;
}