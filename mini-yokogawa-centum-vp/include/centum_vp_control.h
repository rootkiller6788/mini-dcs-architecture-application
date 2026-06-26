#ifndef CENTUM_VP_CONTROL_H
#define CENTUM_VP_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "centum_vp_fcs.h"

typedef enum {
    PID_ALG_VELOCITY   = 0,
    PID_ALG_POSITIONAL = 1,
    PID_ALG_I_PD       = 2,
    PID_ALG_PI_D       = 3
} centum_pid_algorithm_t;

typedef enum {
    PID_MODE_MAN   = 0,
    PID_MODE_AUT   = 1,
    PID_MODE_CAS   = 2,
    PID_MODE_PRCAS = 3,
    PID_MODE_IMAN  = 4,
    PID_MODE_ROUT  = 5,
    PID_MODE_RCAS  = 6
} centum_pid_mode_t;

typedef enum {
    PID_ACT_DIRECT   = 0,
    PID_ACT_REVERSE  = 1
} centum_pid_action_t;

typedef struct {
    uint16_t    block_tag[8];
    uint16_t    block_number;
    centum_pid_mode_t mode;
    centum_pid_algorithm_t algorithm;
    centum_pid_action_t action;
    double      kp;
    double      ti;
    double      td;
    double      n;
    double      sv;
    double      pv;
    double      mv;
    double      mv_high_limit;
    double      mv_low_limit;
    double      mv_change_limit;
    double      sv_high_limit;
    double      sv_low_limit;
    double      dv_high_alarm;
    double      dv_low_alarm;
    double      vh_high_alarm;
    double      vl_low_alarm;
    double      vp_velocity_alarm;
    double      integral;
    double      last_error;
    double      last_pv;
    uint32_t    scan_period_ms;
    bool        tracking;
    double      tracking_input;
    bool        alarm_inhibit;
    bool        iop;
    bool        oop;
    bool        dv_hi_alarm_active;
    bool        dv_lo_alarm_active;
    bool        vh_hi_alarm_active;
    bool        vl_lo_alarm_active;
    bool        vp_vel_alarm_active;
    bool        mh_hi_alarm_active;
    bool        ml_lo_alarm_active;
    bool        anti_windup_active;
    bool        bumpless_active;
    bool        interlock_release;
} centum_pid_block_t;

typedef enum {
    SEQ_TYPE_RULE       = 0,
    SEQ_TYPE_TABLE      = 1,
    SEQ_TYPE_ST16       = 2,
    SEQ_TYPE_SEBOL      = 3
} centum_sequence_type_t;

typedef enum {
    SEQ_STATE_IDLE      = 0,
    SEQ_STATE_RUNNING   = 1,
    SEQ_STATE_HOLDING   = 2,
    SEQ_STATE_ABORTING  = 3,
    SEQ_STATE_ABORTED   = 4,
    SEQ_STATE_STOPPED   = 5,
    SEQ_STATE_COMPLETED = 6,
    SEQ_STATE_PAUSED    = 7,
    SEQ_STATE_RESTART   = 8
} centum_sequence_state_t;

typedef enum {
    COND_EQ     = 0,
    COND_NE     = 1,
    COND_GT     = 2,
    COND_GE     = 3,
    COND_LT     = 4,
    COND_LE     = 5,
    COND_AND    = 6,
    COND_OR     = 7,
    COND_TIMER  = 8,
    COND_COUNT  = 9
} centum_condition_type_t;

typedef enum {
    ACT_SET      = 0,
    ACT_RESET    = 1,
    ACT_PULSE    = 2,
    ACT_MOVE_VAL = 3,
    ACT_TIMER    = 4,
    ACT_COUNTER  = 5,
    ACT_MSG      = 6,
    ACT_PHASE    = 7,
    ACT_JUMP     = 8,
    ACT_CALL     = 9
} centum_action_type_t;

typedef struct {
    centum_condition_type_t cond_type;
    uint16_t    operand1_tag[8];
    uint16_t    operand2_tag[8];
    double      operand1_val;
    double      operand2_val;
    double      threshold;
    double      timer_sec;
    bool        invert;
    bool        last_result;
} centum_sequence_condition_t;

typedef struct {
    centum_action_type_t act_type;
    uint16_t    target_tag[8];
    double      set_value;
    double      timer_sec;
    uint16_t    jump_step;
    uint16_t    msg_number;
    char        message[80];
} centum_sequence_action_t;

typedef struct {
    uint16_t    step_number;
    char        step_label[16];
    centum_sequence_condition_t conditions[8];
    uint8_t     condition_count;
    centum_sequence_action_t true_actions[8];
    uint8_t     true_action_count;
    centum_sequence_action_t false_actions[8];
    uint8_t     false_action_count;
    uint32_t    step_duration_ms;
    bool        wait_for_condition;
} centum_sequence_step_t;

typedef struct {
    uint16_t    seq_block_tag[8];
    uint16_t    seq_block_number;
    centum_sequence_type_t seq_type;
    centum_sequence_state_t state;
    uint16_t    current_step;
    uint16_t    total_steps;
    centum_sequence_step_t steps[32];
    bool        enable;
    bool        hold_request;
    bool        abort_request;
    bool        step_timeout;
    uint32_t    cycle_count;
    time_t      start_time;
    time_t      last_transition;
} centum_sequence_block_t;

typedef enum {
    LC64_AND  = 0,
    LC64_OR   = 1,
    LC64_NOT  = 2,
    LC64_XOR  = 3,
    LC64_SR   = 4,
    LC64_RS   = 5,
    LC64_TON  = 6,
    LC64_TOF  = 7,
    LC64_CTU  = 8,
    LC64_CTD  = 9
} centum_lc64_element_t;

typedef struct {
    uint16_t    lc64_block_tag[8];
    uint16_t    lc64_block_number;
    uint64_t    input_mask;
    uint64_t    output_mask;
    uint8_t     element_types[64];
    uint8_t     element_input1[64];
    uint8_t     element_input2[64];
    uint8_t     element_count;
    bool        enable;
    bool        output_states[64];
    uint32_t    scan_count;
} centum_lc64_block_t;

typedef struct {
    double      input1_val;
    double      input2_val;
    double      input3_val;
    double      input4_val;
    double      output_val;
    uint8_t     select_mode;
    bool        select_high;
    bool        select_low;
    bool        select_mid;
    bool        select_avg;
} centum_selector_block_t;

typedef struct {
    double      mv1;
    double      mv2;
    double      output_low1;
    double      output_high1;
    double      output_low2;
    double      output_high2;
    double      split_point;
    bool        interlock_a;
    bool        interlock_b;
} centum_split_range_block_t;

void centum_pid_block_init(centum_pid_block_t *pid);
void centum_pid_block_set_tuning(centum_pid_block_t *pid, double kp, double ti, double td, double n);
void centum_pid_block_set_mode(centum_pid_block_t *pid, centum_pid_mode_t mode);
void centum_pid_block_set_sv(centum_pid_block_t *pid, double sv);
double centum_pid_block_execute(centum_pid_block_t *pid, double pv, double dt);
void centum_pid_block_handle_alarms(centum_pid_block_t *pid);
void centum_pid_block_bumpless_transfer(centum_pid_block_t *pid, double manual_mv);
void centum_pid_block_anti_windup_clamp(centum_pid_block_t *pid);

void centum_sequence_block_init(centum_sequence_block_t *seq, centum_sequence_type_t type);
bool centum_sequence_add_step(centum_sequence_block_t *seq, const centum_sequence_step_t *step);
void centum_sequence_execute(centum_sequence_block_t *seq);
void centum_sequence_hold(centum_sequence_block_t *seq);
void centum_sequence_abort(centum_sequence_block_t *seq);
void centum_sequence_reset(centum_sequence_block_t *seq);
uint16_t centum_sequence_get_current_step(const centum_sequence_block_t *seq);
centum_sequence_state_t centum_sequence_get_state(const centum_sequence_block_t *seq);

void centum_lc64_block_init(centum_lc64_block_t *lc64);
bool centum_lc64_add_element(centum_lc64_block_t *lc64, centum_lc64_element_t type,
                              uint8_t in1, uint8_t in2);
void centum_lc64_execute(centum_lc64_block_t *lc64, uint64_t inputs);
uint64_t centum_lc64_get_outputs(const centum_lc64_block_t *lc64);

void centum_selector_block_evaluate(centum_selector_block_t *sel);
void centum_split_range_calculate(const centum_split_range_block_t *splt, double input,
                                   double *out1, double *out2);
void centum_ratio_block_calculate(double flow1, double ratio_set, double bias,
                                   double *flow2_setpoint);

const char *centum_pid_mode_to_string(centum_pid_mode_t mode);
const char *centum_sequence_state_to_string(centum_sequence_state_t state);

#endif