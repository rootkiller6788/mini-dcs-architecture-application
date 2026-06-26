#ifndef DELTA_V_BATCH_H
#define DELTA_V_BATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    ISA88_PROCEDURE = 0,
    ISA88_UNIT_PROCEDURE = 1,
    ISA88_OPERATION = 2,
    ISA88_PHASE = 3
} isa88_entity_level_t;

typedef enum {
    DELTAV_BATCH_IDLE        = 0,
    DELTAV_BATCH_RUNNING     = 1,
    DELTAV_BATCH_COMPLETE    = 2,
    DELTAV_BATCH_PAUSING     = 3,
    DELTAV_BATCH_PAUSED      = 4,
    DELTAV_BATCH_HOLDING     = 5,
    DELTAV_BATCH_HELD        = 6,
    DELTAV_BATCH_RESTARTING  = 7,
    DELTAV_BATCH_STOPPING    = 8,
    DELTAV_BATCH_STOPPED     = 9,
    DELTAV_BATCH_ABORTING    = 10,
    DELTAV_BATCH_ABORTED     = 11
} delta_v_batch_state_t;

typedef enum {
    DELTAV_BATCH_CMD_START   = 0,
    DELTAV_BATCH_CMD_HOLD    = 1,
    DELTAV_BATCH_CMD_RESTART = 2,
    DELTAV_BATCH_CMD_STOP    = 3,
    DELTAV_BATCH_CMD_ABORT   = 4,
    DELTAV_BATCH_CMD_PAUSE   = 5,
    DELTAV_BATCH_CMD_RESUME  = 6,
    DELTAV_BATCH_CMD_RESET   = 7
} delta_v_batch_command_t;

typedef struct {
    char        phase_name[32];
    char        phase_description[128];
    uint32_t    phase_duration_sec;
    uint16_t    equipment_module_id;
    bool        report_enabled;
    bool        verification_required;
    double      setpoint_value;
    char        formula_parameter[32];
} delta_v_phase_t;

typedef struct {
    char        operation_name[32];
    uint8_t     phase_count;
    delta_v_phase_t phases[16];
    uint8_t     current_phase;
} delta_v_operation_t;

typedef struct {
    char        unit_name[32];
    uint16_t    unit_id;
    uint8_t     operation_count;
    delta_v_operation_t operations[8];
    uint8_t     current_operation;
} delta_v_unit_procedure_t;

typedef struct {
    char        recipe_name[64];
    char        product_name[64];
    char        recipe_version[16];
    uint8_t     unit_procedure_count;
    delta_v_unit_procedure_t unit_procedures[4];
    uint8_t     current_unit_proc;
    double      batch_size_liters;
    double      master_batch_size;
    bool        scalable;
    time_t      created_date;
    time_t      last_modified;
    char        author[32];
} delta_v_recipe_t;

typedef struct {
    double      raw_material_a_kg;
    double      raw_material_b_kg;
    double      catalyst_kg;
    double      solvent_liters;
    double      temperature_setpoint_c;
    double      pressure_setpoint_kpa;
    double      reaction_time_min;
    double      cooling_temp_c;
} delta_v_formula_t;

typedef struct {
    delta_v_recipe_t recipe;
    delta_v_formula_t formula;
    double          batch_scale_factor;
    delta_v_batch_state_t state;
    char            batch_id[64];
    time_t          start_time;
    time_t          end_time;
    uint32_t        elapsed_seconds;
    uint16_t        current_phase_index;
    bool            active;
    bool            manual_intervention;
    char            operator_message[256];
    uint32_t        alarm_count;
    uint32_t        event_count;
    char            electronic_batch_record[4096];
    uint16_t        ebr_length;
    double          actual_yield_kg;
    double          expected_yield_kg;
} delta_v_batch_execution_t;

void delta_v_recipe_init(delta_v_recipe_t *recipe, const char *name, double master_size);
bool delta_v_recipe_add_unit_procedure(delta_v_recipe_t *recipe, const delta_v_unit_procedure_t *up);
bool delta_v_recipe_add_operation(delta_v_recipe_t *recipe, uint8_t up_idx, const delta_v_operation_t *op);
bool delta_v_recipe_add_phase(delta_v_recipe_t *recipe, uint8_t up_idx, uint8_t op_idx, const delta_v_phase_t *phase);
bool delta_v_recipe_validate(const delta_v_recipe_t *recipe);
void delta_v_formula_init(delta_v_formula_t *formula);
void delta_v_formula_scale(delta_v_formula_t *formula, double scale_factor);
void delta_v_batch_execution_init(delta_v_batch_execution_t *exec, const delta_v_recipe_t *recipe, const char *batch_id);
bool delta_v_batch_issue_command(delta_v_batch_execution_t *exec, delta_v_batch_command_t cmd);
bool delta_v_batch_advance_phase(delta_v_batch_execution_t *exec);
double delta_v_batch_calc_yield(const delta_v_batch_execution_t *exec);
bool delta_v_batch_is_transition_valid(delta_v_batch_state_t current, delta_v_batch_command_t cmd);
void delta_v_batch_generate_ebr(delta_v_batch_execution_t *exec);
double delta_v_batch_equipment_utilization(const delta_v_batch_execution_t *exec);
bool delta_v_batch_check_phase_complete(const delta_v_batch_execution_t *exec);
double delta_v_batch_estimate_cycle_time(const delta_v_recipe_t *recipe);
bool delta_v_batch_validate_formula(const delta_v_formula_t *formula);
bool delta_v_batch_allocate_unit(delta_v_batch_execution_t *exec, uint16_t unit_id);
const char *delta_v_batch_state_to_string(delta_v_batch_state_t state);
const char *delta_v_batch_command_to_string(delta_v_batch_command_t cmd);
const char *isa88_entity_level_to_string(isa88_entity_level_t level);

#endif
