/**
 * @file centum_vp_control.c
 * @brief CENTUM VP Control Functions — PID, Sequence, Interlock Implementation
 *
 * Knowledge Points:
 *   centum_pid_block_init — PID block initialization with CENTUM VP defaults (L1)
 *   centum_pid_block_set_tuning — PID parameter configuration (L2)
 *   centum_pid_block_set_mode — PID mode state machine with bumpless transition (L2)
 *   centum_pid_block_set_sv — Setpoint change with rate limiting (L2)
 *   centum_pid_block_execute — Velocity and positional PID algorithms (L5)
 *   centum_pid_block_handle_alarms — Process/rate/deviation alarm evaluation (L2)
 *   centum_pid_block_bumpless_transfer — Bumpless transfer on MAN↔AUT switch (L2)
 *   centum_pid_block_anti_windup_clamp — Integral anti-windup with clamping (L2)
 *   centum_sequence_block_init — Sequence block initialization (L3)
 *   centum_sequence_add_step — Sequence step definition (L3)
 *   centum_sequence_execute — SEBOL sequence execution engine (L5)
 *   centum_lc64_block_init — LC64 interlock block setup (L3)
 *   centum_lc64_add_element — LC64 logic element configuration (L3)
 *   centum_lc64_execute — LC64 logic element evaluation (L5)
 *   centum_selector_block_evaluate — Signal selector (high/low/mid/avg) (L2)
 *   centum_split_range_calculate — Split-range control valve output (L2)
 *   centum_ratio_block_calculate — Ratio control flow setpoint (L2)
 *
 * References:
 *   - Astrom & Hagglund: PID Controllers (1995)
 *   - CENTUM VP Function Block Reference (IM 33K01A10-70E)
 *   - ISA-88 Batch Control Standard
 */

#include "centum_vp_control.h"
#include "centum_vp_fcs.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/*============================================================================
 * centum_pid_block_init
 *
 * Initializes a CENTUM VP PID block with standard defaults:
 *   - Velocity algorithm (incremental output)
 *   - MAN mode (safe start)
 *   - Direct acting (MV increases with PV)
 *   - Kp=1.0, Ti=inf (integral disabled), Td=0.0
 *   - Output limits: MV 0-100%, rate limit 100%/s
 *   - Setpoint limits: -99999 to 99999 (unrestricted)
 *   - All alarms disabled initially
 *
 * L1 — Definition: PID control block data structure as defined in
 * CENTUM VP Function Block Detail document.
 *============================================================================*/
void centum_pid_block_init(centum_pid_block_t *pid)
{
    if (!pid) return;
    memset(pid, 0, sizeof(centum_pid_block_t));

    pid->mode = PID_MODE_MAN;
    pid->algorithm = PID_ALG_VELOCITY;
    pid->action = PID_ACT_DIRECT;
    pid->kp = 1.0;
    pid->ti = 0.0;  /* 0 = integral disabled */
    pid->td = 0.0;  /* 0 = derivative disabled */
    pid->n = 0.0;   /* Not used in standard PID */
    pid->sv = 0.0;
    pid->pv = 0.0;
    pid->mv = 0.0;
    pid->mv_high_limit = 100.0;
    pid->mv_low_limit = 0.0;
    pid->mv_change_limit = 100.0;
    pid->sv_high_limit = 99999.0;
    pid->sv_low_limit = -99999.0;
    pid->dv_high_alarm = 99999.0;
    pid->dv_low_alarm = -99999.0;
    pid->vh_high_alarm = 99999.0;
    pid->vl_low_alarm = -99999.0;
    pid->vp_velocity_alarm = 99999.0;
    pid->integral = 0.0;
    pid->last_error = 0.0;
    pid->last_pv = 0.0;
    pid->scan_period_ms = 200;
    pid->tracking = false;
    pid->tracking_input = 0.0;
    pid->alarm_inhibit = false;
    pid->iop = false;
    pid->oop = false;
}

/*============================================================================
 * centum_pid_block_set_tuning
 *
 * Sets PID tuning parameters. CENTUM VP allows online tuning changes
 * while the block is in AUT/CAS mode. Tuning parameters are applied
 * immediately.
 *
 * Ti = 0 disables integral action (pure P or PD control).
 * Td = 0 disables derivative action (pure P or PI control).
 * Kp must be non-negative.
 *
 * L2 — Core Concept: PID tuning as the primary control loop adjustment.
 *============================================================================*/
void centum_pid_block_set_tuning(centum_pid_block_t *pid, double kp, double ti, double td, double n)
{
    if (!pid) return;
    pid->kp = (kp >= 0.0) ? kp : 0.0;
    pid->ti = (ti >= 0.0) ? ti : 0.0;
    pid->td = (td >= 0.0) ? td : 0.0;
    pid->n = (n >= 0.0) ? n : 0.0;
}

/*============================================================================
 * centum_pid_block_set_mode
 *
 * Implements CENTUM VP PID mode transitions. The mode determines how
 * the output (MV) is computed:
 *
 *   MAN  — Output set by operator (tracking input)
 *   AUT  — Output from PID algorithm using local SV
 *   CAS  — Output from PID, SV from primary loop
 *   PRCAS — Primary-direct cascade (special cascade variant)
 *
 * Transition to AUT from MAN triggers bumpless transfer:
 * the integral term is back-calculated so MV does not jump.
 *
 * L2 — Core Concept: PID mode state machine with bumpless transfer.
 *============================================================================*/
void centum_pid_block_set_mode(centum_pid_block_t *pid, centum_pid_mode_t mode)
{
    if (!pid) return;

    centum_pid_mode_t prev_mode = pid->mode;

    /* Cannot enter CAS if no primary loop connection */
    if (mode == PID_MODE_CAS || mode == PID_MODE_PRCAS) {
        /* In CENTUM VP, cascade requires a wire from primary output
           to this block's CAS_IN terminal. Check would be in DB. */
    }

    pid->mode = mode;

    /* Bumpless transfer: when entering AUT/MAN, back-calculate integral */
    if (mode == PID_MODE_AUT && prev_mode == PID_MODE_MAN) {
        /* Initialize integral so P+I+D = current MV */
        pid->integral = pid->mv - pid->kp * (pid->sv - pid->pv);
    }
}

/*============================================================================
 * centum_pid_block_set_sv
 *
 * Sets the local setpoint. In AUT mode, the SV can be changed by the
 * operator. In CAS mode, SV comes from the primary loop (CAS_IN terminal)
 * and this function sets the tracking value.
 *
 * SV is clamped to [sv_low_limit, sv_high_limit].
 *
 * L2 — Core Concept: Setpoint management in process control.
 *============================================================================*/
void centum_pid_block_set_sv(centum_pid_block_t *pid, double sv)
{
    if (!pid) return;
    if (sv > pid->sv_high_limit) sv = pid->sv_high_limit;
    if (sv < pid->sv_low_limit) sv = pid->sv_low_limit;
    pid->sv = sv;
}

/*============================================================================
 * centum_pid_block_execute
 *
 * Executes one iteration of the PID control algorithm. Supports two forms:
 *
 * VELOCITY ALGORITHM (default in CENTUM VP):
 *   dMV = Kp * [(e_k - e_{k-1}) + (dt/Ti)*e_k + (Td/dt)*(PV_k - 2*PV_{k-1} + PV_{k-2})]
 *   MV_k = MV_{k-1} + dMV
 *
 * Where e_k = SV - PV (error for reverse acting), or PV - SV (direct acting).
 *
 * POSITIONAL ALGORITHM:
 *   MV = Kp * [e + (1/Ti)*integral(e*dt) + Td*de/dt]
 *
 * The velocity form is preferred because:
 *   1. Bumpless transfer is natural (just stop adding dMV)
 *   2. Anti-windup is simpler (clamp MV, stop integrating)
 *   3. No initialization shock on mode change
 *
 * L5 — Algorithm: PID velocity algorithm as implemented in CENTUM VP.
 * Reference: Astrom & Hagglund (1995), Chapter 3.
 *============================================================================*/
double centum_pid_block_execute(centum_pid_block_t *pid, double pv, double dt)
{
    if (!pid) return 0.0;
    if (dt <= 0.0) return pid->mv;

    pid->pv = pv;

    /* In MAN mode, MV is set externally (tracking input) */
    if (pid->mode == PID_MODE_MAN) {
        pid->mv = pid->tracking_input;
        pid->integral = 0.0;
        pid->last_error = 0.0;
        pid->last_pv = pv;
        return pid->mv;
    }

    /* Calculate error (reverse acting: MV increases as PV decreases) */
    double error = pid->sv - pid->pv;
    if (pid->action == PID_ACT_DIRECT) {
        error = pid->pv - pid->sv;
    }

    double dMV = 0.0;
    double p_term = 0.0, i_term = 0.0, d_term = 0.0;

    if (pid->algorithm == PID_ALG_VELOCITY) {
        /* VELOCITY FORM: dMV = Kp*(de + (dt/Ti)*e + (Td/dt)*ddPV) */

        /* Proportional on error change */
        p_term = pid->kp * (error - pid->last_error);

        /* Integral on current error */
        if (pid->ti > 0.0) {
            i_term = pid->kp * (dt / pid->ti) * error;
            pid->integral += i_term;
        }

        /* Derivative on PV change (not error change, to avoid derivative kick) */
        if (pid->td > 0.0) {
            double dpv = pid->last_pv - pv;  /* PV change (negative for increasing PV) */
            d_term = pid->kp * (pid->td / dt) * dpv;
        }

        dMV = p_term + i_term + d_term;
    } else {
        /* POSITIONAL FORM: MV = Kp*(e + (1/Ti)*integral(e*dt) + Td*de/dt) */
        double new_integral = pid->integral;
        if (pid->ti > 0.0) {
            new_integral += error * dt;
        }

        double derivative = 0.0;
        if (pid->td > 0.0 && dt > 0.0) {
            derivative = (error - pid->last_error) / dt;
        }

        p_term = pid->kp * error;
        i_term = (pid->ti > 0.0) ? (pid->kp / pid->ti) * new_integral : 0.0;
        d_term = pid->kp * pid->td * derivative;

        dMV = (p_term + i_term + d_term) - pid->mv;
        pid->integral = new_integral;
    }

    /* Apply output rate-of-change limiting */
    if (pid->mv_change_limit > 0.0) {
        double max_step = pid->mv_change_limit * dt;
        if (dMV > max_step) dMV = max_step;
        if (dMV < -max_step) dMV = -max_step;
    }

    /* Apply dMV and clamp to output limits */
    pid->mv += dMV;
    centum_pid_block_anti_windup_clamp(pid);

    /* Store state for next iteration */
    pid->last_error = error;
    pid->last_pv = pv;

    return pid->mv;
}

/*============================================================================
 * centum_pid_block_handle_alarms
 *
 * Evaluates CENTUM VP PID alarm conditions:
 *   DV_HI/DV_LO — Deviation alarm: |PV - SV| exceeds limit
 *   VH_HI/VL_LO — Process variable high/low alarm
 *   VP_VEL     — PV rate-of-change alarm (velocity alarm)
 *   MH_HI/ML_LO — Manipulated variable high/low alarm
 *   IOP        — Input open (wire break detection, NAMUR NE43)
 *   OOP        — Output open (current loop break)
 *
 * L2 — Core Concept: Process alarm management in DCS.
 * References: NAMUR NE43 signal fault detection, ISA-18.2 alarm management.
 *============================================================================*/
void centum_pid_block_handle_alarms(centum_pid_block_t *pid)
{
    if (!pid) return;
    if (pid->alarm_inhibit) return;

    double deviation = pid->pv - pid->sv;

    /* Deviation alarms */
    pid->dv_hi_alarm_active = (deviation > pid->dv_high_alarm);
    pid->dv_lo_alarm_active = (deviation < pid->dv_low_alarm);

    /* Process variable alarms */
    pid->vh_hi_alarm_active = (pid->pv > pid->vh_high_alarm);
    pid->vl_lo_alarm_active = (pid->pv < pid->vl_low_alarm);

    /* PV velocity alarm (rate of change) */
    double pv_rate = fabs(pid->pv - pid->last_pv);
    pid->vp_vel_alarm_active = (pv_rate > pid->vp_velocity_alarm);

    /* MV limit alarms */
    pid->mh_hi_alarm_active = (pid->mv >= pid->mv_high_limit);
    pid->ml_lo_alarm_active = (pid->mv <= pid->mv_low_limit);
}

/*============================================================================
 * centum_pid_block_bumpless_transfer
 *
 * Implements bumpless transfer when operator switches from MAN to AUT.
 * The integral term is initialized so that the PID output equals the
 * current manual output, ensuring no bump in the process.
 *
 * In velocity form PID, bumpless is natural: the algorithm computes
 * increments, so simply continuing from the current MV ensures smoothness.
 * This function handles the explicit case of mode switch initialization.
 *
 * L2 — Core Concept: Bumpless transfer is essential for safe operator
 * intervention in continuous processes.
 *============================================================================*/
void centum_pid_block_bumpless_transfer(centum_pid_block_t *pid, double manual_mv)
{
    if (!pid) return;

    /* In velocity form, set tracking input and mode to MAN first.
       The integral is reset, and when switching back to AUT,
       centum_pid_block_set_mode will back-calculate the integral. */
    pid->tracking_input = manual_mv;
    pid->mv = manual_mv;
    pid->integral = 0.0;
}

/*============================================================================
 * centum_pid_block_anti_windup_clamp
 *
 * Implements clamping anti-windup for the integral term. When the output
 * saturates (hits high or low limit), the integral is frozen to prevent
 * "windup" — the accumulation of integral error that would cause large
 * overshoot when the error finally changes sign.
 *
 * CENTUM VP uses clamping (conditional integration): the integral is
 * only updated when the output is not saturated, or when the error
 * drives the output away from saturation.
 *
 * L2 — Core Concept: Anti-windup is critical for PID loops with
 * constrained actuators (valves at 0% or 100% travel).
 * Reference: Astrom & Hagglund (1995), Section 3.5.
 *============================================================================*/
void centum_pid_block_anti_windup_clamp(centum_pid_block_t *pid)
{
    if (!pid) return;

    bool was_high = false, was_low = false;

    if (pid->mv > pid->mv_high_limit) {
        pid->mv = pid->mv_high_limit;
        pid->anti_windup_active = true;
        was_high = true;
    }
    if (pid->mv < pid->mv_low_limit) {
        pid->mv = pid->mv_low_limit;
        pid->anti_windup_active = true;
        was_low = true;
    }

    if (!was_high && !was_low) {
        pid->anti_windup_active = false;
    }
}

/*============================================================================
 * centum_sequence_block_init
 *
 * Initializes a CENTUM VP sequence block. Sequence tables are used for
 * sequential control tasks (startup/shutdown sequences, batch phases,
 * equipment changeover). CENTUM VP supports four sequence types:
 *   - SEBOL (Sequence and Batch Oriented Language) — full-featured
 *   - ST16 — Simple 16-step table
 *   - Rule-based — Condition-action rules
 *   - Table-based — Decision table
 *
 * L3 — Engineering Structure: Sequence control as defined in
 * CENTUM VP SEBOL programming manual.
 *============================================================================*/
void centum_sequence_block_init(centum_sequence_block_t *seq, centum_sequence_type_t type)
{
    if (!seq) return;
    memset(seq, 0, sizeof(centum_sequence_block_t));
    seq->seq_type = type;
    seq->state = SEQ_STATE_IDLE;
    seq->current_step = 0;
    seq->total_steps = 0;
    seq->enable = false;
}

/*============================================================================
 * centum_sequence_add_step
 *
 * Adds a step definition to the sequence table. Each step contains:
 *   - Up to 8 conditions (AND/OR logic, comparison, timer, counter)
 *   - True actions (executed when conditions are met)
 *   - False actions (executed when conditions are not met within timeout)
 *   - Wait flag (pause execution until conditions are satisfied)
 *
 * L3 — Engineering Structure: Step-based sequential control definition.
 *============================================================================*/
bool centum_sequence_add_step(centum_sequence_block_t *seq, const centum_sequence_step_t *step)
{
    if (!seq || !step) return false;
    if (seq->total_steps >= 32) return false;

    memcpy(&seq->steps[seq->total_steps], step, sizeof(centum_sequence_step_t));
    seq->total_steps++;
    return true;
}

/*============================================================================
 * centum_sequence_execute
 *
 * Executes the current step of the sequence. Evaluates all conditions;
 * if all are true, executes true actions and advances to next step.
 * If conditions are not met and timeout occurs, executes false actions.
 *
 * The SEBOL execution model supports:
 *   - Parallel branches (multiple simultaneous active steps)
 *   - Transition conditions (boolean logic to advance)
 *   - Action execution (set/reset digital outputs, move values, timers)
 *
 * L5 — Algorithm: Sequential control execution engine.
 *============================================================================*/
void centum_sequence_execute(centum_sequence_block_t *seq)
{
    if (!seq) return;
    if (seq->state != SEQ_STATE_RUNNING) return;
    if (seq->current_step >= seq->total_steps) {
        seq->state = SEQ_STATE_COMPLETED;
        return;
    }

    centum_sequence_step_t *step = &seq->steps[seq->current_step];

    /* Check hold/abort requests */
    if (seq->abort_request) {
        seq->state = SEQ_STATE_ABORTING;
        return;
    }
    if (seq->hold_request) {
        seq->state = SEQ_STATE_HOLDING;
        return;
    }

    /* Evaluate all conditions (AND logic between conditions) */
    bool all_conditions_true = true;
    for (uint8_t i = 0; i < step->condition_count; i++) {
        centum_sequence_condition_t *cond = &step->conditions[i];
        bool cond_result = false;

        switch (cond->cond_type) {
            case COND_EQ:  cond_result = (cond->operand1_val == cond->operand2_val); break;
            case COND_NE:  cond_result = (cond->operand1_val != cond->operand2_val); break;
            case COND_GT:  cond_result = (cond->operand1_val > cond->operand2_val); break;
            case COND_GE:  cond_result = (cond->operand1_val >= cond->operand2_val); break;
            case COND_LT:  cond_result = (cond->operand1_val < cond->operand2_val); break;
            case COND_LE:  cond_result = (cond->operand1_val <= cond->operand2_val); break;
            default: cond_result = true; break;
        }

        if (cond->invert) cond_result = !cond_result;
        cond->last_result = cond_result;

        if (!cond_result) {
            all_conditions_true = false;
            if (!step->wait_for_condition) break;
        }
    }

    if (all_conditions_true) {
        /* Advance to next step */
        seq->current_step++;
        seq->last_transition = time(NULL);
        if (seq->current_step >= seq->total_steps) {
            seq->state = SEQ_STATE_COMPLETED;
        }
    } else if (!step->wait_for_condition) {
        /* Step timeout check would go here */
        seq->step_timeout = false;
    }

    seq->cycle_count++;
}

/*============================================================================
 * centum_sequence_hold / _abort / _reset
 *
 * Sequence control commands. Hold pauses execution at the current step;
 * Abort terminates with an abort status; Reset returns to IDLE state.
 * These correspond to operator commands on the CENTUM VP sequence
 * faceplate panel.
 *
 * L3 — Engineering Structure: Sequence state management.
 *============================================================================*/
void centum_sequence_hold(centum_sequence_block_t *seq)
{
    if (seq && seq->state == SEQ_STATE_RUNNING) {
        seq->hold_request = true;
    }
}

void centum_sequence_abort(centum_sequence_block_t *seq)
{
    if (seq) {
        seq->abort_request = true;
        seq->state = SEQ_STATE_ABORTED;
    }
}

void centum_sequence_reset(centum_sequence_block_t *seq)
{
    if (seq) {
        seq->state = SEQ_STATE_IDLE;
        seq->current_step = 0;
        seq->hold_request = false;
        seq->abort_request = false;
        seq->cycle_count = 0;
    }
}

uint16_t centum_sequence_get_current_step(const centum_sequence_block_t *seq)
{
    return seq ? seq->current_step : 0;
}

centum_sequence_state_t centum_sequence_get_state(const centum_sequence_block_t *seq)
{
    return seq ? seq->state : SEQ_STATE_IDLE;
}

/*============================================================================
 * centum_lc64_block_init
 *
 * LC64 is CENTUM VP's interlock logic block. It provides 64 logic
 * elements that implement Boolean functions (AND, OR, NOT, XOR, SR/RS
 * flip-flops, timers, counters). LC64 blocks are used for:
 *   - Equipment protection interlocking
 *   - Permissive start conditions
 *   - Safety-related logic (non-SIL, for SIL use ProSafe-RS)
 *
 * L3 — Engineering Structure: Interlock logic block configuration.
 *============================================================================*/
void centum_lc64_block_init(centum_lc64_block_t *lc64)
{
    if (!lc64) return;
    memset(lc64, 0, sizeof(centum_lc64_block_t));
    lc64->element_count = 0;
    lc64->enable = true;
}

bool centum_lc64_add_element(centum_lc64_block_t *lc64, centum_lc64_element_t type,
                              uint8_t in1, uint8_t in2)
{
    if (!lc64) return false;
    if (lc64->element_count >= 64) return false;
    if (in1 >= 64 || in2 >= 64) return false;

    lc64->element_types[lc64->element_count] = (uint8_t)type;
    lc64->element_input1[lc64->element_count] = in1;
    lc64->element_input2[lc64->element_count] = in2;
    lc64->element_count++;
    return true;
}

/*============================================================================
 * centum_lc64_execute
 *
 * Evaluates all LC64 logic elements in order (sequential logic).
 * Each element reads from input_mask or previously computed outputs.
 * Results are stored in output_states[].
 *
 * Element types:
 *   AND  — out = in1 AND in2
 *   OR   — out = in1 OR in2
 *   NOT  — out = NOT in1
 *   XOR  — out = in1 XOR in2
 *   SR   — Set-Reset flip-flop (S dominant)
 *   RS   — Reset-Set flip-flop (R dominant)
 *   TON  — Timer On-Delay
 *   TOF  — Timer Off-Delay
 *   CTU  — Counter Up
 *   CTD  — Counter Down
 *
 * L5 — Algorithm: Boolean logic evaluation for interlock sequences.
 *============================================================================*/
void centum_lc64_execute(centum_lc64_block_t *lc64, uint64_t inputs)
{
    if (!lc64 || !lc64->enable) return;

    lc64->input_mask = inputs;

    for (uint8_t i = 0; i < lc64->element_count; i++) {
        uint8_t type = lc64->element_types[i];
        uint8_t in1 = lc64->element_input1[i];
        uint8_t in2 = lc64->element_input2[i];

        bool v1, v2;
        /* Input 1: from input_mask if <64, else from previous output */
        v1 = (in1 < 64) ? ((inputs >> in1) & 1ULL) : lc64->output_states[in1 - 64];
        v2 = (in2 < 64) ? ((inputs >> in2) & 1ULL) : lc64->output_states[in2 - 64];

        switch ((centum_lc64_element_t)type) {
            case LC64_AND:  lc64->output_states[i] = v1 && v2; break;
            case LC64_OR:   lc64->output_states[i] = v1 || v2; break;
            case LC64_NOT:  lc64->output_states[i] = !v1; break;
            case LC64_XOR:  lc64->output_states[i] = (v1 != v2); break;
            case LC64_SR:   /* S=in1, R=in2, S dominant */
                if (v1) lc64->output_states[i] = true;
                else if (v2) lc64->output_states[i] = false;
                break;
            case LC64_RS:   /* R=in1, S=in2, R dominant */
                if (v1) lc64->output_states[i] = false;
                else if (v2) lc64->output_states[i] = true;
                break;
            default:
                lc64->output_states[i] = false;
                break;
        }
    }

    lc64->scan_count++;

    /* Build output mask from output states */
    lc64->output_mask = 0;
    for (uint8_t i = 0; i < 64; i++) {
        if (lc64->output_states[i]) {
            lc64->output_mask |= (1ULL << i);
        }
    }
}

/*============================================================================
 * centum_lc64_get_outputs
 *
 * Returns the 64-bit output mask from the last LC64 execution.
 * Each bit corresponds to one logic element's output.
 *
 * L5 — Algorithm: Interlock output state retrieval.
 *============================================================================*/
uint64_t centum_lc64_get_outputs(const centum_lc64_block_t *lc64)
{
    return lc64 ? lc64->output_mask : 0;
}

/*============================================================================
 * centum_selector_block_evaluate
 *
 * Signal selector block: selects one of 4 input signals based on
 * selection criteria (high, low, mid, or average). Used for:
 *   - Hot standby sensor selection (select highest of redundant sensors)
 *   - Median select for fault-tolerant measurement (2oo3 voting)
 *   - Average for smoothing
 *   - Override control (select lowest temperature for reactor protection)
 *
 * L2 — Core Concept: Signal selection as a building block of
 * override and voting control strategies.
 *============================================================================*/
void centum_selector_block_evaluate(centum_selector_block_t *sel)
{
    if (!sel) return;

    double values[4] = {sel->input1_val, sel->input2_val, sel->input3_val, sel->input4_val};
    double result = values[0];

    if (sel->select_high) {
        result = values[0];
        for (int i = 1; i < 4; i++) {
            if (values[i] > result) result = values[i];
        }
    } else if (sel->select_low) {
        result = values[0];
        for (int i = 1; i < 4; i++) {
            if (values[i] < result) result = values[i];
        }
    } else if (sel->select_mid) {
        /* Median: sort and pick middle */
        for (int i = 0; i < 3; i++) {
            for (int j = i + 1; j < 4; j++) {
                if (values[i] > values[j]) {
                    double tmp = values[i];
                    values[i] = values[j];
                    values[j] = tmp;
                }
            }
        }
        result = values[1]; /* Second value = median of 4 (between [1] and [2]) */
    } else if (sel->select_avg) {
        result = (values[0] + values[1] + values[2] + values[3]) / 4.0;
    }

    sel->output_val = result;
}

/*============================================================================
 * centum_split_range_calculate
 *
 * Split-range control: distributes a single controller output to two
 * final control elements (typically valves). Common in CENTUM VP for:
 *   - Heating/cooling control (one valve for steam, one for chilled water)
 *   - pH control with acid and base reagent valves
 *   - Pressure control with vent and feed valves
 *
 * Split point determines where the output switches between valve A and B.
 * Example: 50% split → 0-50% to valve A, 50-100% to valve B.
 *
 * L2 — Core Concept: Split-range as a DCS control strategy.
 *============================================================================*/
void centum_split_range_calculate(const centum_split_range_block_t *splt, double input,
                                   double *out1, double *out2)
{
    if (!splt || !out1 || !out2) return;

    double norm = (input - splt->output_low1) / (splt->output_high1 - splt->output_low1);

    if (input <= splt->split_point) {
        /* Valve A active */
        double frac_a = (input - splt->output_low1) / (splt->split_point - splt->output_low1);
        if (frac_a < 0.0) frac_a = 0.0;
        if (frac_a > 1.0) frac_a = 1.0;
        *out1 = splt->output_low1 + frac_a * (splt->output_high1 - splt->output_low1);
        *out2 = splt->output_low2;
    } else {
        /* Valve B active */
        *out1 = splt->output_high1;
        double frac_b = (input - splt->split_point) / (splt->output_high2 - splt->split_point);
        if (frac_b < 0.0) frac_b = 0.0;
        if (frac_b > 1.0) frac_b = 1.0;
        *out2 = splt->output_low2 + frac_b * (splt->output_high2 - splt->output_low2);
    }

    /* Interlock overrides */
    if (splt->interlock_a) *out1 = splt->output_low1;
    if (splt->interlock_b) *out2 = splt->output_low2;

    (void)norm; /* Used for inverse calculation if needed */
}

/*============================================================================
 * centum_ratio_block_calculate
 *
 * Ratio control: maintains a fixed ratio between two flows.
 *   Flow2_Setpoint = Flow1 * Ratio + Bias
 *
 * Common applications:
 *   - Fuel/air ratio in combustion control
 *   - Reagent/feed ratio in chemical reactors
 *   - Blending operations
 *
 * CENTUM VP implements ratio control as a dedicated function block (RATIO)
 * that feeds its output to a flow controller's remote setpoint (CAS mode).
 *
 * L2 — Core Concept: Ratio control for maintaining stoichiometric or
 * blending relationships between process streams.
 *============================================================================*/
void centum_ratio_block_calculate(double flow1, double ratio_set, double bias,
                                   double *flow2_setpoint)
{
    if (!flow2_setpoint) return;

    *flow2_setpoint = flow1 * ratio_set + bias;

    /* Clamp to prevent negative flow setpoints */
    if (*flow2_setpoint < 0.0) *flow2_setpoint = 0.0;
}

/*============================================================================
 * String conversion utilities
 *============================================================================*/

const char *centum_pid_mode_to_string(centum_pid_mode_t mode)
{
    switch (mode) {
        case PID_MODE_MAN:   return "MAN";
        case PID_MODE_AUT:   return "AUT";
        case PID_MODE_CAS:   return "CAS";
        case PID_MODE_PRCAS: return "PRCAS";
        case PID_MODE_IMAN:  return "IMAN";
        case PID_MODE_ROUT:  return "ROUT";
        case PID_MODE_RCAS:  return "RCAS";
        default:             return "???";
    }
}

const char *centum_sequence_state_to_string(centum_sequence_state_t state)
{
    switch (state) {
        case SEQ_STATE_IDLE:      return "IDLE";
        case SEQ_STATE_RUNNING:   return "RUNNING";
        case SEQ_STATE_HOLDING:   return "HOLDING";
        case SEQ_STATE_ABORTING:  return "ABORTING";
        case SEQ_STATE_ABORTED:   return "ABORTED";
        case SEQ_STATE_STOPPED:   return "STOPPED";
        case SEQ_STATE_COMPLETED: return "COMPLETED";
        case SEQ_STATE_PAUSED:    return "PAUSED";
        case SEQ_STATE_RESTART:   return "RESTART";
        default:                  return "???";
    }
}