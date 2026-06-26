#include "delta_v_batch.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void delta_v_recipe_init(delta_v_recipe_t *recipe, const char *name, double master_size)
{
    if (!recipe) return;
    memset(recipe, 0, sizeof(delta_v_recipe_t));
    if (name) strncpy(recipe->recipe_name, name, sizeof(recipe->recipe_name)-1);
    recipe->master_batch_size = master_size;
    recipe->batch_size_liters = master_size;
    recipe->scalable = true;
    strncpy(recipe->recipe_version, "1.0", sizeof(recipe->recipe_version)-1);
    snprintf(recipe->product_name, sizeof(recipe->product_name), "%s_Product", name ? name : "Batch");
}

bool delta_v_recipe_add_unit_procedure(delta_v_recipe_t *recipe, const delta_v_unit_procedure_t *up)
{
    if (!recipe || !up || recipe->unit_procedure_count >= 4) return false;
    recipe->unit_procedures[recipe->unit_procedure_count] = *up;
    recipe->unit_procedure_count++;
    return true;
}

bool delta_v_recipe_add_operation(delta_v_recipe_t *recipe, uint8_t up_idx, const delta_v_operation_t *op)
{
    if (!recipe || !op || up_idx >= recipe->unit_procedure_count) return false;
    if (recipe->unit_procedures[up_idx].operation_count >= 8) return false;
    recipe->unit_procedures[up_idx].operations[recipe->unit_procedures[up_idx].operation_count] = *op;
    recipe->unit_procedures[up_idx].operation_count++;
    return true;
}

bool delta_v_recipe_add_phase(delta_v_recipe_t *recipe, uint8_t up_idx, uint8_t op_idx, const delta_v_phase_t *phase)
{
    if (!recipe || !phase || up_idx >= recipe->unit_procedure_count) return false;
    delta_v_unit_procedure_t *up = &recipe->unit_procedures[up_idx];
    if (op_idx >= up->operation_count) return false;
    delta_v_operation_t *op = &up->operations[op_idx];
    if (op->phase_count >= 16) return false;
    op->phases[op->phase_count] = *phase;
    op->phase_count++;
    return true;
}

bool delta_v_recipe_validate(const delta_v_recipe_t *recipe)
{
    if (!recipe) return false;
    if (recipe->recipe_name[0] == '\0') return false;
    if (recipe->master_batch_size <= 0.0) return false;
    if (recipe->unit_procedure_count == 0) return false;
    for (uint8_t u = 0; u < recipe->unit_procedure_count; u++) {
        if (recipe->unit_procedures[u].operation_count == 0) return false;
        for (uint8_t o = 0; o < recipe->unit_procedures[u].operation_count; o++) {
            if (recipe->unit_procedures[u].operations[o].phase_count == 0) return false;
        }
    }
    return true;
}

void delta_v_formula_init(delta_v_formula_t *formula)
{
    if (!formula) return;
    memset(formula, 0, sizeof(delta_v_formula_t));
}

void delta_v_formula_scale(delta_v_formula_t *formula, double scale_factor)
{
    if (!formula || scale_factor <= 0.0) return;
    formula->raw_material_a_kg *= scale_factor;
    formula->raw_material_b_kg *= scale_factor;
    formula->catalyst_kg *= scale_factor;
    formula->solvent_liters *= scale_factor;
}

void delta_v_batch_execution_init(delta_v_batch_execution_t *exec, const delta_v_recipe_t *recipe, const char *batch_id)
{
    if (!exec || !recipe) return;
    memset(exec, 0, sizeof(delta_v_batch_execution_t));
    exec->recipe = *recipe;
    if (batch_id) strncpy(exec->batch_id, batch_id, sizeof(exec->batch_id)-1);
    exec->state = DELTAV_BATCH_IDLE;
    exec->batch_scale_factor = 1.0;
    exec->expected_yield_kg = recipe->master_batch_size * 0.95;
}

bool delta_v_batch_is_transition_valid(delta_v_batch_state_t current, delta_v_batch_command_t cmd)
{
    switch (cmd) {
    case DELTAV_BATCH_CMD_START:
        return (current == DELTAV_BATCH_IDLE || current == DELTAV_BATCH_COMPLETE);
    case DELTAV_BATCH_CMD_HOLD:
        return (current == DELTAV_BATCH_RUNNING || current == DELTAV_BATCH_RESTARTING);
    case DELTAV_BATCH_CMD_RESTART:
        return (current == DELTAV_BATCH_HELD || current == DELTAV_BATCH_PAUSED);
    case DELTAV_BATCH_CMD_STOP:
        return (current == DELTAV_BATCH_RUNNING || current == DELTAV_BATCH_HELD || current == DELTAV_BATCH_PAUSED);
    case DELTAV_BATCH_CMD_ABORT:
        return (current != DELTAV_BATCH_IDLE && current != DELTAV_BATCH_ABORTED);
    case DELTAV_BATCH_CMD_PAUSE:
        return (current == DELTAV_BATCH_RUNNING);
    case DELTAV_BATCH_CMD_RESUME:
        return (current == DELTAV_BATCH_PAUSED);
    case DELTAV_BATCH_CMD_RESET:
        return (current == DELTAV_BATCH_COMPLETE || current == DELTAV_BATCH_STOPPED || current == DELTAV_BATCH_ABORTED);
    default: return false;
    }
}

bool delta_v_batch_issue_command(delta_v_batch_execution_t *exec, delta_v_batch_command_t cmd)
{
    if (!exec) return false;
    if (!delta_v_batch_is_transition_valid(exec->state, cmd)) return false;
    switch (cmd) {
    case DELTAV_BATCH_CMD_START:
        exec->state = DELTAV_BATCH_RUNNING; exec->active = true; exec->start_time = 0; break;
    case DELTAV_BATCH_CMD_HOLD:
        exec->state = (exec->state == DELTAV_BATCH_RUNNING) ? DELTAV_BATCH_HOLDING : DELTAV_BATCH_HELD; break;
    case DELTAV_BATCH_CMD_RESTART: exec->state = DELTAV_BATCH_RESTARTING; break;
    case DELTAV_BATCH_CMD_STOP: exec->state = DELTAV_BATCH_STOPPING; break;
    case DELTAV_BATCH_CMD_ABORT: exec->state = DELTAV_BATCH_ABORTING; break;
    case DELTAV_BATCH_CMD_PAUSE: exec->state = DELTAV_BATCH_PAUSING; break;
    case DELTAV_BATCH_CMD_RESUME: exec->state = DELTAV_BATCH_RUNNING; break;
    case DELTAV_BATCH_CMD_RESET: exec->state = DELTAV_BATCH_IDLE; exec->active = false; break;
    default: return false;
    }
    return true;
}

bool delta_v_batch_advance_phase(delta_v_batch_execution_t *exec)
{
    if (!exec || !exec->active || exec->state != DELTAV_BATCH_RUNNING) return false;
    exec->current_phase_index++;
    return true;
}

double delta_v_batch_calc_yield(const delta_v_batch_execution_t *exec)
{
    if (!exec || exec->expected_yield_kg <= 0.0) return 0.0;
    return (exec->actual_yield_kg / exec->expected_yield_kg) * 100.0;
}

void delta_v_batch_generate_ebr(delta_v_batch_execution_t *exec)
{
    if (!exec) return;
    int written = snprintf(exec->electronic_batch_record, sizeof(exec->electronic_batch_record),
        "=== Electronic Batch Record ===\n"
        "Batch ID: %s\nProduct: %s\nRecipe: %s\n"
        "Start: %ld\nEnd: %ld\nElapsed: %u sec\n"
        "Scale Factor: %.2f\nActual Yield: %.2f kg\nExpected Yield: %.2f kg\n"
        "Yield %%: %.1f\nState: %s\nAlarms: %u\nEvents: %u\n"
        "=== End of Record ===\n",
        exec->batch_id, exec->recipe.product_name, exec->recipe.recipe_name,
        (long)exec->start_time, (long)exec->end_time, exec->elapsed_seconds,
        exec->batch_scale_factor, exec->actual_yield_kg, exec->expected_yield_kg,
        delta_v_batch_calc_yield(exec), delta_v_batch_state_to_string(exec->state),
        exec->alarm_count, exec->event_count);
    exec->ebr_length = (written > 0 && written < (int)sizeof(exec->electronic_batch_record)) ? (uint16_t)written : 0;
}

double delta_v_batch_equipment_utilization(const delta_v_batch_execution_t *exec)
{
    if (!exec || exec->start_time == 0) return 0.0;
    double total_time = (double)exec->elapsed_seconds;
    double run_time = total_time;
    if (run_time <= 0.0) return 0.0;
    double utilization = run_time / (run_time + 3600.0);
    return fmin(100.0, utilization * 100.0);
}

bool delta_v_batch_check_phase_complete(const delta_v_batch_execution_t *exec)
{
    if (!exec) return false;
    return (exec->elapsed_seconds >= 3600);
}

const char *delta_v_batch_state_to_string(delta_v_batch_state_t state) {
    static const char *s[] = {"Idle","Running","Complete","Pausing","Paused","Holding","Held","Restarting","Stopping","Stopped","Aborting","Aborted"};
    return (state <= DELTAV_BATCH_ABORTED) ? s[state] : "Unknown";
}

const char *delta_v_batch_command_to_string(delta_v_batch_command_t cmd) {
    static const char *s[] = {"Start","Hold","Restart","Stop","Abort","Pause","Resume","Reset"};
    return (cmd <= DELTAV_BATCH_CMD_RESET) ? s[cmd] : "Unknown";
}

const char *isa88_entity_level_to_string(isa88_entity_level_t level) {
    static const char *s[] = {"Procedure","UnitProcedure","Operation","Phase"};
    return (level <= ISA88_PHASE) ? s[level] : "Unknown";
}

bool delta_v_batch_validate_formula(const delta_v_formula_t *formula)
{
    if (!formula) return false;
    if (formula->raw_material_a_kg < 0.0 || formula->raw_material_b_kg < 0.0) return false;
    if (formula->temperature_setpoint_c <= 0.0) return false;
    return true;
}

double delta_v_batch_estimate_cycle_time(const delta_v_recipe_t *recipe)
{
    if (!recipe) return 0.0;
    double total_sec = 0.0;
    for (uint8_t u = 0; u < recipe->unit_procedure_count; u++) {
        for (uint8_t o = 0; o < recipe->unit_procedures[u].operation_count; o++) {
            for (uint8_t p = 0; p < recipe->unit_procedures[u].operations[o].phase_count; p++) {
                total_sec += recipe->unit_procedures[u].operations[o].phases[p].phase_duration_sec;
            }
        }
    }
    return total_sec;
}

bool delta_v_batch_allocate_unit(delta_v_batch_execution_t *exec, uint16_t unit_id)
{
    if (!exec) return false;
    if (exec->active && exec->state == DELTAV_BATCH_RUNNING) return false;
    exec->recipe.unit_procedures[0].unit_id = unit_id;
    return true;
}
