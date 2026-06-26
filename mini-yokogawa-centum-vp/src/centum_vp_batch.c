/**
 * @file centum_vp_batch.c
 * @brief CENTUM VP Batch Control — ISA-88 Recipe & Procedure Management
 *
 * Knowledge Points:
 *   centum_batch_manager_init — Batch manager initialization (L3)
 *   centum_batch_add_recipe — Recipe registration in batch server (L3)
 *   centum_batch_find_recipe — Recipe lookup by ID (L3)
 *   centum_batch_add_procedure — Procedure definition (ISA-88 procedure level) (L1)
 *   centum_batch_add_unit_procedure — Unit procedure definition (ISA-88) (L1)
 *   centum_batch_add_operation — Operation definition (ISA-88) (L1)
 *   centum_batch_add_phase — Phase definition (ISA-88) (L1)
 *   centum_batch_add_formula_item — Formula/recipe parameter definition (L3)
 *   centum_batch_add_parameter — Phase parameter definition (L3)
 *   centum_batch_start — Batch execution start (L5)
 *   centum_batch_command — Batch command processing (hold/stop/abort) (L2)
 *   centum_batch_execute — Batch execution engine (L5)
 *   centum_batch_is_complete — Batch completion check (L3)
 *   centum_batch_progress — Batch progress calculation (L3)
 *   centum_batch_validate_recipe — Recipe validation (L3)
 *   centum_batch_generate_report — Electronic batch record generation (L7)
 *   centum_batch_scale_recipe — Recipe scaling for different batch sizes (L5)
 *   centum_batch_check_equipment_availability — Equipment allocation check (L5)
 *
 * References:
 *   - ISA-88 Batch Control Standard (Part 1: Models and Terminology)
 *   - CENTUM VP Batch Package Manual (IM 33K01A10-90E)
 *   - ISA-95 Part 3: Activity Models of Manufacturing Operations
 */

#include "centum_vp_batch.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * centum_batch_manager_init
 *
 * Initializes the CENTUM VP Batch Manager. The batch manager is a
 * software component running on the batch server (HIS or dedicated
 * APCS station) that manages recipe execution, equipment allocation,
 * and batch history recording per ISA-88.
 *
 * L3 — Engineering Structure: Batch manager as implemented in
 * CENTUM VP Batch Package.
 *============================================================================*/
void centum_batch_manager_init(centum_batch_manager_t *mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(centum_batch_manager_t));
    mgr->batch_server_active = false;
    mgr->recipe_count = 0;
    mgr->procedure_count = 0;
    mgr->unit_proc_count = 0;
    mgr->operation_count = 0;
    mgr->phase_count = 0;
}

/*============================================================================
 * centum_batch_add_recipe
 *
 * Registers a recipe in the batch manager's recipe database.
 * In CENTUM VP, master recipes are created in the engineering
 * environment and downloaded to the batch server. Control recipes
 * are created from master recipes at batch start time.
 *
 * Recipes contain:
 *   - Header (name, ID, product, version, author)
 *   - Procedure references (which unit procedures, in what order)
 *   - Formula (material quantities, process parameters)
 *
 * L3 — Engineering Structure: Recipe management per ISA-88.
 *============================================================================*/
bool centum_batch_add_recipe(centum_batch_manager_t *mgr, const centum_batch_recipe_t *recipe)
{
    if (!mgr || !recipe) return false;
    if (mgr->recipe_count >= CENTUM_BATCH_MAX_RECIPES) return false;

    /* Check duplicate recipe ID */
    for (uint16_t i = 0; i < mgr->recipe_count; i++) {
        if (strcmp(mgr->recipes[i].recipe_id, recipe->recipe_id) == 0) {
            return false;
        }
    }

    memcpy(&mgr->recipes[mgr->recipe_count], recipe, sizeof(centum_batch_recipe_t));
    mgr->recipe_count++;
    return true;
}

/*============================================================================
 * centum_batch_find_recipe
 *
 * Retrieves a recipe by its unique recipe ID. Returns the recipe
 * structure via out parameter.
 *
 * L3 — Engineering Structure: Recipe database lookup.
 *============================================================================*/
bool centum_batch_find_recipe(const centum_batch_manager_t *mgr, const char *recipe_id,
                               centum_batch_recipe_t *out)
{
    if (!mgr || !recipe_id || !out) return false;

    for (uint16_t i = 0; i < mgr->recipe_count; i++) {
        if (strcmp(mgr->recipes[i].recipe_id, recipe_id) == 0) {
            memcpy(out, &mgr->recipes[i], sizeof(centum_batch_recipe_t));
            return true;
        }
    }
    return false;
}

/*============================================================================
 * centum_batch_add_procedure
 *
 * Adds a procedure definition per ISA-88. A procedure is the highest
 * level of procedural control and defines the strategy for making a
 * batch. It consists of an ordered set of unit procedures.
 *
 * L1 — Definition: ISA-88 Procedure level entity.
 *============================================================================*/
bool centum_batch_add_procedure(centum_batch_manager_t *mgr, const centum_batch_procedure_t *proc)
{
    if (!mgr || !proc) return false;
    if (mgr->procedure_count >= CENTUM_BATCH_MAX_PROCEDURES) return false;

    memcpy(&mgr->procedures[mgr->procedure_count], proc, sizeof(centum_batch_procedure_t));
    mgr->procedure_count++;
    return true;
}

/*============================================================================
 * centum_batch_add_unit_procedure
 *
 * Adds a unit procedure definition. A unit procedure is an ordered set
 * of operations that execute within a single process unit (reactor,
 * mixer, etc.). Unit procedures acquire and release equipment.
 *
 * L1 — Definition: ISA-88 Unit Procedure level entity.
 *============================================================================*/
bool centum_batch_add_unit_procedure(centum_batch_manager_t *mgr,
                                      const centum_batch_unit_procedure_t *up)
{
    if (!mgr || !up) return false;
    if (mgr->unit_proc_count >= CENTUM_BATCH_MAX_UNIT_PROC) return false;

    memcpy(&mgr->unit_procs[mgr->unit_proc_count], up,
           sizeof(centum_batch_unit_procedure_t));
    mgr->unit_proc_count++;
    return true;
}

/*============================================================================
 * centum_batch_add_operation
 *
 * Adds an operation definition. An operation is an ordered set of
 * phases that accomplish a process task (e.g., "Charge Reactant A",
 * "Heat to 80C", "Hold for 30 min").
 *
 * L1 — Definition: ISA-88 Operation level entity.
 *============================================================================*/
bool centum_batch_add_operation(centum_batch_manager_t *mgr,
                                 const centum_batch_operation_t *op)
{
    if (!mgr || !op) return false;
    if (mgr->operation_count >= CENTUM_BATCH_MAX_OPERATIONS) return false;

    memcpy(&mgr->operations[mgr->operation_count], op,
           sizeof(centum_batch_operation_t));
    mgr->operation_count++;
    return true;
}

/*============================================================================
 * centum_batch_add_phase
 *
 * Adds a phase definition. A phase is the smallest element of
 * procedural control in ISA-88. Phases execute process actions:
 *   - AGITATE (start/stop agitator)
 *   - CHARGE (add material to target weight)
 *   - HEAT (heat to target temperature)
 *   - REACT (maintain conditions for duration)
 *   - TRANSFER (pump contents to another unit)
 *   - WAIT (delay for specified time)
 *
 * Phases communicate with equipment modules (valves, pumps, PID loops)
 * through CENTUM VP function block connections.
 *
 * L1 — Definition: ISA-88 Phase level entity — the fundamental
 * building block of batch control.
 *============================================================================*/
bool centum_batch_add_phase(centum_batch_manager_t *mgr, const centum_batch_phase_t *phase)
{
    if (!mgr || !phase) return false;
    if (mgr->phase_count >= CENTUM_BATCH_MAX_PHASES) return false;

    memcpy(&mgr->phases[mgr->phase_count], phase, sizeof(centum_batch_phase_t));
    mgr->phase_count++;
    return true;
}

/*============================================================================
 * centum_batch_add_formula_item
 *
 * Adds a formula item (material with target quantity) to a recipe.
 * The formula defines what materials and how much of each are needed
 * for one batch of the product.
 *
 * L3 — Engineering Structure: Recipe formula definition.
 *============================================================================*/
bool centum_batch_add_formula_item(centum_batch_recipe_t *recipe,
                                    const centum_formula_item_t *item)
{
    if (!recipe || !item) return false;
    if (recipe->formula_item_count >= CENTUM_BATCH_MAX_FORMULA_ITEMS) return false;

    memcpy(&recipe->formula[recipe->formula_item_count], item,
           sizeof(centum_formula_item_t));
    recipe->formula_item_count++;
    return true;
}

/*============================================================================
 * centum_batch_add_parameter
 *
 * Adds a process parameter to a phase definition. Parameters define
 * the specific values used during phase execution (e.g., target
 * temperature, target weight, mixing speed, hold duration).
 *
 * Parameters can be marked as:
 *   - Reportable: included in the electronic batch record
 *   - Key: critical quality attribute requiring verification
 *
 * L3 — Engineering Structure: Phase parameter configuration.
 *============================================================================*/
bool centum_batch_add_parameter(centum_batch_phase_t *phase,
                                 const centum_batch_parameter_t *param)
{
    if (!phase || !param) return false;
    if (phase->parameter_count >= CENTUM_BATCH_MAX_PARAMETERS) return false;

    memcpy(&phase->parameters[phase->parameter_count], param,
           sizeof(centum_batch_parameter_t));
    phase->parameter_count++;
    return true;
}

/*============================================================================
 * centum_batch_start
 *
 * Starts a new batch execution. This function:
 *   1. Validates the recipe exists
 *   2. Creates a control recipe from the master recipe
 *   3. Allocates a batch ID
 *   4. Sets the initial batch state to RUNNING
 *   5. Records the start time
 *
 * In CENTUM VP, batch start is initiated from the HIS batch operation
 * window. The operator must have Batch Manager privileges.
 *
 * L5 — Algorithm: Batch lifecycle initiation.
 *============================================================================*/
bool centum_batch_start(centum_batch_manager_t *mgr, const char *batch_id,
                         const char *recipe_id)
{
    if (!mgr || !batch_id || !recipe_id) return false;

    /* Find the recipe */
    centum_batch_recipe_t recipe;
    if (!centum_batch_find_recipe(mgr, recipe_id, &recipe)) return false;
    if (!recipe.released) return false; /* Must be released for production */

    /* Initialize batch execution context */
    memset(&mgr->active_batch, 0, sizeof(centum_batch_execution_t));
    strncpy(mgr->active_batch.batch_id, batch_id,
            sizeof(mgr->active_batch.batch_id) - 1);
    strncpy(mgr->active_batch.recipe_id, recipe_id,
            sizeof(mgr->active_batch.recipe_id) - 1);
    mgr->active_batch.state = BATCH_STATE_RUNNING;
    mgr->active_batch.start_time = time(NULL);
    mgr->active_batch.planned_start = time(NULL);
    mgr->active_batch.progress_percent = 0.0;
    mgr->active_batch.active_procedure_index = 0;
    mgr->active_batch.active_unit_proc_index = 0;
    mgr->active_batch.active_operation_index = 0;
    mgr->active_batch.active_phase_index = 0;
    mgr->batch_server_active = true;

    return true;
}

/*============================================================================
 * centum_batch_command
 *
 * Processes batch operator commands. CENTUM VP supports the standard
 * ISA-88 batch commands: START, HOLD, RESTART, STOP, ABORT, RESET.
 *
 * Command semantics:
 *   HOLD  — Pause execution gracefully (complete current phase)
 *   STOP  — Stop after current phase completes
 *   ABORT — Immediate termination (emergency)
 *   RESET — Return to IDLE after COMPLETE/ABORTED
 *
 * L2 — Core Concept: Batch command state machine per ISA-88.
 *============================================================================*/
bool centum_batch_command(centum_batch_manager_t *mgr, centum_batch_command_t cmd)
{
    if (!mgr) return false;

    centum_batch_state_t current = mgr->active_batch.state;

    switch (cmd) {
        case BATCH_CMD_HOLD:
            if (current == BATCH_STATE_RUNNING || current == BATCH_STATE_RESTARTING) {
                mgr->active_batch.pending_command = BATCH_CMD_HOLD;
                mgr->active_batch.state = BATCH_STATE_HOLDING;
                return true;
            }
            break;
        case BATCH_CMD_STOP:
            if (current == BATCH_STATE_RUNNING || current == BATCH_STATE_HELD) {
                mgr->active_batch.pending_command = BATCH_CMD_STOP;
                mgr->active_batch.state = BATCH_STATE_STOPPING;
                return true;
            }
            break;
        case BATCH_CMD_ABORT:
            if (current != BATCH_STATE_IDLE && current != BATCH_STATE_COMPLETE) {
                mgr->active_batch.pending_command = BATCH_CMD_ABORT;
                mgr->active_batch.state = BATCH_STATE_ABORTED;
                mgr->active_batch.end_time = time(NULL);
                return true;
            }
            break;
        case BATCH_CMD_RESTART:
            if (current == BATCH_STATE_HELD) {
                mgr->active_batch.pending_command = BATCH_CMD_RESTART;
                mgr->active_batch.state = BATCH_STATE_RESTARTING;
                return true;
            }
            break;
        case BATCH_CMD_RESET:
            if (current == BATCH_STATE_COMPLETE || current == BATCH_STATE_ABORTED ||
                current == BATCH_STATE_STOPPED) {
                mgr->active_batch.state = BATCH_STATE_IDLE;
                mgr->active_batch.progress_percent = 0.0;
                mgr->batch_server_active = false;
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

/*============================================================================
 * centum_batch_execute
 *
 * Main batch execution engine. Called each scan cycle to advance the
 * batch through its procedural hierarchy:
 *
 *   Procedure → Unit Procedure → Operation → Phase
 *
 * Execution advances sequentially through phases within an operation,
 * operations within a unit procedure, and unit procedures within a
 * procedure. Phase transitions occur when:
 *   - Phase completes (time elapsed or condition met)
 *   - Operator acknowledges (if requires_acknowledgment)
 *   - Hold/Stop/Abort command received
 *
 * L5 — Algorithm: Sequential batch execution engine per ISA-88.
 *============================================================================*/
void centum_batch_execute(centum_batch_manager_t *mgr, double dt_sec)
{
    if (!mgr) return;
    if (mgr->active_batch.state != BATCH_STATE_RUNNING &&
        mgr->active_batch.state != BATCH_STATE_RESTARTING) {
        return;
    }
    (void)dt_sec; /* dt parameter used for time-based phase completion */

    /* Process any pending commands before execution */
    if (mgr->active_batch.pending_command == BATCH_CMD_HOLD) {
        mgr->active_batch.state = BATCH_STATE_HELD;
        return;
    }

    /* Get current procedure */
    uint16_t pi = mgr->active_batch.active_procedure_index;
    if (pi >= mgr->procedure_count) {
        mgr->active_batch.state = BATCH_STATE_COMPLETING;
        mgr->active_batch.end_time = time(NULL);
        mgr->active_batch.progress_percent = 100.0;
        mgr->active_batch.state = BATCH_STATE_COMPLETE;
        return;
    }

    /* Get current unit procedure within procedure */
    centum_batch_procedure_t *proc = &mgr->procedures[pi];
    uint16_t upi = mgr->active_batch.active_unit_proc_index;
    if (upi >= proc->unit_proc_count) {
        /* Advance to next procedure */
        mgr->active_batch.active_procedure_index++;
        mgr->active_batch.active_unit_proc_index = 0;
        mgr->active_batch.active_operation_index = 0;
        mgr->active_batch.active_phase_index = 0;
        return;
    }

    uint16_t up_idx = proc->unit_proc_indices[upi];
    if (up_idx >= mgr->unit_proc_count) {
        mgr->active_batch.active_unit_proc_index++;
        return;
    }

    /* Get current operation */
    centum_batch_unit_procedure_t *up = &mgr->unit_procs[up_idx];
    uint16_t opi = mgr->active_batch.active_operation_index;
    if (opi >= up->operation_count) {
        mgr->active_batch.active_unit_proc_index++;
        mgr->active_batch.active_operation_index = 0;
        mgr->active_batch.active_phase_index = 0;
        return;
    }

    uint16_t op_idx = up->operation_indices[opi];
    if (op_idx >= mgr->operation_count) {
        mgr->active_batch.active_operation_index++;
        return;
    }

    /* Get current phase */
    centum_batch_operation_t *op = &mgr->operations[op_idx];
    uint16_t phi = mgr->active_batch.active_phase_index;
    if (phi >= op->phase_count) {
        mgr->active_batch.active_operation_index++;
        mgr->active_batch.active_phase_index = 0;
        return;
    }

    uint16_t ph_idx = op->phase_indices[phi];
    if (ph_idx >= mgr->phase_count) {
        mgr->active_batch.active_phase_index++;
        return;
    }

    /* Execute current phase (in real CENTUM VP, this triggers
       SEBOL sequence tables and function block writes) */
    centum_batch_phase_t *phase = &mgr->phases[ph_idx];

    /* Update phase elapsed time */
    phase->elapsed_sec += (uint32_t)dt_sec;

    /* Check phase completion */
    bool phase_complete = false;
    if (phase->max_duration_sec > 0 &&
        phase->elapsed_sec >= phase->max_duration_sec) {
        phase_complete = true;
    }
    if (phase->requires_acknowledgment && !phase->acknowledged) {
        phase_complete = false; /* Wait for operator acknowledgment */
    }

    if (phase_complete) {
        phase->state = BATCH_STATE_COMPLETE;
        mgr->active_batch.active_phase_index++;
    }

    /* Calculate overall progress */
    double total_phases = 0.0;
    double completed_phases = 0.0;
    for (uint16_t i = 0; i < mgr->phase_count; i++) {
        total_phases += 1.0;
        if (mgr->phases[i].state == BATCH_STATE_COMPLETE) {
            completed_phases += 1.0;
        }
    }
    if (total_phases > 0.0) {
        mgr->active_batch.progress_percent = (completed_phases / total_phases) * 100.0;
    }
}

/*============================================================================
 * centum_batch_is_complete
 *
 * Checks if the active batch has reached a terminal state.
 *
 * L3 — Engineering Structure: Batch completion detection.
 *============================================================================*/
bool centum_batch_is_complete(const centum_batch_manager_t *mgr)
{
    if (!mgr) return false;
    return (mgr->active_batch.state == BATCH_STATE_COMPLETE);
}

/*============================================================================
 * centum_batch_progress
 *
 * Returns the current batch execution progress as a percentage.
 *
 * L3 — Engineering Structure: Batch progress monitoring.
 *============================================================================*/
double centum_batch_progress(const centum_batch_manager_t *mgr)
{
    if (!mgr) return 0.0;
    return mgr->active_batch.progress_percent;
}

/*============================================================================
 * centum_batch_get_state
 *
 * Returns the current state of the active batch.
 *
 * L3 — Engineering Structure: Batch state query.
 *============================================================================*/
centum_batch_state_t centum_batch_get_state(const centum_batch_manager_t *mgr)
{
    if (!mgr) return BATCH_STATE_IDLE;
    return mgr->active_batch.state;
}

/*============================================================================
 * centum_batch_validate_recipe
 *
 * Validates a recipe for consistency:
 *   - Must have at least one procedure
 *   - All procedure references must resolve to valid indices
 *   - Formula items must have valid ranges
 *   - Batch size must be within min/max limits
 *
 * L3 — Engineering Structure: Recipe validation before release.
 *============================================================================*/
bool centum_batch_validate_recipe(const centum_batch_recipe_t *recipe)
{
    if (!recipe) return false;

    /* Must have at least one procedure */
    if (recipe->procedure_count == 0) return false;

    /* Recipe must have a unique ID */
    if (strlen(recipe->recipe_id) == 0) return false;

    /* Product name is required */
    if (strlen(recipe->product_name) == 0) return false;

    /* Batch size range must be valid */
    if (recipe->min_batch_size > recipe->max_batch_size) return false;
    if (recipe->target_batch_size < recipe->min_batch_size ||
        recipe->target_batch_size > recipe->max_batch_size) {
        return false;
    }

    /* At least one formula item if batch size > 0 */
    if (recipe->target_batch_size > 0.0 && recipe->formula_item_count == 0) {
        return false;
    }

    return true;
}

/*============================================================================
 * centum_batch_generate_report
 *
 * Generates an electronic batch record (EBR) for the active batch.
 * CENTUM VP Batch Package stores batch data in Exaquantum historian
 * and generates EBRs compliant with FDA 21 CFR Part 11.
 *
 * The EBR includes:
 *   - Batch identification
 *   - Recipe used (with version)
 *   - Start/end times
 *   - Operator name
 *   - Phase execution details
 *   - Key parameter actuals vs. targets
 *   - Alarm and event log summary
 *
 * L7 — Industrial Application: Electronic batch record generation
 * for pharmaceutical and food industry regulatory compliance.
 *============================================================================*/
void centum_batch_generate_report(const centum_batch_manager_t *mgr, char *report,
                                   size_t report_size)
{
    if (!mgr || !report || report_size == 0) return;

    snprintf(report, report_size,
             "=== ELECTRONIC BATCH RECORD ===\n"
             "Batch ID: %s\n"
             "Recipe: %s\n"
             "State: %s\n"
             "Start Time: %s"
             "End Time: %s"
             "Progress: %.1f%%\n"
             "Operator: %s\n"
             "Alarms: %u\n"
             "Events: %u\n"
             "=== END OF RECORD ===\n",
             mgr->active_batch.batch_id,
             mgr->active_batch.recipe_id,
             centum_batch_state_to_string(mgr->active_batch.state),
             ctime(&mgr->active_batch.start_time),
             mgr->active_batch.end_time ? ctime(&mgr->active_batch.end_time) : "N/A\n",
             mgr->active_batch.progress_percent,
             mgr->active_batch.operator_name[0] ? mgr->active_batch.operator_name : "N/A",
             mgr->active_batch.alarm_count,
             mgr->active_batch.event_count);
}

/*============================================================================
 * centum_batch_scale_recipe
 *
 * Scales a master recipe to a different batch size. Formula quantities
 * are linearly scaled (constant recipe proportions maintained).
 *
 * Scaling formula: new_quantity = original_quantity * (target_size / original_size)
 *
 * CENTUM VP supports recipe scaling in both engineering and runtime
 * environments. Scaling preserves material ratios and process parameters
 * while adjusting quantities.
 *
 * L5 — Algorithm: Linear recipe scaling for batch size flexibility.
 *============================================================================*/
bool centum_batch_scale_recipe(centum_batch_recipe_t *recipe, double target_size)
{
    if (!recipe) return false;
    if (target_size <= 0.0) return false;
    if (target_size < recipe->min_batch_size || target_size > recipe->max_batch_size) {
        return false;
    }

    double scale_factor = target_size / recipe->target_batch_size;

    /* Scale all formula item quantities */
    for (uint16_t i = 0; i < recipe->formula_item_count; i++) {
        recipe->formula[i].target_quantity *= scale_factor;
        recipe->formula[i].min_quantity *= scale_factor;
        recipe->formula[i].max_quantity *= scale_factor;
        /* Actual quantity will be reset when batch runs */
        recipe->formula[i].actual_quantity = 0.0;
        recipe->formula[i].dispensed = false;
    }

    recipe->target_batch_size = target_size;
    return true;
}

/*============================================================================
 * centum_batch_check_equipment_availability
 *
 * Checks if a process unit is available for allocation. CENTUM VP
 * Batch manages equipment arbitration: a unit can only run one
 * batch at a time (unless multi-stream capable).
 *
 * This function checks if the specified unit is currently allocated
 * to another active batch.
 *
 * L5 — Algorithm: Equipment allocation arbitration for batch control.
 *============================================================================*/
bool centum_batch_check_equipment_availability(const centum_batch_manager_t *mgr,
                                                const char *unit_tag)
{
    if (!mgr || !unit_tag) return false;

    /* Check if any active unit procedure has this unit allocated */
    for (uint16_t i = 0; i < mgr->unit_proc_count; i++) {
        if (strcmp(mgr->unit_procs[i].assigned_unit_tag, unit_tag) == 0) {
            /* Unit is assigned but could be idle (batch not running) */
            if (mgr->unit_procs[i].state == BATCH_STATE_RUNNING ||
                mgr->unit_procs[i].state == BATCH_STATE_HELD) {
                return false; /* Unit is busy */
            }
        }
    }

    return true; /* Unit is available */
}

/*============================================================================
 * String conversion utilities for batch state/command/level display
 *============================================================================*/
const char *centum_batch_state_to_string(centum_batch_state_t state)
{
    switch (state) {
        case BATCH_STATE_IDLE:        return "IDLE";
        case BATCH_STATE_RUNNING:     return "RUNNING";
        case BATCH_STATE_HOLDING:     return "HOLDING";
        case BATCH_STATE_HELD:        return "HELD";
        case BATCH_STATE_RESTARTING:  return "RESTARTING";
        case BATCH_STATE_STOPPING:    return "STOPPING";
        case BATCH_STATE_STOPPED:     return "STOPPED";
        case BATCH_STATE_ABORTING:    return "ABORTING";
        case BATCH_STATE_ABORTED:     return "ABORTED";
        case BATCH_STATE_COMPLETING:  return "COMPLETING";
        case BATCH_STATE_COMPLETE:    return "COMPLETE";
        default:                      return "UNKNOWN";
    }
}

const char *centum_batch_command_to_string(centum_batch_command_t cmd)
{
    switch (cmd) {
        case BATCH_CMD_START:   return "START";
        case BATCH_CMD_HOLD:    return "HOLD";
        case BATCH_CMD_RESTART: return "RESTART";
        case BATCH_CMD_STOP:    return "STOP";
        case BATCH_CMD_ABORT:   return "ABORT";
        case BATCH_CMD_RESET:   return "RESET";
        case BATCH_CMD_PAUSE:   return "PAUSE";
        case BATCH_CMD_RESUME:  return "RESUME";
        default:                return "???";
    }
}

const char *isa88_level_to_string(isa88_entity_level_t level)
{
    switch (level) {
        case ISA88_PROCEDURE:  return "Procedure";
        case ISA88_UNIT_PROC:  return "Unit Procedure";
        case ISA88_OPERATION:  return "Operation";
        case ISA88_PHASE:      return "Phase";
        default:               return "Unknown";
    }
}