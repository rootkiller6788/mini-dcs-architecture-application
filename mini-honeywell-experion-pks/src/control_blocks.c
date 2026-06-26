/**
 * @file control_blocks.c
 * @brief Experion PKS Control Blocks Implementation
 *
 * Implements: PID controller (ISA standard form, velocity form),
 * cascade control, feedforward (static + dynamic lead-lag),
 * ratio control, split-range, override selector, signal characterizer,
 * lead-lag compensator.
 *
 * L5: PID algorithm, bumpless transfer, anti-windup
 * L2: Cascade, feedforward, ratio, split-range concepts
 */

#include "../include/control_blocks.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ==========================================================================
 * L5 - PID Parameters Initialization
 * ========================================================================== */

void pid_params_init(PIDParams *params)
{
    if (!params) return;
    memset(params, 0, sizeof(PIDParams));
    params->kc = 1.0;
    params->ti_sec = 0.0;       /* No integral by default */
    params->td_sec = 0.0;       /* No derivative by default */
    params->ts_sec = 0.25;      /* 250ms default */
    params->form = PID_FORM_ISA_STANDARD;
    params->action = PID_REVERSE;
    params->sp_hi_limit = 100.0;
    params->sp_lo_limit = 0.0;
    params->op_hi_limit = 100.0;
    params->op_lo_limit = 0.0;
    params->op_rate_limit = 0.0;
    params->deadband = 0.0;
    params->gap_width = 0.0;
    params->anti_windup_enabled = true;
    params->windup_limit_hi = 100.0;
    params->windup_limit_lo = 0.0;
    params->pv_filter_time_sec = 0.0;
    params->d_filter_time_sec = 0.0;
}

void pid_state_init(PIDState *state)
{
    if (!state) return;
    memset(state, 0, sizeof(PIDState));
    state->mode = PID_MANUAL;
    state->op = 0.0;
    state->pv_quality = XQUAL_GOOD;
}

int pid_block_init(PIDControlBlock *block, uint32_t id, const char *tag)
{
    if (!block || !tag) return -1;
    memset(block, 0, sizeof(PIDControlBlock));
    block->block_id = id;
    strncpy(block->tag, tag, sizeof(block->tag) - 1);
    block->type = CB_REG_PID;
    pid_params_init(&block->params);
    pid_state_init(&block->state);
    block->enabled = false;
    block->exec_order = 0;
    block->period_mult = 1;
    return 0;
}

/* ==========================================================================
 * L5 - PID Tuning and Limit Configuration
 * ========================================================================== */

int pid_set_tuning(PIDControlBlock *block, double kc, double ti_sec, double td_sec)
{
    if (!block) return -1;
    if (kc < 0.0) return -1;   /* Gain must be non-negative */
    if (ti_sec < 0.0) return -1;
    if (td_sec < 0.0) return -1;

    block->params.kc = kc;
    block->params.ti_sec = ti_sec;
    block->params.td_sec = td_sec;
    return 0;
}

int pid_set_limits(PIDControlBlock *block, double sp_lo, double sp_hi,
                   double op_lo, double op_hi)
{
    if (!block) return -1;
    if (sp_lo >= sp_hi) return -1;
    if (op_lo >= op_hi) return -1;

    block->params.sp_lo_limit = sp_lo;
    block->params.sp_hi_limit = sp_hi;
    block->params.op_lo_limit = op_lo;
    block->params.op_hi_limit = op_hi;
    return 0;
}

/* ==========================================================================
 * L5 - PID Mode Control with Bumpless Transfer
 * ========================================================================== */

int pid_set_mode(PIDControlBlock *block, PIDMode mode)
{
    if (!block) return -1;

    PIDMode old_mode = block->state.mode;

    /* Bumpless transfer on mode transitions */
    switch (mode) {
    case PID_MANUAL:
        /* When switching to manual, preserve current output */
        break;

    case PID_AUTO:
        if (old_mode == PID_MANUAL) {
            /* Bumpless: initialize integral term so output doesn't jump */
            /* Set SP = PV to start with zero error */
            block->state.sp = block->state.pv;
            block->state.integral_term = block->state.op;
        }
        break;

    case PID_CASCADE:
        /* Remote setpoint tracking */
        block->state.sp = block->state.remote_sp;
        break;

    case PID_INITIALIZE:
        /* Track external output value */
        block->state.op = block->state.track_op;
        block->state.integral_term = block->state.track_op;
        break;

    default:
        break;
    }

    block->state.mode = mode;
    return 0;
}

/* ==========================================================================
 * L5 - PID Execution: ISA Standard Form (Velocity Algorithm)
 * ========================================================================== */

/**
 * PID execution in velocity (incremental) form — ISA Standard.
 *
 * Continuous transfer function (ISA form):
 *   Gc(s) = Kc * (1 + 1/(Ti*s) + Td*s)
 *
 * Discretized using backward differences:
 *   Integral term: I_k = I_{k-1} + (Kc * Ts / Ti) * e_k
 *   Derivative term: D_k = (Kc * Td / Ts) * (e_k - e_{k-1})
 *
 * Velocity (incremental) output:
 *   delta_OP = Kc * [ (e_k - e_{k-1}) + (Ts/Ti)*e_k + (Td/Ts)*(e_k - 2*e_{k-1} + e_{k-2}) ]
 *   OP_k = OP_{k-1} + delta_OP
 *
 * Anti-windup via conditional integration:
 *   If OP_k is clamped and error drives further into saturation,
 *   freeze integral update.
 *
 * Reference: Astrom & Hagglund, PID Controllers (1995), Ch.3
 * Course: MIT 2.171, Stanford ENGR205
 */
int pid_execute(PIDControlBlock *block, double pv, double dt_sec, double *output)
{
    if (!block || !output) return -1;
    if (dt_sec <= 0.0) return -1;

    PIDParams *p = &block->params;
    PIDState *s = &block->state;

    /* Store previous values for derivative calculation */
    double prev_op = s->op;

    /* Update PV with optional filtering */
    if (p->pv_filter_time_sec > 0.0) {
        double alpha = 1.0 - exp(-dt_sec / p->pv_filter_time_sec);
        s->pv = alpha * pv + (1.0 - alpha) * s->pv;
    } else {
        s->pv = pv;
    }

    /* If in MANUAL or INITIALIZE mode, don't compute */
    if (s->mode == PID_MANUAL || s->mode == PID_INITIALIZE ||
        s->mode == PID_EMERGENCY) {
        *output = s->op;
        return 0;
    }

    /* Calculate error based on action direction */
    double error;
    if (p->action == PID_REVERSE) {
        error = s->sp - s->pv;
    } else {
        error = s->pv - s->sp; /* DIRECT acting */
    }

    /* Deadband */
    if (p->deadband > 0.0 && fabs(error) < p->deadband) {
        error = 0.0;
    }

    s->error = error;

    /* --- Proportional term --- */
    double p_term = p->kc * error;

    /* --- Integral term with anti-windup --- */
    double i_increment = 0.0;
    if (p->ti_sec > 0.0) {
        i_increment = p->kc * (dt_sec / p->ti_sec) * error;

        /* Anti-windup: conditional integration */
        if (p->anti_windup_enabled) {
            double projected = s->integral_term + i_increment + p_term;

            /* If output would saturate and integral drives further out, freeze */
            if (projected > p->windup_limit_hi && i_increment > 0.0) {
                i_increment = 0.0;
            } else if (projected < p->windup_limit_lo && i_increment < 0.0) {
                i_increment = 0.0;
            }
        }

        s->integral_term += i_increment;

        /* Clamp integral term */
        if (s->integral_term > p->windup_limit_hi)
            s->integral_term = p->windup_limit_hi;
        if (s->integral_term < p->windup_limit_lo)
            s->integral_term = p->windup_limit_lo;
    }

    /* --- Derivative term --- */
    double d_term = 0.0;
    if (p->td_sec > 0.0) {
        /* Derivative on error (standard form) */
        double derivative = (error - s->prev_error) / dt_sec;
        d_term = p->kc * p->td_sec * derivative;

        /* Optional derivative filter */
        if (p->d_filter_time_sec > 0.0) {
            double alpha_d = dt_sec / (dt_sec + p->d_filter_time_sec);
            d_term = alpha_d * d_term + (1.0 - alpha_d) * s->derivative_term;
        }
    }

    /* --- Compute total output --- */
    double raw_op = p_term + s->integral_term + d_term;

    /* --- Output clamping --- */
    if (raw_op > p->op_hi_limit) raw_op = p->op_hi_limit;
    if (raw_op < p->op_lo_limit) raw_op = p->op_lo_limit;

    /* --- Output rate limiting --- */
    if (p->op_rate_limit > 0.0) {
        double max_change = p->op_rate_limit * dt_sec;
        double change = raw_op - prev_op;
        if (change > max_change) {
            raw_op = prev_op + max_change;
        } else if (change < -max_change) {
            raw_op = prev_op - max_change;
        }
    }

    /* Store state for next execution */
    s->prev_error = error;
    s->prev_op = raw_op;
    s->op = raw_op;
    s->p_term = p_term;
    s->i_term = i_increment;
    s->d_term = d_term;

    *output = raw_op;
    return 0;
}

/* ==========================================================================
 * L5 - PID Velocity Form
 * ========================================================================== */

/**
 * PID velocity (incremental) form — outputs delta_OP instead of absolute OP.
 *
 * This is the form used in DCS controllers for bumpless transfer,
 * since the output is inherently incremental and can be summed
 * by a downstream integrator.
 *
 * delta_OP = Kc * [ (e_k - e_{k-1}) + (Ts/Ti) * e_k + (Td/Ts) * (e_k - 2*e_{k-1} + e_{k-2}) ]
 */
int pid_execute_velocity(PIDControlBlock *block, double pv, double dt_sec,
                          double *delta_op)
{
    if (!block || !delta_op) return -1;
    if (dt_sec <= 0.0) return -1;

    PIDParams *p = &block->params;
    PIDState *s = &block->state;

    s->pv = pv;

    double error;
    if (p->action == PID_REVERSE) {
        error = s->sp - s->pv;
    } else {
        error = s->pv - s->sp;
    }

    double de = error - s->prev_error;
    double d2e = error - 2.0 * s->prev_error + s->prev2_error;

    double delta = p->kc * (de + (dt_sec / p->ti_sec) * error +
                             (p->td_sec / dt_sec) * d2e);

    /* Clamp delta */
    if (p->op_rate_limit > 0.0) {
        double max_delta = p->op_rate_limit * dt_sec;
        if (delta > max_delta) delta = max_delta;
        if (delta < -max_delta) delta = -max_delta;
    }

    s->prev2_error = s->prev_error;
    s->prev_error = error;
    s->prev_op += delta;

    if (s->prev_op > p->op_hi_limit) s->prev_op = p->op_hi_limit;
    if (s->prev_op < p->op_lo_limit) s->prev_op = p->op_lo_limit;

    s->op = s->prev_op;

    *delta_op = delta;
    return 0;
}

/* ==========================================================================
 * L5 - Bumpless Transfer
 * ========================================================================== */

int pid_bumpless_transfer(PIDControlBlock *block, double current_op)
{
    if (!block) return -1;

    block->state.op = current_op;
    block->state.prev_op = current_op;
    block->state.integral_term = current_op;
    block->state.sp = block->state.pv;  /* Initialize SP = PV */
    block->state.prev_error = 0.0;
    block->state.prev2_error = 0.0;

    return 0;
}

/* ==========================================================================
 * L5 - PID Term Query
 * ========================================================================== */

int pid_get_terms(const PIDControlBlock *block, double *p, double *i, double *d)
{
    if (!block) return -1;
    if (p) *p = block->state.p_term;
    if (i) *i = block->state.i_term;
    if (d) *d = block->state.d_term;
    return 0;
}

/* ==========================================================================
 * L2 - Cascade Control
 * ========================================================================== */

int cascade_pair_init(CascadePair *cp, uint32_t master_id, uint32_t slave_id)
{
    if (!cp) return -1;
    memset(cp, 0, sizeof(CascadePair));
    cp->master_block_id = master_id;
    cp->slave_block_id = slave_id;
    cp->cascade_active = false;
    cp->sp_ratio = 1.0;
    cp->sp_bias = 0.0;
    cp->bumpless_on_master_fail = true;
    return 0;
}

int cascade_engage(CascadePair *cp, bool engage)
{
    if (!cp) return -1;
    cp->cascade_active = engage;
    return 0;
}

/** Calculate slave setpoint from master output.
 *  slave_sp = master_op * sp_ratio + sp_bias */
int cascade_calculate_sp(const CascadePair *cp, double master_op, double *slave_sp)
{
    if (!cp || !slave_sp) return -1;
    if (!cp->cascade_active) return -1;

    *slave_sp = master_op * cp->sp_ratio + cp->sp_bias;
    return 0;
}

/* ==========================================================================
 * L2 - Feedforward Control (Static + Dynamic)
 * ========================================================================== */

void feedforward_init(FeedforwardBlock *ff, double ff_gain, double lead_sec,
                       double lag_sec, double ts_sec)
{
    if (!ff) return;
    memset(ff, 0, sizeof(FeedforwardBlock));
    ff->ff_gain = ff_gain;
    ff->lead_time_sec = lead_sec;
    ff->lag_time_sec = lag_sec;
    ff->ts_sec = ts_sec;
    ff->dynamic_enabled = (lead_sec > 0.0 || lag_sec > 0.0);
    ff->static_only = !ff->dynamic_enabled;
}

/**
 * Execute feedforward computation.
 *
 * Static: total = PID_output + FF_gain * FF_signal + FF_bias
 *
 * Dynamic lead-lag compensation (Tustin discretization):
 *   H(z) = FF_gain * (b0 + b1*z^{-1}) / (1 + a1*z^{-1})
 *   where b0 = (2*T_lead + Ts) / (2*T_lag + Ts)
 *         b1 = (Ts - 2*T_lead) / (2*T_lag + Ts)
 *         a1 = (Ts - 2*T_lag) / (2*T_lag + Ts)
 *
 * Reference: Seborg, Edgar, Mellichamp, Process Dynamics and Control, Ch.15
 * Course: Stanford ENGR205 — feedforward control
 */
int feedforward_execute(FeedforwardBlock *ff, double ff_signal, double pid_output,
                         double *total_output)
{
    if (!ff || !total_output) return -1;

    double ff_contribution;

    if (ff->static_only || !ff->dynamic_enabled) {
        /* Static feedforward only */
        ff_contribution = ff->ff_gain * ff_signal + ff->ff_bias;
    } else {
        /* Dynamic lead-lag compensation via Tustin */
        double Ts = ff->ts_sec;
        double T_lead = ff->lead_time_sec;
        double T_lag = ff->lag_time_sec;

        if (T_lag < Ts / 10.0) T_lag = Ts / 10.0; /* Avoid singularity */

        double denom = 2.0 * T_lag + Ts;

        /* Tustin coefficients */
        double b0 = (2.0 * T_lead + Ts) / denom;
        double b1 = (Ts - 2.0 * T_lead) / denom;
        double a1 = (Ts - 2.0 * T_lag) / denom;

        /* Difference equation: y_k = b0*x_k + b1*x_{k-1} - a1*y_{k-1} */
        double y_k = ff->ff_gain * (b0 * ff_signal + b1 * ff->prev_ff_signal)
                     - a1 * ff->prev_ff_output;

        ff->prev_ff_signal = ff_signal;
        ff->prev_ff_output = y_k;
        ff_contribution = y_k + ff->ff_bias;
    }

    *total_output = pid_output + ff_contribution;
    return 0;
}

/* ==========================================================================
 * L2 - Ratio Control
 * ========================================================================== */

int ratio_block_init(RatioBlock *ratio, double target_ratio)
{
    if (!ratio) return -1;
    memset(ratio, 0, sizeof(RatioBlock));
    ratio->ratio = target_ratio;
    ratio->ratio_min = 0.0;
    ratio->ratio_max = 10.0;
    ratio->ratio_clamp = true;
    return 0;
}

int ratio_execute(RatioBlock *ratio, double wild_flow, double *controlled_sp)
{
    if (!ratio || !controlled_sp) return -1;

    double r = ratio->ratio;
    if (ratio->ratio_clamp) {
        if (r < ratio->ratio_min) r = ratio->ratio_min;
        if (r > ratio->ratio_max) r = ratio->ratio_max;
    }

    ratio->wild_flow = wild_flow;
    ratio->controlled_sp = r * wild_flow + ratio->bias;
    *controlled_sp = ratio->controlled_sp;
    return 0;
}

/* ==========================================================================
 * L2 - Split-Range Control
 * ========================================================================== */

int split_range_init(SplitRangeBlock *sr, int num_ranges)
{
    if (!sr) return -1;
    if (num_ranges < 1 || num_ranges > SPLIT_MAX_RANGES) return -1;
    memset(sr, 0, sizeof(SplitRangeBlock));
    sr->active_ranges = num_ranges;
    return 0;
}

int split_range_set_breakpoint(SplitRangeBlock *sr, int range_idx,
                                double start_pct, double end_pct)
{
    if (!sr) return -1;
    if (range_idx < 0 || range_idx >= sr->active_ranges) return -1;
    if (start_pct < 0.0 || end_pct > 100.0 || start_pct >= end_pct) return -1;

    sr->range_start[range_idx] = start_pct;
    sr->range_end[range_idx] = end_pct;
    return 0;
}

/** Map PID output (0-100%) to multiple output ranges.
 *  Each range maps [range_start, range_end] of PID input to [0, 100%] output. */
int split_range_execute(SplitRangeBlock *sr, double pid_output, double *outputs)
{
    if (!sr || !outputs) return -1;

    for (int i = 0; i < sr->active_ranges; i++) {
        double in = pid_output;
        double start = sr->range_start[i];
        double end = sr->range_end[i];

        if (in < start) {
            outputs[i] = 0.0;
        } else if (in > end) {
            outputs[i] = 100.0;
        } else {
            /* Linear interpolation within range */
            double frac = (end > start) ? (in - start) / (end - start) : 0.0;
            outputs[i] = frac * 100.0;
        }
        sr->output[i] = outputs[i];
    }
    return 0;
}

/* ==========================================================================
 * L2 - Override Selector
 * ========================================================================== */

int override_selector_init(OverrideSelector *os, OverrideSelectType sel_type,
                            int n_inputs)
{
    if (!os) return -1;
    if (n_inputs < 1 || n_inputs > OVRD_MAX_INPUTS) return -1;
    memset(os, 0, sizeof(OverrideSelector));
    os->select_type = sel_type;
    os->input_count = n_inputs;
    return 0;
}

int override_selector_set_input(OverrideSelector *os, int idx, double value)
{
    if (!os) return -1;
    if (idx < 0 || idx >= os->input_count) return -1;
    os->inputs[idx] = value;
    return 0;
}

/** Select output based on selection criterion. */
int override_selector_execute(OverrideSelector *os, double *selected)
{
    if (!os || !selected) return -1;
    if (os->input_count < 1) return -1;

    int best_idx = 0;
    double best_val = os->inputs[0];

    for (int i = 1; i < os->input_count; i++) {
        switch (os->select_type) {
        case OVRD_HIGH_SELECT:
            if (os->inputs[i] > best_val) { best_val = os->inputs[i]; best_idx = i; }
            break;
        case OVRD_LOW_SELECT:
            if (os->inputs[i] < best_val) { best_val = os->inputs[i]; best_idx = i; }
            break;
        case OVRD_MEDIAN_SELECT:
            /* Selection requires sorting */
            break;
        }
    }

    if (os->select_type == OVRD_MEDIAN_SELECT && os->input_count >= 3) {
        /* Simple median via partial sort (bubble 3 elements) */
        double a = os->inputs[0], b = os->inputs[1], c = os->inputs[2];
        if (a > b) { double t = a; a = b; b = t; }
        if (b > c) { double t = b; b = c; c = t; }
        if (a > b) { double t = a; a = b; b = t; }
        best_val = b;
        for (int i = 0; i < os->input_count; i++) {
            if (os->inputs[i] == best_val) { best_idx = i; break; }
        }
    }

    os->selected_output = best_val;
    os->selected_index = best_idx;
    *selected = best_val;
    return 0;
}

/* ==========================================================================
 * L3 - Signal Characterizer (Piecewise Linear)
 * ========================================================================== */

int signal_char_init(SignalCharacterizer *sc)
{
    if (!sc) return -1;
    memset(sc, 0, sizeof(SignalCharacterizer));
    return 0;
}

int signal_char_add_point(SignalCharacterizer *sc, double x, double y)
{
    if (!sc) return -1;
    if (sc->num_points >= CHAR_MAX_BREAKPOINTS) return -1;

    int n = sc->num_points;

    /* Insert maintaining monotonic x */
    int pos = n;
    for (int i = 0; i < n; i++) {
        if (x < sc->x[i]) { pos = i; break; }
        if (fabs(x - sc->x[i]) < 1e-12) return -1; /* Duplicate x */
    }

    /* Shift right */
    for (int i = n; i > pos; i--) {
        sc->x[i] = sc->x[i - 1];
        sc->y[i] = sc->y[i - 1];
    }

    sc->x[pos] = x;
    sc->y[pos] = y;
    sc->num_points++;
    return 0;
}

/** Evaluate piecewise linear function at x using linear interpolation. */
int signal_char_evaluate(const SignalCharacterizer *sc, double x, double *y)
{
    if (!sc || !y) return -1;
    if (sc->num_points < 2) return -1;

    /* Extrapolation below first point */
    if (x <= sc->x[0]) {
        *y = sc->y[0];
        return 0;
    }
    /* Extrapolation above last point */
    if (x >= sc->x[sc->num_points - 1]) {
        *y = sc->y[sc->num_points - 1];
        return 0;
    }

    /* Find segment and linearly interpolate */
    for (int i = 0; i < sc->num_points - 1; i++) {
        if (x >= sc->x[i] && x <= sc->x[i + 1]) {
            double dx = sc->x[i + 1] - sc->x[i];
            double dy = sc->y[i + 1] - sc->y[i];
            if (fabs(dx) < 1e-12) {
                *y = sc->y[i];
            } else {
                *y = sc->y[i] + (x - sc->x[i]) * dy / dx;
            }
            return 0;
        }
    }

    *y = sc->y[sc->num_points - 1];
    return 0;
}

/* ==========================================================================
 * L2 - Lead-Lag Dynamic Compensator
 * ========================================================================== */

int leadlag_init(LeadLagBlock *ll, double gain, double lead_sec,
                  double lag_sec, double ts_sec)
{
    if (!ll) return -1;
    if (ts_sec <= 0.0) return -1;
    memset(ll, 0, sizeof(LeadLagBlock));
    ll->gain = gain;
    ll->lead_time_sec = lead_sec;
    ll->lag_time_sec = lag_sec;
    ll->ts_sec = ts_sec;
    return 0;
}

/** Execute lead-lag using Tustin discretization. */
int leadlag_execute(LeadLagBlock *ll, double input, double *output)
{
    if (!ll || !output) return -1;

    double Ts = ll->ts_sec;
    double T_lead = ll->lead_time_sec;
    double T_lag = ll->lag_time_sec;

    if (!ll->initialized) {
        ll->prev_input = input;
        ll->prev_output = input * ll->gain;
        ll->initialized = true;
        *output = ll->prev_output;
        return 0;
    }

    if (T_lag < Ts / 10.0) {
        /* Pure derivative behavior when lag is very small */
        double deriv = (input - ll->prev_input) / Ts;
        *output = ll->gain * (input + T_lead * deriv);
    } else {
        double denom = 2.0 * T_lag + Ts;
        double b0 = ll->gain * (2.0 * T_lead + Ts) / denom;
        double b1 = ll->gain * (Ts - 2.0 * T_lead) / denom;
        double a1 = (Ts - 2.0 * T_lag) / denom;

        *output = b0 * input + b1 * ll->prev_input - a1 * ll->prev_output;
    }

    ll->prev_input = input;
    ll->prev_output = *output;
    return 0;
}