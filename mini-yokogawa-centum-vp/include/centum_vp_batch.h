#ifndef CENTUM_VP_BATCH_H
#define CENTUM_VP_BATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define CENTUM_BATCH_MAX_PROCEDURES    16
#define CENTUM_BATCH_MAX_UNIT_PROC     16
#define CENTUM_BATCH_MAX_OPERATIONS    32
#define CENTUM_BATCH_MAX_PHASES        64
#define CENTUM_BATCH_MAX_PARAMETERS    32
#define CENTUM_BATCH_MAX_MESSAGES      32
#define CENTUM_BATCH_MAX_RECIPES       16
#define CENTUM_BATCH_MAX_FORMULA_ITEMS 16

typedef enum {
    BATCH_STATE_IDLE        = 0,
    BATCH_STATE_RUNNING     = 1,
    BATCH_STATE_HOLDING     = 2,
    BATCH_STATE_HELD        = 3,
    BATCH_STATE_RESTARTING  = 4,
    BATCH_STATE_STOPPING    = 5,
    BATCH_STATE_STOPPED     = 6,
    BATCH_STATE_ABORTING    = 7,
    BATCH_STATE_ABORTED     = 8,
    BATCH_STATE_COMPLETING  = 9,
    BATCH_STATE_COMPLETE    = 10
} centum_batch_state_t;

typedef enum {
    BATCH_CMD_START      = 0,
    BATCH_CMD_HOLD       = 1,
    BATCH_CMD_RESTART    = 2,
    BATCH_CMD_STOP       = 3,
    BATCH_CMD_ABORT      = 4,
    BATCH_CMD_RESET      = 5,
    BATCH_CMD_PAUSE      = 6,
    BATCH_CMD_RESUME     = 7
} centum_batch_command_t;

typedef enum {
    ISA88_PROCEDURE  = 0,
    ISA88_UNIT_PROC  = 1,
    ISA88_OPERATION  = 2,
    ISA88_PHASE      = 3
} isa88_entity_level_t;

typedef enum {
    ISA88_UNIT           = 0,
    ISA88_EQUIPMENT_MOD  = 1,
    ISA88_CONTROL_MOD    = 2
} isa88_equipment_level_t;

typedef enum {
    PHASE_TYPE_BASIC      = 0,
    PHASE_TYPE_EQUIPMENT  = 1,
    PHASE_TYPE_MANUAL     = 2
} centum_phase_type_t;

typedef enum {
    PARAM_TYPE_INT     = 0,
    PARAM_TYPE_FLOAT   = 1,
    PARAM_TYPE_BOOL    = 2,
    PARAM_TYPE_STRING  = 3,
    PARAM_TYPE_ENUM    = 4
} centum_parameter_type_t;

typedef struct {
    char            name[32];
    centum_parameter_type_t type;
    union {
        int32_t     i_val;
        double      f_val;
        bool        b_val;
        char        s_val[64];
        int32_t     e_val;
    } value;
    double          eu_low;
    double          eu_high;
    char            eu_units[8];
    bool            reportable;
    bool            key_parameter;
} centum_batch_parameter_t;

typedef struct {
    char        formula_id[16];
    char        material_name[32];
    double      target_quantity;
    double      min_quantity;
    double      max_quantity;
    double      actual_quantity;
    char        quantity_units[8];
    double      concentration;
    bool        dispensed;
    time_t      dispense_time;
} centum_formula_item_t;

typedef struct {
    char            phase_name[32];
    char            phase_tag[16];
    centum_phase_type_t phase_type;
    centum_batch_state_t state;
    uint32_t        max_duration_sec;
    uint32_t        elapsed_sec;
    centum_batch_parameter_t parameters[CENTUM_BATCH_MAX_PARAMETERS];
    uint16_t        parameter_count;
    bool            requires_acknowledgment;
    bool            acknowledged;
    char            equipment_module_tag[16];
    uint16_t        fail_count;
    char            fail_message[128];
} centum_batch_phase_t;

typedef struct {
    char            operation_name[32];
    uint16_t        phase_count;
    uint16_t        phase_indices[CENTUM_BATCH_MAX_PHASES];
    centum_batch_state_t state;
    bool            sequential;
} centum_batch_operation_t;

typedef struct {
    char            unit_proc_name[32];
    uint16_t        operation_count;
    uint16_t        operation_indices[CENTUM_BATCH_MAX_OPERATIONS];
    centum_batch_state_t state;
    char            assigned_unit_tag[16];
} centum_batch_unit_procedure_t;

typedef struct {
    char            recipe_name[64];
    char            recipe_id[16];
    char            product_name[32];
    char            product_grade[16];
    uint32_t        recipe_version;
    time_t          created_date;
    time_t          modified_date;
    char            author[32];
    char            description[128];
    uint16_t        procedure_count;
    uint16_t        procedure_indices[CENTUM_BATCH_MAX_PROCEDURES];
    centum_formula_item_t formula[CENTUM_BATCH_MAX_FORMULA_ITEMS];
    uint16_t        formula_item_count;
    bool            approved;
    bool            released;
    bool            is_master_recipe;
    char            parent_recipe_id[16];
    double          target_batch_size;
    char            batch_size_units[8];
    double          min_batch_size;
    double          max_batch_size;
} centum_batch_recipe_t;

typedef struct {
    char            procedure_name[32];
    isa88_entity_level_t level;
    uint16_t        unit_proc_count;
    uint16_t        unit_proc_indices[CENTUM_BATCH_MAX_UNIT_PROC];
    centum_batch_state_t state;
    uint32_t        max_cycle_time_sec;
    bool            auto_restart_on_hold;
    bool            requires_unit_allocation;
    char            required_unit_class[32];
} centum_batch_procedure_t;

typedef struct {
    char            batch_id[32];
    char            recipe_id[16];
    char            batch_name[64];
    centum_batch_state_t state;
    time_t          start_time;
    time_t          end_time;
    time_t          planned_start;
    time_t          planned_end;
    uint16_t        active_procedure_index;
    uint16_t        active_unit_proc_index;
    uint16_t        active_operation_index;
    uint16_t        active_phase_index;
    centum_batch_command_t pending_command;
    double          progress_percent;
    uint32_t        alarm_count;
    uint32_t        event_count;
    char            operator_name[32];
    char            batch_report_path[256];
    bool            electronic_signature;
} centum_batch_execution_t;

typedef struct {
    centum_batch_recipe_t recipes[CENTUM_BATCH_MAX_RECIPES];
    uint16_t    recipe_count;
    centum_batch_procedure_t procedures[CENTUM_BATCH_MAX_PROCEDURES];
    uint16_t    procedure_count;
    centum_batch_unit_procedure_t unit_procs[CENTUM_BATCH_MAX_UNIT_PROC];
    uint16_t    unit_proc_count;
    centum_batch_operation_t operations[CENTUM_BATCH_MAX_OPERATIONS];
    uint16_t    operation_count;
    centum_batch_phase_t phases[CENTUM_BATCH_MAX_PHASES];
    uint16_t    phase_count;
    centum_batch_execution_t active_batch;
    bool        batch_server_active;
} centum_batch_manager_t;

void centum_batch_manager_init(centum_batch_manager_t *mgr);

bool centum_batch_add_recipe(centum_batch_manager_t *mgr, const centum_batch_recipe_t *recipe);
bool centum_batch_find_recipe(const centum_batch_manager_t *mgr, const char *recipe_id,
                               centum_batch_recipe_t *out);

bool centum_batch_add_procedure(centum_batch_manager_t *mgr, const centum_batch_procedure_t *proc);
bool centum_batch_add_unit_procedure(centum_batch_manager_t *mgr, const centum_batch_unit_procedure_t *up);
bool centum_batch_add_operation(centum_batch_manager_t *mgr, const centum_batch_operation_t *op);
bool centum_batch_add_phase(centum_batch_manager_t *mgr, const centum_batch_phase_t *phase);
bool centum_batch_add_formula_item(centum_batch_recipe_t *recipe, const centum_formula_item_t *item);
bool centum_batch_add_parameter(centum_batch_phase_t *phase, const centum_batch_parameter_t *param);

bool centum_batch_start(centum_batch_manager_t *mgr, const char *batch_id,
                         const char *recipe_id);
bool centum_batch_command(centum_batch_manager_t *mgr, centum_batch_command_t cmd);
void centum_batch_execute(centum_batch_manager_t *mgr, double dt_sec);
bool centum_batch_is_complete(const centum_batch_manager_t *mgr);
double centum_batch_progress(const centum_batch_manager_t *mgr);

bool centum_batch_validate_recipe(const centum_batch_recipe_t *recipe);
centum_batch_state_t centum_batch_get_state(const centum_batch_manager_t *mgr);

void centum_batch_generate_report(const centum_batch_manager_t *mgr, char *report, size_t report_size);
bool centum_batch_scale_recipe(centum_batch_recipe_t *recipe, double target_size);
bool centum_batch_check_equipment_availability(const centum_batch_manager_t *mgr,
                                                const char *unit_tag);

const char *centum_batch_state_to_string(centum_batch_state_t state);
const char *centum_batch_command_to_string(centum_batch_command_t cmd);
const char *isa88_level_to_string(isa88_entity_level_t level);

#endif