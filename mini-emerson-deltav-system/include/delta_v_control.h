#ifndef DELTA_V_CONTROL_H
#define DELTA_V_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "delta_v_controller.h"

typedef enum {
    DELTAV_PID_SERIES   = 0,
    DELTAV_PID_STANDARD = 1,
    DELTAV_PID_PARALLEL = 2
} delta_v_pid_form_t;

typedef enum {
    DELTAV_PID_MAN     = 0,
    DELTAV_PID_AUT     = 1,
    DELTAV_PID_CAS     = 2,
    DELTAV_PID_RCAS    = 3,
    DELTAV_PID_ROUT    = 4,
    DELTAV_PID_IMAN    = 5,
    DELTAV_PID_LO      = 6,
    DELTAV_PID_IMAN_WP = 7
} delta_v_pid_mode_t;

typedef enum {
    DELTAV_PID_DIRECT  = 0,
    DELTAV_PID_REVERSE = 1
} delta_v_pid_action_t;

typedef enum {
    DELTAV_FB_SEL_HIGH  = 0,
    DELTAV_FB_SEL_LOW   = 1,
    DELTAV_FB_SEL_MID   = 2,
    DELTAV_FB_SEL_AVG   = 3,
    DELTAV_FB_SEL_MEDIAN = 4,
    DELTAV_FB_SEL_FIRST_GOOD = 5
} delta_v_selector_type_t;

typedef enum {
    DELTAV_SEQ_STEP_SET   = 0,
    DELTAV_SEQ_STEP_WAIT  = 1,
    DELTAV_SEQ_STEP_IF    = 2,
    DELTAV_SEQ_STEP_GOTO  = 3,
    DELTAV_SEQ_STEP_CALL  = 4,
    DELTAV_SEQ_STEP_RETURN = 5,
    DELTAV_SEQ_STEP_PARALLEL = 6
} delta_v_seq_step_type_t;

typedef struct {
    uint32_t    module_tag;
    char        module_name[32];
    delta_v_pid_form_t pid_form;
    delta_v_pid_mode_t mode;
    delta_v_pid_action_t action;
    double      gain;
    double      reset;
    double      rate;
    double      pv;
    double      sp;
    double      out;
    double      out_high_limit;
    double      out_low_limit;
    double      out_change_limit;
    double      sp_high_limit;
    double      sp_low_limit;
    double      pv_high_alarm;
    double      pv_low_alarm;
    double      dev_high_alarm;
    double      dev_low_alarm;
    double      integral;
    double      last_error;
    double      last_pv;
    double      feedforward_gain;
    double      feedforward_value;
    double      feedforward_scale;
    uint32_t    scan_period_ms;
    bool        tracking_active;
    double      tracking_value;
    uint8_t     cascade_input_pin[4];
    uint8_t     cascade_output_pin[4];
    bool        alarm_enable;
    bool        pv_bad;
    bool        out_bad;
    bool        anti_windup_active;
    bool        bumpless_enabled;
    bool        feedforward_enabled;
    uint8_t     remote_setpoint_pin[4];
    uint8_t     remote_out_pin[4];
} delta_v_pid_block_t;

typedef struct {
    uint16_t               step_number;
    delta_v_seq_step_type_t step_type;
    char                    description[64];
    uint16_t                param1;
    uint16_t                param2;
    double                  param3;
    bool                    condition;
    uint16_t                next_step_true;
    uint16_t                next_step_false;
} delta_v_seq_step_t;

typedef struct {
    char              seq_name[32];
    uint16_t          total_steps;
    delta_v_seq_step_t steps[64];
    uint16_t          current_step;
    bool              running;
    bool              held;
    bool              completed;
    uint32_t          elapsed_time_ms;
    uint32_t          step_start_time_ms;
} delta_v_sequence_block_t;

typedef enum {
    DELTAV_SPLIT_SEQUENTIAL = 0,
    DELTAV_SPLIT_OVERLAPPED = 1
} delta_v_split_mode_t;

typedef struct {
    delta_v_split_mode_t mode;
    double      split_point_percent;
    double      overlap_percent;
    double      valve_a_output;
    double      valve_b_output;
    double      input_signal;
    bool        valve_a_active;
    bool        valve_b_active;
} delta_v_split_range_t;

typedef struct {
    double      ratio_gain;
    double      ratio_bias;
    double      wildcard_input;
    double      controlled_input;
    double      output;
    double      output_high_limit;
    double      output_low_limit;
    bool        ratio_active;
    bool        clamp_active;
} delta_v_ratio_block_t;

#define DELTAV_CHAR_MAX_POINTS 21

typedef struct {
    double      x[DELTAV_CHAR_MAX_POINTS];
    double      y[DELTAV_CHAR_MAX_POINTS];
    uint8_t     point_count;
    bool        monotonic;
} delta_v_signal_characterizer_t;

typedef struct {
    double      deadtime_sec;
    double      lead_time_sec;
    double      lag_time_sec;
    double      gain;
    double      input;
    double      output;
    double      delayed_buffer[100];
    uint16_t    buffer_index;
    uint32_t    buffer_size;
    uint64_t    last_update_us;
} delta_v_lead_lag_block_t;

typedef struct {
    double      input_a;
    double      input_b;
    double      input_c;
    double      output;
    bool        input_a_valid;
    bool        input_b_valid;
    bool        input_c_valid;
    bool        voted_output_valid;
    double      deviation_limit;
    bool        deviation_alarm;
} delta_v_2oo3_voting_t;

typedef enum {
    DELTAV_MPC_DMC      = 0,
    DELTAV_MPC_GPC      = 1,
    DELTAV_MPC_ADAPTIVE = 2
} delta_v_mpc_algorithm_t;

typedef struct {
    delta_v_mpc_algorithm_t algorithm;
    uint8_t     mv_count;
    uint8_t     cv_count;
    uint8_t     dv_count;
    uint32_t    prediction_horizon;
    uint32_t    control_horizon;
    double      move_suppression[8];
    double      cv_weight[16];
    double      mv_low_limit[8];
    double      mv_high_limit[8];
    double      mv_rate_limit[8];
    double      cv_setpoint[16];
    double      a_matrix[16][8][60];
    bool        optimization_enabled;
    double      objective_value;
    uint32_t    solve_iterations;
    double      solve_tolerance;
    bool        step_test_active;
    uint32_t    model_switch_count;
    double      condition_number;
} delta_v_mpc_config_t;

typedef struct {
    uint8_t     input_count;
    uint8_t     hidden_count;
    uint8_t     hidden_layer_count;
    uint8_t     output_count;
    double      training_rate;
    double      momentum;
    double      weights[64][64];
    double      biases[64];
    bool        trained;
    double      training_rmse;
    uint32_t    training_epochs;
    bool        online_adaptation;
    double      adaptation_rate;
    double      confidence;
} delta_v_neural_config_t;

typedef enum {
    DELTAV_FUZZY_TRIANGLE  = 0,
    DELTAV_FUZZY_TRAPEZOID = 1,
    DELTAV_FUZZY_GAUSSIAN  = 2,
    DELTAV_FUZZY_SINGLETON = 3
} delta_v_fuzzy_shape_t;

typedef struct {
    char               set_name[16];
    delta_v_fuzzy_shape_t shape;
    double             param_a;
    double             param_b;
    double             param_c;
    double             param_d;
} delta_v_fuzzy_set_t;

typedef struct {
    delta_v_fuzzy_set_t input_sets[5][7];
    delta_v_fuzzy_set_t output_sets[3][7];
    uint8_t    input_count;
    uint8_t    output_count;
    uint8_t    rules[49][4];
    uint8_t    rule_count;
    double     defuzzification_output;
    bool       fuzzy_enabled;
} delta_v_fuzzy_config_t;

typedef enum {
    DELTAV_CMOD_TYPE_PID       = 0,
    DELTAV_CMOD_TYPE_AI        = 1,
    DELTAV_CMOD_TYPE_AO        = 2,
    DELTAV_CMOD_TYPE_DI        = 3,
    DELTAV_CMOD_TYPE_DO        = 4,
    DELTAV_CMOD_TYPE_CALC      = 5,
    DELTAV_CMOD_TYPE_SEQUENCE  = 6,
    DELTAV_CMOD_TYPE_MOTOR     = 7,
    DELTAV_CMOD_TYPE_VALVE     = 8,
    DELTAV_CMOD_TYPE_MPC       = 9,
    DELTAV_CMOD_TYPE_NEURAL    = 10,
    DELTAV_CMOD_TYPE_FUZZY     = 11,
    DELTAV_CMOD_TYPE_INTERLOCK = 12
} delta_v_cmod_type_t;

typedef struct {
    uint32_t    module_id;
    delta_v_cmod_type_t type;
    char        name[32];
    char        description[64];
    uint8_t     scan_phase;
    uint32_t    scan_period_ms;
    uint8_t     input_pin_count;
    uint8_t     output_pin_count;
    uint32_t    input_pins[16];
    uint32_t    output_pins[16];
    bool        active;
    bool        alarm_inhibit;
    uint8_t     display_priority;
    uint16_t    area_assignment;
} delta_v_control_module_t;

void delta_v_pid_block_init(delta_v_pid_block_t *pid, delta_v_pid_form_t form);
void delta_v_pid_block_set_gains(delta_v_pid_block_t *pid, double gain, double reset, double rate);
bool delta_v_pid_block_set_mode(delta_v_pid_block_t *pid, delta_v_pid_mode_t new_mode);
void delta_v_pid_block_calculate(delta_v_pid_block_t *pid, double dt_sec);
void delta_v_pid_block_bumpless_transfer(delta_v_pid_block_t *pid);
void delta_v_pid_block_anti_windup(delta_v_pid_block_t *pid);
void delta_v_pid_block_set_limits(delta_v_pid_block_t *pid, double out_hi, double out_lo);

void delta_v_sequence_block_init(delta_v_sequence_block_t *seq, const char *name);
bool delta_v_sequence_add_step(delta_v_sequence_block_t *seq, const delta_v_seq_step_t *step);
bool delta_v_sequence_start(delta_v_sequence_block_t *seq);
bool delta_v_sequence_hold(delta_v_sequence_block_t *seq);
bool delta_v_sequence_resume(delta_v_sequence_block_t *seq);
bool delta_v_sequence_execute(delta_v_sequence_block_t *seq, uint32_t dt_ms);

void delta_v_split_range_init(delta_v_split_range_t *sr, delta_v_split_mode_t mode, double split_point);
void delta_v_split_range_calculate(delta_v_split_range_t *sr, double input);

void delta_v_ratio_block_init(delta_v_ratio_block_t *rb, double gain, double bias);
void delta_v_ratio_block_calculate(delta_v_ratio_block_t *rb);

void delta_v_signal_characterizer_init(delta_v_signal_characterizer_t *sc);
bool delta_v_characterizer_add_point(delta_v_signal_characterizer_t *sc, double x, double y);
double delta_v_characterizer_interpolate(const delta_v_signal_characterizer_t *sc, double x);

void delta_v_lead_lag_init(delta_v_lead_lag_block_t *ll, double deadtime, double lead, double lag);
double delta_v_lead_lag_calculate(delta_v_lead_lag_block_t *ll, double input, double dt_sec);

void delta_v_2oo3_init(delta_v_2oo3_voting_t *v, double dev_limit);
double delta_v_2oo3_vote(delta_v_2oo3_voting_t *v, double a, double b, double c, bool av, bool bv, bool cv);

void delta_v_mpc_init(delta_v_mpc_config_t *mpc, uint8_t n_mv, uint8_t n_cv);
void delta_v_mpc_set_horizons(delta_v_mpc_config_t *mpc, uint32_t pred, uint32_t ctrl);
bool delta_v_mpc_solve(delta_v_mpc_config_t *mpc);
double delta_v_mpc_calc_move(const delta_v_mpc_config_t *mpc, uint8_t mv_index, uint8_t step);

void delta_v_neural_init(delta_v_neural_config_t *nn, uint8_t nin, uint8_t nhid, uint8_t nout);
double delta_v_neural_forward(delta_v_neural_config_t *nn, const double *inputs, double *outputs);
double delta_v_neural_train_step(delta_v_neural_config_t *nn, const double *inputs, const double *targets);

void delta_v_fuzzy_init(delta_v_fuzzy_config_t *fz);
bool delta_v_fuzzy_add_input_set(delta_v_fuzzy_config_t *fz, uint8_t input_idx, const delta_v_fuzzy_set_t *set);
bool delta_v_fuzzy_add_rule(delta_v_fuzzy_config_t *fz, uint8_t ir1, uint8_t ir2, uint8_t out);
double delta_v_fuzzy_infer(delta_v_fuzzy_config_t *fz, const double *inputs);

void delta_v_control_module_init(delta_v_control_module_t *cm, delta_v_cmod_type_t type, uint32_t id);

const char *delta_v_pid_mode_to_string(delta_v_pid_mode_t mode);
const char *delta_v_pid_form_to_string(delta_v_pid_form_t form);
const char *delta_v_cmod_type_to_string(delta_v_cmod_type_t type);

#endif
