#include "delta_v_control.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

void delta_v_pid_block_init(delta_v_pid_block_t *pid, delta_v_pid_form_t form)
{
    if (!pid) return;
    memset(pid, 0, sizeof(delta_v_pid_block_t));
    pid->pid_form = form;
    pid->mode = DELTAV_PID_MAN;
    pid->action = DELTAV_PID_DIRECT;
    pid->gain = 1.0; pid->reset = 60.0; pid->rate = 0.0;
    pid->out_high_limit = 100.0; pid->out_low_limit = 0.0;
    pid->out_change_limit = 25.0;
    pid->sp_high_limit = 100.0; pid->sp_low_limit = 0.0;
    pid->scan_period_ms = 100;
    pid->bumpless_enabled = true;
    pid->anti_windup_active = false;
    pid->pv_bad = true; pid->out_bad = true;
    pid->tracking_active = false;
}

void delta_v_pid_block_set_gains(delta_v_pid_block_t *pid, double gain, double reset, double rate)
{
    if (!pid) return;
    pid->gain = gain;
    pid->reset = (reset > 0.0) ? reset : 60.0;
    pid->rate = (rate >= 0.0) ? rate : 0.0;
}

bool delta_v_pid_block_set_mode(delta_v_pid_block_t *pid, delta_v_pid_mode_t new_mode)
{
    if (!pid) return false;
    if (pid->mode == new_mode) return true;
    switch (new_mode) {
    case DELTAV_PID_MAN:
        pid->tracking_active = true;
        break;
    case DELTAV_PID_AUT:
        if (pid->mode == DELTAV_PID_MAN && pid->bumpless_enabled)
            delta_v_pid_block_bumpless_transfer(pid);
        pid->tracking_active = false;
        break;
    case DELTAV_PID_CAS:
        if (pid->mode == DELTAV_PID_AUT) delta_v_pid_block_bumpless_transfer(pid);
        pid->tracking_active = false;
        break;
    default:
        break;
    }
    pid->mode = new_mode;
    return true;
}

void delta_v_pid_block_calculate(delta_v_pid_block_t *pid, double dt_sec)
{
    if (!pid || dt_sec <= 0.0 || pid->mode == DELTAV_PID_MAN) return;
    double error = pid->sp - pid->pv;
    double delta_pv = pid->pv - pid->last_pv;
    double prev_out = pid->out;
    double out_new;

    switch (pid->pid_form) {
    case DELTAV_PID_SERIES:
        pid->integral += error * dt_sec;
        out_new = pid->gain * (error + pid->integral / pid->reset - pid->rate * delta_pv / dt_sec);
        break;
    case DELTAV_PID_STANDARD:
        pid->integral += error * dt_sec;
        out_new = pid->gain * (error + pid->integral / pid->reset + pid->rate * (error - pid->last_error) / dt_sec);
        break;
    case DELTAV_PID_PARALLEL:
        pid->integral += error * dt_sec;
        out_new = pid->gain * error + pid->gain / pid->reset * pid->integral + pid->gain * pid->rate * (error - pid->last_error) / dt_sec;
        break;
    default:
        out_new = pid->out; break;
    }

    if (pid->feedforward_enabled)
        out_new += pid->feedforward_gain * pid->feedforward_value * pid->feedforward_scale;

    if (pid->out_change_limit > 0.0) {
        double delta = out_new - prev_out;
        if (fabs(delta) > pid->out_change_limit)
            out_new = prev_out + (delta > 0 ? pid->out_change_limit : -pid->out_change_limit);
    }

    if (out_new > pid->out_high_limit) { out_new = pid->out_high_limit; pid->anti_windup_active = true; }
    else if (out_new < pid->out_low_limit) { out_new = pid->out_low_limit; pid->anti_windup_active = true; }
    else { pid->anti_windup_active = false; }

    pid->out = out_new;
    pid->last_error = error;
    pid->last_pv = pid->pv;
    pid->pv_bad = false;
    pid->out_bad = false;
}

void delta_v_pid_block_bumpless_transfer(delta_v_pid_block_t *pid)
{
    if (!pid) return;
    double error = pid->sp - pid->pv;
    if (pid->gain > 0.0 && pid->reset > 0.0) {
        pid->integral = pid->reset * (pid->out / pid->gain - error);
    } else {
        pid->integral = 0.0;
    }
    pid->last_error = error;
    pid->last_pv = pid->pv;
    pid->bumpless_enabled = true;
}

void delta_v_pid_block_anti_windup(delta_v_pid_block_t *pid)
{
    if (!pid || pid->reset <= 0.0) return;
    if (pid->out >= pid->out_high_limit || pid->out <= pid->out_low_limit)
        pid->integral = fmax(0.0, pid->integral);
}

void delta_v_pid_block_set_limits(delta_v_pid_block_t *pid, double out_hi, double out_lo)
{
    if (!pid) return;
    pid->out_high_limit = out_hi;
    pid->out_low_limit = out_lo;
    if (pid->out > out_hi) pid->out = out_hi;
    if (pid->out < out_lo) pid->out = out_lo;
}

void delta_v_sequence_block_init(delta_v_sequence_block_t *seq, const char *name)
{
    if (!seq) return;
    memset(seq, 0, sizeof(delta_v_sequence_block_t));
    if (name) strncpy(seq->seq_name, name, sizeof(seq->seq_name) - 1);
    seq->current_step = 0;
}

bool delta_v_sequence_add_step(delta_v_sequence_block_t *seq, const delta_v_seq_step_t *step)
{
    if (!seq || !step || seq->total_steps >= 64) return false;
    seq->steps[seq->total_steps] = *step;
    seq->total_steps++;
    return true;
}

bool delta_v_sequence_start(delta_v_sequence_block_t *seq) {
    if (!seq || seq->total_steps == 0) return false;
    seq->running = true; seq->current_step = 0; seq->step_start_time_ms = 0;
    return true;
}
bool delta_v_sequence_hold(delta_v_sequence_block_t *seq) {
    if (!seq || !seq->running) return false;
    seq->held = true; return true;
}
bool delta_v_sequence_resume(delta_v_sequence_block_t *seq) {
    if (!seq || !seq->held) return false;
    seq->held = false; return true;
}

bool delta_v_sequence_execute(delta_v_sequence_block_t *seq, uint32_t dt_ms)
{
    if (!seq || !seq->running || seq->held || seq->completed) return false;
    if (seq->current_step >= seq->total_steps) { seq->completed = true; seq->running = false; return false; }
    delta_v_seq_step_t *step = &seq->steps[seq->current_step];
    seq->elapsed_time_ms += dt_ms;
    seq->step_start_time_ms += dt_ms;
    bool advance = false;
    switch (step->step_type) {
    case DELTAV_SEQ_STEP_SET: advance = true; break;
    case DELTAV_SEQ_STEP_WAIT: if (seq->step_start_time_ms >= step->param1) advance = true; break;
    case DELTAV_SEQ_STEP_IF: advance = step->condition; break;
    default: advance = true; break;
    }
    if (advance) {
        seq->current_step = (step->step_type == DELTAV_SEQ_STEP_IF && !step->condition) ?
                            step->next_step_false : step->next_step_true;
        seq->step_start_time_ms = 0;
    }
    return advance;
}

void delta_v_split_range_init(delta_v_split_range_t *sr, delta_v_split_mode_t mode, double split_point)
{
    if (!sr) return;
    memset(sr, 0, sizeof(delta_v_split_range_t));
    sr->mode = mode;
    sr->split_point_percent = (split_point >= 0.0 && split_point <= 100.0) ? split_point : 50.0;
}

void delta_v_split_range_calculate(delta_v_split_range_t *sr, double input)
{
    if (!sr) return;
    sr->input_signal = input;
    double pct = input;
    if (sr->mode == DELTAV_SPLIT_SEQUENTIAL) {
        if (pct <= sr->split_point_percent) {
            sr->valve_a_output = (pct / sr->split_point_percent) * 100.0;
            sr->valve_b_output = 0.0;
            sr->valve_a_active = true; sr->valve_b_active = false;
        } else {
            sr->valve_a_output = 100.0;
            sr->valve_b_output = ((pct - sr->split_point_percent) / (100.0 - sr->split_point_percent)) * 100.0;
            sr->valve_a_active = true; sr->valve_b_active = true;
        }
    } else {
        sr->valve_a_output = fmin(100.0, fmax(0.0, 100.0 * pct / sr->split_point_percent));
        sr->valve_b_output = fmin(100.0, fmax(0.0, 100.0 * (pct - sr->split_point_percent) / (100.0 - sr->split_point_percent)));
        sr->valve_a_active = (pct > 0.0); sr->valve_b_active = (pct >= sr->split_point_percent);
    }
}

void delta_v_ratio_block_init(delta_v_ratio_block_t *rb, double gain, double bias)
{
    if (!rb) return;
    memset(rb, 0, sizeof(delta_v_ratio_block_t));
    rb->ratio_gain = gain; rb->ratio_bias = bias;
    rb->output_high_limit = 100.0; rb->output_low_limit = 0.0;
}

void delta_v_ratio_block_calculate(delta_v_ratio_block_t *rb)
{
    if (!rb) return;
    double r = rb->ratio_gain * rb->wildcard_input + rb->ratio_bias;
    if (r > rb->output_high_limit) r = rb->output_high_limit;
    else if (r < rb->output_low_limit) r = rb->output_low_limit;
    rb->output = r; rb->controlled_input = rb->output;
}

void delta_v_signal_characterizer_init(delta_v_signal_characterizer_t *sc)
{
    if (!sc) return;
    memset(sc, 0, sizeof(delta_v_signal_characterizer_t));
    sc->monotonic = true;
}

bool delta_v_characterizer_add_point(delta_v_signal_characterizer_t *sc, double x, double y)
{
    if (!sc || sc->point_count >= DELTAV_CHAR_MAX_POINTS) return false;
    if (sc->point_count > 0 && x <= sc->x[sc->point_count-1]) sc->monotonic = false;
    sc->x[sc->point_count] = x; sc->y[sc->point_count] = y;
    sc->point_count++;
    return true;
}

double delta_v_characterizer_interpolate(const delta_v_signal_characterizer_t *sc, double x)
{
    if (!sc || sc->point_count < 2) return x;
    if (x <= sc->x[0]) return sc->y[0];
    if (x >= sc->x[sc->point_count-1]) return sc->y[sc->point_count-1];
    for (uint8_t i = 0; i < sc->point_count-1; i++) {
        if (x >= sc->x[i] && x <= sc->x[i+1]) {
            double d = sc->x[i+1] - sc->x[i];
            if (fabs(d) < 1e-12) return sc->y[i];
            return sc->y[i] + (sc->y[i+1]-sc->y[i])*(x-sc->x[i])/d;
        }
    }
    return x;
}

void delta_v_lead_lag_init(delta_v_lead_lag_block_t *ll, double deadtime, double lead, double lag)
{
    if (!ll) return;
    memset(ll, 0, sizeof(delta_v_lead_lag_block_t));
    ll->deadtime_sec = fmax(0.0, deadtime);
    ll->lead_time_sec = fmax(0.0, lead);
    ll->lag_time_sec = fmax(0.001, lag);
    ll->gain = 1.0;
}

double delta_v_lead_lag_calculate(delta_v_lead_lag_block_t *ll, double input, double dt_sec)
{
    if (!ll || dt_sec <= 0.0) return input;
    double alpha = dt_sec / (ll->lag_time_sec + dt_sec);
    ll->output = alpha * (ll->gain * input) + (1.0 - alpha) * ll->output;
    ll->input = input;
    return ll->output;
}

void delta_v_2oo3_init(delta_v_2oo3_voting_t *v, double dev_limit)
{
    if (!v) return;
    memset(v, 0, sizeof(delta_v_2oo3_voting_t));
    v->deviation_limit = dev_limit;
}

double delta_v_2oo3_vote(delta_v_2oo3_voting_t *v, double a, double b, double c, bool av, bool bv, bool cv)
{
    if (!v) return 0.0;
    v->input_a = a; v->input_b = b; v->input_c = c;
    v->input_a_valid = av; v->input_b_valid = bv; v->input_c_valid = cv;
    int vc = (av?1:0) + (bv?1:0) + (cv?1:0);
    if (vc < 2) { v->voted_output_valid = false; return 0.0; }
    if (vc == 2) {
        double x, y;
        if (av && bv) { x = a; y = b; }
        else if (av && cv) { x = a; y = c; }
        else { x = b; y = c; }
        v->output = (x + y) / 2.0;
        v->voted_output_valid = (fabs(x - y) <= v->deviation_limit);
        return v->output;
    }
    double d_ab = fabs(a-b), d_ac = fabs(a-c), d_bc = fabs(b-c);
    if (d_ab <= v->deviation_limit && d_ac <= v->deviation_limit && d_bc <= v->deviation_limit) {
        v->output = (a + b + c) / 3.0;
    } else if (d_ab <= v->deviation_limit) { v->output = (a+b)/2.0; }
    else if (d_ac <= v->deviation_limit) { v->output = (a+c)/2.0; }
    else if (d_bc <= v->deviation_limit) { v->output = (b+c)/2.0; }
    else { v->voted_output_valid = false; v->deviation_alarm = true; return 0.0; }
    v->voted_output_valid = true;
    return v->output;
}

void delta_v_mpc_init(delta_v_mpc_config_t *mpc, uint8_t n_mv, uint8_t n_cv)
{
    if (!mpc) return;
    memset(mpc, 0, sizeof(delta_v_mpc_config_t));
    mpc->algorithm = DELTAV_MPC_DMC;
    mpc->mv_count = (n_mv < 8) ? n_mv : 8;
    mpc->cv_count = (n_cv < 16) ? n_cv : 16;
    mpc->dv_count = 0;
    mpc->prediction_horizon = 60;
    mpc->control_horizon = 10;
    mpc->solve_tolerance = 1e-6;
    mpc->solve_iterations = 100;
    for (uint8_t i = 0; i < mpc->mv_count; i++) {
        mpc->move_suppression[i] = 1.0;
        mpc->mv_low_limit[i] = 0.0;
        mpc->mv_high_limit[i] = 100.0;
        mpc->mv_rate_limit[i] = 10.0;
    }
    for (uint8_t i = 0; i < mpc->cv_count; i++) {
        mpc->cv_weight[i] = 1.0;
        mpc->cv_setpoint[i] = 0.0;
    }
}

void delta_v_mpc_set_horizons(delta_v_mpc_config_t *mpc, uint32_t pred, uint32_t ctrl)
{
    if (!mpc) return;
    mpc->prediction_horizon = (pred > 0) ? pred : 60;
    mpc->control_horizon = (ctrl > 0 && ctrl < pred) ? ctrl : 10;
}

bool delta_v_mpc_solve(delta_v_mpc_config_t *mpc)
{
    if (!mpc || mpc->mv_count == 0 || mpc->cv_count == 0) return false;
    double error = 0.0;
    for (uint8_t i = 0; i < mpc->cv_count; i++) {
        double cv_error = mpc->cv_setpoint[i];
        for (uint8_t j = 0; j < mpc->mv_count; j++)
            cv_error -= mpc->a_matrix[i][j][0] * mpc->mv_low_limit[j];
        error += mpc->cv_weight[i] * cv_error * cv_error;
    }
    mpc->objective_value = error;
    if (error < mpc->solve_tolerance) { mpc->optimization_enabled = true; return true; }
    return (error < 1000.0);
}

double delta_v_mpc_calc_move(const delta_v_mpc_config_t *mpc, uint8_t mv_index, uint8_t step)
{
    if (!mpc || mv_index >= mpc->mv_count || step >= mpc->control_horizon) return 0.0;
    double move = 0.0;
    for (uint8_t j = 0; j < mpc->cv_count; j++) {
        double err = mpc->cv_setpoint[j];
        for (uint8_t k = 0; k < mpc->mv_count; k++)
            err -= mpc->a_matrix[j][k][step] * mpc->mv_low_limit[k];
        move += mpc->cv_weight[j] * err;
    }
    double denom = mpc->move_suppression[mv_index];
    for (uint8_t j = 0; j < mpc->cv_count; j++)
        denom += mpc->cv_weight[j] * mpc->a_matrix[j][mv_index][0] * mpc->a_matrix[j][mv_index][0];
    if (fabs(denom) < 1e-12) return 0.0;
    move /= denom;
    if (move > mpc->mv_rate_limit[mv_index]) move = mpc->mv_rate_limit[mv_index];
    if (move < -mpc->mv_rate_limit[mv_index]) move = -mpc->mv_rate_limit[mv_index];
    return move;
}

void delta_v_neural_init(delta_v_neural_config_t *nn, uint8_t nin, uint8_t nhid, uint8_t nout)
{
    if (!nn) return;
    memset(nn, 0, sizeof(delta_v_neural_config_t));
    nn->input_count = (nin < 64) ? nin : 63;
    nn->hidden_count = (nhid < 64) ? nhid : 63;
    nn->hidden_layer_count = 1;
    nn->output_count = (nout < 64) ? nout : 63;
    nn->training_rate = 0.01; nn->momentum = 0.9;
    nn->online_adaptation = false;
}

double delta_v_neural_forward(delta_v_neural_config_t *nn, const double *inputs, double *outputs)
{
    if (!nn || !inputs || !outputs || !nn->trained) return 0.0;
    for (uint8_t o = 0; o < nn->output_count; o++) {
        double sum = nn->biases[o];
        for (uint8_t h = 0; h < nn->hidden_count; h++)
            sum += nn->weights[h][o] * inputs[h % nn->input_count];
        outputs[o] = 1.0 / (1.0 + exp(-sum));
    }
    double error = 0.0;
    for (uint8_t o = 0; o < nn->output_count; o++) {
        double diff = outputs[o] - inputs[o % nn->input_count];
        error += diff * diff;
    }
    return sqrt(error / nn->output_count);
}

double delta_v_neural_train_step(delta_v_neural_config_t *nn, const double *inputs, const double *targets)
{
    if (!nn || !inputs || !targets) return 0.0;
    double outputs[64]; double error = 0.0;
    for (uint8_t o = 0; o < nn->output_count; o++) {
        double sum = nn->biases[o];
        for (uint8_t h = 0; h < nn->hidden_count; h++)
            sum += nn->weights[h][o] * inputs[h % nn->input_count];
        outputs[o] = 1.0 / (1.0 + exp(-sum));
        double diff = outputs[o] - targets[o];
        error += diff * diff;
        double delta = diff * outputs[o] * (1.0 - outputs[o]);
        nn->biases[o] -= nn->training_rate * delta;
        for (uint8_t h = 0; h < nn->hidden_count; h++)
            nn->weights[h][o] -= nn->training_rate * delta * inputs[h % nn->input_count];
    }
    error = sqrt(error / nn->output_count);
    nn->training_rmse = error; nn->training_epochs++;
    if (error < 0.01) nn->trained = true;
    return error;
}

void delta_v_fuzzy_init(delta_v_fuzzy_config_t *fz)
{
    if (!fz) return;
    memset(fz, 0, sizeof(delta_v_fuzzy_config_t));
    fz->input_count = 0; fz->output_count = 0; fz->rule_count = 0;
}

bool delta_v_fuzzy_add_input_set(delta_v_fuzzy_config_t *fz, uint8_t input_idx, const delta_v_fuzzy_set_t *set)
{
    if (!fz || !set || input_idx >= 5) return false;
    for (uint8_t i = 0; i < 7; i++) {
        if (fz->input_sets[input_idx][i].shape == 0) {
            fz->input_sets[input_idx][i] = *set;
            if (input_idx >= fz->input_count) fz->input_count = input_idx + 1;
            return true;
        }
    }
    return false;
}

bool delta_v_fuzzy_add_rule(delta_v_fuzzy_config_t *fz, uint8_t ir1, uint8_t ir2, uint8_t out)
{
    if (!fz || fz->rule_count >= 49) return false;
    fz->rules[fz->rule_count][0] = ir1;
    fz->rules[fz->rule_count][1] = ir2;
    fz->rules[fz->rule_count][2] = out;
    fz->rules[fz->rule_count][3] = 0;
    fz->rule_count++;
    return true;
}

double delta_v_fuzzy_infer(delta_v_fuzzy_config_t *fz, const double *inputs)
{
    if (!fz || !inputs || fz->rule_count == 0) return 0.0;
    double numerator = 0.0, denominator = 0.0;
    for (uint8_t r = 0; r < fz->rule_count; r++) {
        double mu1 = 1.0, mu2 = 1.0;
        uint8_t s1 = fz->rules[r][0], s2 = fz->rules[r][1];
        if (s1 < 7 && fz->input_sets[0][s1].shape != 0) {
            double x = inputs[0], a = fz->input_sets[0][s1].param_b;
            double b = fz->input_sets[0][s1].param_c;
            mu1 = (x <= a || x >= b) ? 0.0 : ((x - a) / (fz->input_sets[0][s1].param_a - a));
        }
        if (s2 < 7 && fz->input_count > 1 && fz->input_sets[1][s2].shape != 0) {
            double x = inputs[1], a = fz->input_sets[1][s2].param_b;
            double b = fz->input_sets[1][s2].param_c;
            mu2 = (x <= a || x >= b) ? 0.0 : ((x - a) / (fz->input_sets[1][s2].param_a - a));
        }
        double firing = (mu1 < mu2) ? mu1 : mu2;
        numerator += firing * fz->rules[r][2];
        denominator += firing;
    }
    fz->defuzzification_output = (denominator > 1e-12) ? numerator / denominator : 0.0;
    return fz->defuzzification_output;
}

void delta_v_control_module_init(delta_v_control_module_t *cm, delta_v_cmod_type_t type, uint32_t id)
{
    if (!cm) return;
    memset(cm, 0, sizeof(delta_v_control_module_t));
    cm->module_id = id; cm->type = type;
    cm->scan_period_ms = 100; cm->scan_phase = 0;
}

const char *delta_v_pid_mode_to_string(delta_v_pid_mode_t mode) {
    static const char *s[] = {"MAN","AUT","CAS","RCAS","ROUT","IMAN","LO","IMAN_WP"};
    return (mode <= DELTAV_PID_IMAN_WP) ? s[mode] : "Unknown";
}

const char *delta_v_pid_form_to_string(delta_v_pid_form_t form) {
    static const char *s[] = {"Series","Standard","Parallel"};
    return (form <= DELTAV_PID_PARALLEL) ? s[form] : "Unknown";
}

const char *delta_v_cmod_type_to_string(delta_v_cmod_type_t type) {
    static const char *s[] = {"PID","AI","AO","DI","DO","CALC","SEQ","MOTOR","VALVE","MPC","NEURAL","FUZZY","INTLCK"};
    return (type <= DELTAV_CMOD_TYPE_INTERLOCK) ? s[type] : "Unknown";
}

typedef struct {
    bool    input_states[16];
    bool    output_states[16];
    uint8_t input_count;
    uint8_t output_count;
    bool    enabled;
    bool    tripped;
} delta_v_interlock_block_t;

void delta_v_interlock_init(delta_v_interlock_block_t *il)
{
    if (!il) return;
    memset(il, 0, sizeof(delta_v_interlock_block_t));
}

bool delta_v_interlock_evaluate(delta_v_interlock_block_t *il, bool condition)
{
    if (!il || !il->enabled) return false;
    if (condition) { il->tripped = true; il->output_states[0] = false; return true; }
    il->tripped = false; il->output_states[0] = true; return false;
}

typedef struct {
    double      input_a;
    double      input_b;
    double      output;
    double      bias;
    char        expression[32];
} delta_v_calc_block_t;

void delta_v_calc_block_init(delta_v_calc_block_t *cb)
{
    if (!cb) return;
    memset(cb, 0, sizeof(delta_v_calc_block_t));
    cb->bias = 0.0;
}

double delta_v_calc_block_add(delta_v_calc_block_t *cb) {
    if (!cb) return 0.0;
    cb->output = cb->input_a + cb->input_b + cb->bias;
    return cb->output;
}

double delta_v_calc_block_subtract(delta_v_calc_block_t *cb) {
    if (!cb) return 0.0;
    cb->output = cb->input_a - cb->input_b + cb->bias;
    return cb->output;
}

double delta_v_calc_block_multiply(delta_v_calc_block_t *cb) {
    if (!cb) return 0.0;
    cb->output = cb->input_a * cb->input_b + cb->bias;
    return cb->output;
}

double delta_v_calc_block_divide(delta_v_calc_block_t *cb) {
    if (!cb) return 0.0;
    cb->output = (fabs(cb->input_b) > 1e-12) ? cb->input_a / cb->input_b + cb->bias : 0.0;
    return cb->output;
}

double delta_v_calc_block_sqrt(delta_v_calc_block_t *cb) {
    if (!cb) return 0.0;
    cb->output = (cb->input_a >= 0.0) ? sqrt(cb->input_a) + cb->bias : 0.0;
    return cb->output;
}

bool delta_v_pid_block_check_alarm(delta_v_pid_block_t *pid)
{
    if (!pid || !pid->alarm_enable) return false;
    bool alarm = false;
    if (pid->pv > pid->pv_high_alarm) { pid->pv_bad = true; alarm = true; }
    if (pid->pv < pid->pv_low_alarm) { pid->pv_bad = true; alarm = true; }
    double dev = fabs(pid->pv - pid->sp);
    if (dev > pid->dev_high_alarm) alarm = true;
    return alarm;
}

double delta_v_pid_block_calculate_loop_performance(const delta_v_pid_block_t *pid)
{
    if (!pid) return 0.0;
    double error = fabs(pid->sp - pid->pv);
    double span = pid->sp_high_limit - pid->sp_low_limit;
    if (span < 1e-12) return 0.0;
    return (1.0 - error / span) * 100.0;
}

double delta_v_pid_block_simulate_process(double mv, double gain, double time_constant, double deadtime, double prev_pv, double dt_sec)
{
    (void)deadtime;
    double alpha = dt_sec / (time_constant + dt_sec);
    return alpha * gain * mv + (1.0 - alpha) * prev_pv;
}
