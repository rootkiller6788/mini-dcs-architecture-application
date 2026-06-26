/**
 * @file dcs_isa88_batch.c
 * @brief ISA-88 batch control integration within DCS architecture.
 *
 * Knowledge Levels: L4 Engineering Standards (ISA-88), L6 Canonical Problems
 *
 * References:
 *   - ANSI/ISA-88.00.01-2010: Batch Control Part 1 — Models and Terminology
 *   - ISA-88.00.02-2001: Batch Control Part 2 — Data Structures and Guidelines
 *   - ISA-88.00.03-2003: Batch Control Part 3 — General and Site Recipe Models
 *   - Honeywell Experion Batch Manager
 *   - Yokogawa CENTUM VP Batch Package
 *
 * Covers recipe management, procedural control hierarchy, phase logic,
 * unit allocation, and batch execution state machines.
 */

#include "dcs_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * L1: ISA-88 Batch Control Definitions
 *===========================================================================*/

/**
 * @brief ISA-88 physical model hierarchy.
 */
typedef enum {
    ISA88_ENTERPRISE   = 0,
    ISA88_SITE         = 1,
    ISA88_AREA         = 2,
    ISA88_PROCESS_CELL = 3,
    ISA88_UNIT         = 4,
    ISA88_EQUIPMENT_MODULE = 5,
    ISA88_CONTROL_MODULE   = 6
} isa88_physical_level_t;

/**
 * @brief ISA-88 recipe types.
 *
 * General Recipe:     Enterprise-level, equipment-independent.
 * Site Recipe:         Site-specific, may reference local materials.
 * Master Recipe:       Process-cell-specific, targeting equipment types.
 * Control Recipe:      Executable recipe, bound to specific equipment.
 */
typedef enum {
    ISA88_RECIPE_GENERAL  = 0,
    ISA88_RECIPE_SITE     = 1,
    ISA88_RECIPE_MASTER   = 2,
    ISA88_RECIPE_CONTROL  = 3
} isa88_recipe_type_t;

/**
 * @brief ISA-88 batch state machine (per ISA-88 Part 1, Figure 9).
 *
 * The procedural control model executes within these states.
 */
typedef enum {
    ISA88_BATCH_IDLE        = 0,
    ISA88_BATCH_RUNNING     = 1,
    ISA88_BATCH_COMPLETE    = 2,
    ISA88_BATCH_HELD        = 3,
    ISA88_BATCH_STOPPED     = 4,
    ISA88_BATCH_ABORTED     = 5
} isa88_batch_state_t;

/**
 * @brief Recipe phase definition.
 *
 * A phase is the smallest element of procedural control.
 * Examples: CHARGE, HEAT, REACT, COOL, TRANSFER, AGITATE.
 */
typedef struct {
    uint32_t            phase_id;
    char                phase_name[32];
    char                phase_type[32];    /* e.g., "CHARGE", "HEAT", "AGITATE" */
    isa88_phase_state_t state;
    double              setpoint_value;    /* Phase parameter */
    double              setpoint_2;        /* Secondary parameter */
    double              duration_min;      /* Expected duration */
    double              elapsed_min;       /* Time in this phase */
    double              completion_pct;    /* 0-100 */
    int                 requires_operator; /* Needs operator confirmation */
    int                 failure_strategy;  /* 0=Hold, 1=Abort, 2=Continue */
} isa88_phase_t;

/**
 * @brief Recipe operation definition.
 *
 * An operation is a sequence of phases that performs a major
 * processing activity.
 */
typedef struct {
    uint32_t    operation_id;
    char        operation_name[32];
    uint32_t    num_phases;
    isa88_phase_t *phases;          /* Ordered array of phases */
    uint32_t    current_phase_idx;   /* Currently executing phase */
    int         is_complete;
} isa88_operation_t;

/**
 * @brief Control recipe definition.
 *
 * A control recipe is the executable specification that defines
 * the manufacture of a batch of product. It contains:
 *   - Header: product identification, version, author
 *   - Formula: process inputs, parameters, outputs
 *   - Equipment requirements: unit types, quantities
 *   - Procedure: operations → phases → steps
 */
typedef struct {
    uint32_t            recipe_id;
    char                recipe_name[48];
    char                product_code[16];
    isa88_recipe_type_t type;
    char                version[12];
    uint32_t            num_operations;
    isa88_operation_t  *operations;
    uint32_t            target_unit_id;    /* Assigned process unit */
    double              target_batch_size; /* kg, liters, etc. */
    char                batch_size_units[8];
    isa88_batch_state_t batch_state;
    uint64_t            start_timestamp;
    double              total_duration_min;
} isa88_control_recipe_t;

/*===========================================================================
 * L4: Recipe Management Functions
 *===========================================================================*/

/**
 * @brief Initialize a control recipe from a master recipe.
 *
 * A master recipe is equipment-independent; the control recipe
 * binds it to a specific unit with specific batch size.
 *
 * @param recipe           Output: initialized control recipe.
 * @param name             Recipe name.
 * @param product_code     Product identifier.
 * @param target_unit_id   Physical unit assigned for execution.
 * @param batch_size       Batch size in recipe units.
 * @return                 1 on success.
 */
int isa88_recipe_init(isa88_control_recipe_t *recipe,
                       const char *name,
                       const char *product_code,
                       uint32_t target_unit_id,
                       double batch_size)
{
    if (recipe == NULL || name == NULL) return 0;

    memset(recipe, 0, sizeof(isa88_control_recipe_t));

    recipe->recipe_id = target_unit_id; /* Simplified: ID = unit ID */
    snprintf(recipe->recipe_name, 48, "%s", name);
    snprintf(recipe->product_code, 16, "%s",
              product_code != NULL ? product_code : "DEFAULT");
    recipe->type = ISA88_RECIPE_CONTROL;
    snprintf(recipe->version, 12, "1.0");
    recipe->target_unit_id = target_unit_id;
    recipe->target_batch_size = batch_size;
    snprintf(recipe->batch_size_units, 8, "kg");
    recipe->batch_state = ISA88_BATCH_IDLE;

    return 1;
}

/**
 * @brief Add an operation to a recipe.
 *
 * @param recipe      Control recipe.
 * @param op_name     Operation name (e.g., "CHARGE", "REACT").
 * @param num_phases  Number of phases in this operation.
 * @param phases      Array of phase definitions.
 * @return            1 on success, 0 on failure.
 */
int isa88_recipe_add_operation(isa88_control_recipe_t *recipe,
                                const char *op_name,
                                uint32_t num_phases,
                                const isa88_phase_t *phases)
{
    if (recipe == NULL || op_name == NULL || phases == NULL) return 0;
    if (num_phases == 0) return 0;

    /* Reallocate operations array */
    uint32_t new_count = recipe->num_operations + 1;
    isa88_operation_t *new_ops = (isa88_operation_t *)realloc(
        recipe->operations, new_count * sizeof(isa88_operation_t));
    if (new_ops == NULL) return 0;

    recipe->operations = new_ops;
    isa88_operation_t *op = &recipe->operations[recipe->num_operations];

    memset(op, 0, sizeof(isa88_operation_t));
    op->operation_id = recipe->num_operations + 1;
    snprintf(op->operation_name, 32, "%s", op_name);
    op->num_phases = num_phases;
    op->current_phase_idx = 0;
    op->is_complete = 0;

    /* Allocate phases */
    op->phases = (isa88_phase_t *)malloc(num_phases * sizeof(isa88_phase_t));
    if (op->phases == NULL) {
        free(new_ops);
        return 0;
    }

    memcpy(op->phases, phases, num_phases * sizeof(isa88_phase_t));

    recipe->num_operations = new_count;

    return 1;
}

/*===========================================================================
 * L4: Batch Execution State Machine
 *===========================================================================*/

/**
 * @brief Execute one step of batch control state machine.
 *
 * The ISA-88 batch state machine transitions:
 *
 * IDLE → RUNNING:      Operator starts the batch.
 * RUNNING → HELD:       Operator holds (pauses) the batch.
 * HELD → RUNNING:      Operator restarts.
 * RUNNING → STOPPED:   Operator stops (controlled shutdown).
 * STOPPED → RUNNING:   Operator restarts (may skip completed phases).
 * RUNNING → COMPLETE:  All operations complete normally.
 * Any state → ABORTED: Emergency/failure abort.
 *
 * @param recipe        Control recipe being executed.
 * @param command       Command: 0=Start, 1=Hold, 2=Stop, 3=Abort, 4=Restart.
 * @return              1 if transition valid, 0 if invalid.
 */
int isa88_batch_command(isa88_control_recipe_t *recipe, int command)
{
    if (recipe == NULL) return 0;

    /*
     * State transition table.
     * Rows: current state, Columns: command
     *
     * Command: 0=Start 1=Hold 2=Stop 3=Abort 4=Restart
     */
    static const int transition_table[6][5] = {
        /* IDLE    */ { 1, 0, 0, 1, 0 },
        /* RUNNING */ { 0, 1, 1, 1, 0 },
        /* COMPLETE*/ { 0, 0, 0, 1, 0 },
        /* HELD    */ { 0, 0, 1, 1, 1 },
        /* STOPPED */ { 0, 0, 0, 1, 1 },
        /* ABORTED */ { 0, 0, 0, 0, 0 }
    };

    int current = (int)recipe->batch_state;
    if (current < 0 || current > 5) current = 0;
    if (command < 0 || command > 4) return 0;

    if (!transition_table[current][command]) {
        return 0; /* Invalid transition */
    }

    /* Execute transition */
    switch (command) {
        case 0: /* Start */
            recipe->batch_state = ISA88_BATCH_RUNNING;
            break;
        case 1: /* Hold */
            recipe->batch_state = ISA88_BATCH_HELD;
            break;
        case 2: /* Stop */
            recipe->batch_state = ISA88_BATCH_STOPPED;
            break;
        case 3: /* Abort */
            recipe->batch_state = ISA88_BATCH_ABORTED;
            break;
        case 4: /* Restart */
            recipe->batch_state = ISA88_BATCH_RUNNING;
            break;
        default:
            return 0;
    }

    return 1;
}

/**
 * @brief Execute a phase within the current operation.
 *
 * Advances the phase state machine:
 *   IDLE → RUNNING (start phase)
 *   RUNNING → COMPLETE (phase finishes)
 *   RUNNING → HELD (operator holds)
 *   HELD → RUNNING (restart after hold)
 *
 * @param phase    Phase to execute.
 * @param dt_min   Time step in minutes.
 * @return         1 if phase state changed, 0 if unchanged.
 */
int isa88_phase_execute(isa88_phase_t *phase, double dt_min)
{
    if (phase == NULL) return 0;

    isa88_phase_state_t prev = phase->state;

    switch (phase->state) {
        case ISA88_PHASE_IDLE:
            /* Auto-start for execution */
            phase->state = ISA88_PHASE_RUNNING;
            phase->elapsed_min = 0.0;
            phase->completion_pct = 0.0;
            break;

        case ISA88_PHASE_RUNNING:
            phase->elapsed_min += dt_min;
            if (phase->duration_min > 0.0) {
                phase->completion_pct = (phase->elapsed_min / phase->duration_min)
                                       * 100.0;
                if (phase->completion_pct >= 100.0) {
                    phase->completion_pct = 100.0;
                    phase->state = ISA88_PHASE_COMPLETE;
                }
            }
            break;

        case ISA88_PHASE_COMPLETE:
        case ISA88_PHASE_STOPPED:
        case ISA88_PHASE_ABORTED:
            /* Terminal states — no further execution */
            break;

        case ISA88_PHASE_HELD:
            /* Held: wait for operator to restart */
            break;

        case ISA88_PHASE_RESTARTING:
            phase->state = ISA88_PHASE_RUNNING;
            break;

        default:
            break;
    }

    return (phase->state != prev) ? 1 : 0;
}

/**
 * @brief Execute the current operation of a batch recipe.
 *
 * Iterates through phases sequentially. When a phase completes,
 * advances to the next phase. When all phases complete, the
 * operation is marked complete.
 *
 * @param recipe   Control recipe.
 * @param dt_min   Time step in minutes.
 * @return         Current phase name (static string, do not free).
 */
const char *isa88_batch_execute_step(isa88_control_recipe_t *recipe,
                                      double dt_min)
{
    if (recipe == NULL) return "INVALID";

    if (recipe->batch_state != ISA88_BATCH_RUNNING) {
        return "BATCH_NOT_RUNNING";
    }

    if (recipe->num_operations == 0) {
        recipe->batch_state = ISA88_BATCH_COMPLETE;
        return "NO_OPERATIONS";
    }

    /* Find current incomplete operation */
    for (uint32_t i = 0; i < recipe->num_operations; i++) {
        isa88_operation_t *op = &recipe->operations[i];
        if (op->is_complete) continue;

        /* Execute current phase */
        isa88_phase_t *phase = &op->phases[op->current_phase_idx];
        isa88_phase_execute(phase, dt_min);

        if (phase->state == ISA88_PHASE_COMPLETE) {
            /* Advance to next phase */
            op->current_phase_idx++;

            if (op->current_phase_idx >= op->num_phases) {
                /* Operation complete */
                op->is_complete = 1;
            }
        }

        return phase->phase_name;
    }

    /* All operations complete */
    recipe->batch_state = ISA88_BATCH_COMPLETE;
    return "BATCH_COMPLETE";
}

/*===========================================================================
 * L6: Recipe Execution Duration Prediction
 *===========================================================================*/

/**
 * @brief Calculate total predicted batch duration.
 *
 * Sums the duration of all phases in all operations.
 * Used for production scheduling and unit allocation.
 *
 * @param recipe    Control recipe.
 * @return          Total batch duration in minutes.
 */
double isa88_calculate_batch_duration(const isa88_control_recipe_t *recipe)
{
    if (recipe == NULL) return 0.0;

    double total = 0.0;

    for (uint32_t i = 0; i < recipe->num_operations; i++) {
        const isa88_operation_t *op = &recipe->operations[i];
        for (uint32_t j = 0; j < op->num_phases; j++) {
            total += op->phases[j].duration_min;
        }
    }

    return total;
}

/**
 * @brief Calculate batch execution progress percentage.
 *
 * Progress = elapsed time of completed phases + current phase progress
 *            divided by total predicted duration.
 *
 * @param recipe    Control recipe.
 * @return          Progress percentage (0-100).
 */
double isa88_calculate_batch_progress(const isa88_control_recipe_t *recipe)
{
    if (recipe == NULL) return 0.0;

    double total = isa88_calculate_batch_duration(recipe);
    if (total <= 0.0) return 0.0;

    double completed = 0.0;

    for (uint32_t i = 0; i < recipe->num_operations; i++) {
        const isa88_operation_t *op = &recipe->operations[i];
        for (uint32_t j = 0; j < op->num_phases; j++) {
            const isa88_phase_t *ph = &op->phases[j];
            if (ph->state == ISA88_PHASE_COMPLETE) {
                completed += ph->duration_min;
            } else if (ph->state == ISA88_PHASE_RUNNING) {
                completed += ph->elapsed_min;
            }
        }
    }

    return (completed / total) * 100.0;
}

/*===========================================================================
 * L6: Unit Allocation
 *===========================================================================*/

/**
 * @brief Check if a unit is available for batch allocation.
 *
 * In ISA-88, a unit can only execute one batch at a time.
 * Unit states:
 *   - IDLE: Available for allocation
 *   - BUSY: Currently executing a batch
 *   - HELD: Batch held, unit reserved
 *   - CLEANING: Between batches
 *   - MAINTENANCE: Not available
 */
typedef enum {
    ISA88_UNIT_IDLE        = 0,
    ISA88_UNIT_BUSY        = 1,
    ISA88_UNIT_HELD        = 2,
    ISA88_UNIT_CLEANING    = 3,
    ISA88_UNIT_MAINTENANCE = 4
} isa88_unit_state_t;

/**
 * @brief Unit resource for batch allocation.
 */
typedef struct {
    uint32_t           unit_id;
    char               unit_name[32];
    isa88_unit_state_t state;
    uint32_t           current_recipe_id;
    double             max_capacity;
    char               capacity_units[8];
} isa88_unit_t;

/**
 * @brief Allocate a unit for batch execution.
 *
 * Selects the first available unit of the required type
 * that has sufficient capacity for the batch.
 *
 * @param units              Array of available units.
 * @param num_units          Number of units.
 * @param required_capacity  Minimum capacity required.
 * @param assigned_unit_id   Output: assigned unit ID, 0 if none available.
 * @return                   1 if a unit was assigned, 0 if none available.
 */
int isa88_allocate_unit(const isa88_unit_t *units,
                         uint32_t num_units,
                         double required_capacity,
                         uint32_t *assigned_unit_id)
{
    if (units == NULL || assigned_unit_id == NULL) return 0;

    *assigned_unit_id = 0;

    for (uint32_t i = 0; i < num_units; i++) {
        if (units[i].state == ISA88_UNIT_IDLE
            && units[i].max_capacity >= required_capacity) {
            *assigned_unit_id = units[i].unit_id;
            return 1;
        }
    }

    return 0; /* No unit available */
}

/**
 * @brief Free recipe resources (memory cleanup).
 *
 * @param recipe  Control recipe to free.
 */
void isa88_recipe_free(isa88_control_recipe_t *recipe)
{
    if (recipe == NULL) return;

    for (uint32_t i = 0; i < recipe->num_operations; i++) {
        if (recipe->operations[i].phases != NULL) {
            free(recipe->operations[i].phases);
        }
    }

    if (recipe->operations != NULL) {
        free(recipe->operations);
        recipe->operations = NULL;
    }

    recipe->num_operations = 0;
}
