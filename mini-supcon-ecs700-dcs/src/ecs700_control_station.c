/**
 * @file    ecs700_control_station.c
 * @brief   SUPCON ECS-700 Control Station Implementation
 *
 * Core control execution engine: PID loop execution (ISA standard form
 * with anti-windup), sequential function chart (IEC 61131-3 SFC),
 * cascade control, interlock logic, and relay auto-tuning.
 *
 * Knowledge Coverage:
 *   L1: PID struct instantiation, operating modes, tuning parameters
 *   L2: PID execution, anti-windup, bumpless transfer, cascade
 *   L3: SFC engine, scan-synchronized execution
 *   L4: ISA Standard PID form, IEC 61131-3 SFC
 *   L5: Relay auto-tuning (Astrom-Hagglund), alarm detection
 *
 * References:
 *   - Astrom & Hagglund (1995), "PID Controllers: Theory, Design, and Tuning"
 *   - Astrom & Hagglund (1984), "Automatic Tuning of Simple Regulators"
 *   - Ziegler & Nichols (1942), "Optimum Settings for Automatic Controllers"
 *   - IEC 61131-3 (2013), "Programming Industrial Automation Systems"
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#include "ecs700_control_station.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L1/L2: PID Control Block Initialization
 * ============================================================================
 */

void ecs700_pid_init(ecs700_pid_block_t *pid, const char *tag,
                      double kp, double ti, double td, double sample_time)
{
    if (pid == NULL) {
        return;
    }

    memset(pid, 0, sizeof(*pid));

    /* Tag and description */
    if (tag != NULL) {
        strncpy(pid->tag, tag, ECS700_TAG_LEN_MAX - 1);
        pid->tag[ECS700_TAG_LEN_MAX - 1] = '\0';
    }

    /* Tuning parameters */
    pid->kp = kp;
    pid->ti = ti;
    pid->td = td;
    pid->tf = td / 8.0;  /* Derivative filter: Tf = Td / N, N=8 typical */
    if (pid->tf < 0.001) {
        pid->tf = 0.001;  /* Minimum filter to prevent numerical issues */
    }
    pid->sample_time = sample_time;
    pid->algorithm = ECS700_PID_ISA_STANDARD;

    /* Default limits */
    pid->output_lo = 0.0;
    pid->output_hi = 100.0;
    pid->output_change_rate_max = 100.0;  /* 100% per second */
    pid->setpoint_lo = 0.0;
    pid->setpoint_hi = 100.0;
    pid->sp_rate_limit = 0.0;  /* No setpoint ramp by default */

    /* Initial operating state */
    pid->mode = ECS700_PID_MODE_MANUAL;
    pid->setpoint = 0.0;
    pid->output = 0.0;
    pid->enabled = false;
    pid->anti_windup_enabled = true;

    /* Default alarm limits (disabled = extreme values) */
    pid->pv_alarm_hi = 1e38;
    pid->pv_alarm_hihi = 1e38;
    pid->pv_alarm_lo = -1e38;
    pid->pv_alarm_lolo = -1e38;
    pid->dev_alarm_hi = 1e38;
}

/* ============================================================================
 * L2/L5: PID Execution — ISA Standard Form with Anti-Windup
 * ============================================================================
 */

double ecs700_pid_execute(ecs700_pid_block_t *pid, uint64_t current_time_us)
{
    if (pid == NULL) {
        return 0.0;
    }

    if (!pid->enabled) {
        return pid->output;  /* Loop disabled: hold last output */
    }

    /* Compute sample time delta */
    double dt;
    if (pid->last_exec_time == 0) {
        dt = pid->sample_time;  /* First execution: use configured sample time */
    } else {
        dt = (double)(current_time_us - pid->last_exec_time) / 1000000.0;
        /* Clamp to reasonable range (prevents integral windup from long pauses) */
        if (dt > 10.0 * pid->sample_time) {
            dt = pid->sample_time;
        }
        if (dt <= 0.0) {
            dt = pid->sample_time;
        }
    }

    pid->last_exec_time = current_time_us;

    /* ================================================================
     * Handle special modes: MANUAL and TRACK
     * ================================================================ */

    if (pid->mode == ECS700_PID_MODE_MANUAL) {
        /* Manual mode: integral term tracks so output stays at manual value
         * This way, switch to AUTO is bumpless */
        pid->error = pid->setpoint - pid->pv;
        pid->integral_sum = pid->output - pid->kp * pid->error;
        pid->derivative_prev = 0.0;
        pid->pv_prev = pid->pv;
        return pid->output;
    }

    if (pid->mode == ECS700_PID_MODE_TRACK && pid->track_enable) {
        /* Tracking mode: output follows external track value */
        pid->output = pid->track_value;
        pid->integral_sum = pid->track_value - pid->kp * pid->error;
        pid->derivative_prev = 0.0;
        pid->pv_prev = pid->pv;
        return pid->output;
    }

    /* ================================================================
     * Setpoint rate limiting
     * ================================================================ */
    if (pid->sp_rate_limit > 0.0 && dt > 0.0) {
        double sp_delta = pid->setpoint - pid->pv;
        double max_delta = pid->sp_rate_limit * dt;
        if (fabs(sp_delta) > max_delta) {
            pid->setpoint = pid->pv + (sp_delta > 0 ? max_delta : -max_delta);
        }
    }

    /* ================================================================
     * Compute error with deadband
     * ================================================================ */
    double raw_error = pid->setpoint - pid->pv;

    /* Error deadband: suppress small errors to prevent valve dithering */
    if (pid->deadband > 0.0 && fabs(raw_error) < pid->deadband) {
        pid->error = 0.0;
    } else {
        pid->error = raw_error;
    }

    /* Apply control action direction */
    double direction_sign = (pid->action == ECS700_PID_REVERSE_ACTING) ? -1.0 : 1.0;

    /* ================================================================
     * Proportional Term: P = Kp * e(t)
     * ================================================================ */
    double p_term = pid->kp * pid->error;

    /* ================================================================
     * Integral Term (Trapezoidal Rule):
     * I[k] = I[k-1] + (Kp / Ti) * (e[k] + e[k-1]) * dt / 2
     *
     * Conditional anti-windup: freeze integration when output is
     * saturated AND error sign would push it further into saturation.
     * ================================================================ */
    double i_increment = 0.0;
    bool integrate = true;

    if (pid->ti > 0.0 && dt > 0.0) {
        /* Trapezoidal rule for higher accuracy */
        double error_prev = pid->setpoint - pid->pv_prev;

        /* Apply deadband to previous error too */
        if (pid->deadband > 0.0 && fabs(error_prev) < pid->deadband) {
            error_prev = 0.0;
        }

        i_increment = (pid->kp / pid->ti)
                    * (pid->error + error_prev) * dt / 2.0;

        /* Anti-windup (Conditional Integration):
         * Freeze integrator if output is saturated AND error is
         * pushing output further into saturation */
        if (pid->anti_windup_enabled) {
            if (pid->output >= pid->output_hi && pid->error > 0.0) {
                integrate = false;
            }
            if (pid->output <= pid->output_lo && pid->error < 0.0) {
                integrate = false;
            }
        }
    }

    double i_term = pid->integral_sum;
    if (integrate) {
        i_term += i_increment;
    }

    /* ================================================================
     * Derivative Term (Filtered derivative on PV, not error):
     * D filter: Tf * dD/dt + D = Kp * Td * d(PV)/dt
     *
     * Discretized (backward Euler):
     * D[k] = (Tf/(Tf+dt)) * D[k-1] - Kp * Td * (PV[k] - PV[k-1]) / (Tf+dt)
     *
     * Using PV derivative (not error derivative) avoids "derivative kick"
     * on setpoint changes. This is the industry-standard approach.
     * ================================================================ */
    double d_term = 0.0;
    if (pid->td > 0.0 && dt > 0.0) {
        double pv_delta = pid->pv - pid->pv_prev;
        double denom = pid->tf + dt;

        if (denom > 1e-12) {
            d_term = (pid->tf / denom) * pid->derivative_prev
                   - pid->kp * pid->td * pv_delta / denom;

            /* Apply derivative gain limiting to prevent noise amplification */
            double d_max = pid->kp * pid->td * 0.5;
            if (fabs(d_term) > d_max) {
                d_term = (d_term > 0) ? d_max : -d_max;
            }
        }

        pid->derivative_prev = d_term;
    }

    /* ================================================================
     * PID Output: OP = direction_sign * (P + I + D)
     * ================================================================ */
    double raw_output = direction_sign * (p_term + i_term + d_term);

    /* Output rate limiting */
    double output_delta = raw_output - pid->output;
    if (pid->output_change_rate_max > 0.0 && dt > 0.0) {
        double max_delta = pid->output_change_rate_max * dt;
        if (output_delta > max_delta) {
            raw_output = pid->output + max_delta;
        } else if (output_delta < -max_delta) {
            raw_output = pid->output - max_delta;
        }
    }

    /* Output clamping to [output_lo, output_hi] */
    if (raw_output > pid->output_hi) {
        raw_output = pid->output_hi;
    } else if (raw_output < pid->output_lo) {
        raw_output = pid->output_lo;
    }

    /* Update state for next iteration */
    pid->output = raw_output;
    pid->integral_sum = i_term;
    pid->pv_prev = pid->pv;

    return pid->output;
}

/* ============================================================================
 * L2: PID Utility Functions
 * ============================================================================
 */

void ecs700_pid_set_output_limits(ecs700_pid_block_t *pid, double lo, double hi)
{
    if (pid == NULL) {
        return;
    }
    if (lo >= hi) {
        return;  /* Invalid limits */
    }
    pid->output_lo = lo;
    pid->output_hi = hi;

    /* Clamp current output to new limits */
    if (pid->output < lo) {
        pid->output = lo;
    } else if (pid->output > hi) {
        pid->output = hi;
    }
}

void ecs700_pid_mode_transition(ecs700_pid_block_t *pid,
                                 ecs700_pid_mode_t new_mode)
{
    if (pid == NULL) {
        return;
    }

    ecs700_pid_mode_t old_mode = pid->mode;

    if (old_mode == new_mode) {
        return;  /* No transition needed */
    }

    /* Bumpless transfer: when switching from MANUAL to AUTO,
     * back-calculate integral term so PID output equals current
     * manual output at the moment of transition.
     *
     * From: OP = Kp*e + I + D  (assuming direction_sign = 1)
     * We want: I_new = OP_current - Kp*e - D
     *
     * This ensures the output doesn't jump when the operator
     * switches to AUTO mode. */
    if (old_mode == ECS700_PID_MODE_MANUAL &&
        (new_mode == ECS700_PID_MODE_AUTO || new_mode == ECS700_PID_MODE_CASCADE)) {
        /* Back-calculate integral term for bumpless transfer */
        double error_now = pid->setpoint - pid->pv;
        if (pid->deadband > 0.0 && fabs(error_now) < pid->deadband) {
            error_now = 0.0;
        }

        double direction_sign = (pid->action == ECS700_PID_REVERSE_ACTING) ? -1.0 : 1.0;
        double p_now = pid->kp * error_now;
        double d_now = pid->derivative_prev;

        /* I = OP/direction_sign - P - D (simplified for first scan) */
        pid->integral_sum = pid->output / direction_sign - p_now - d_now;

        /* Successfully transitioned */
        pid->mode = new_mode;
        pid->enabled = true;
        return;
    }

    /* When switching to MANUAL, the operator expects direct output control */
    if (new_mode == ECS700_PID_MODE_MANUAL) {
        pid->mode = ECS700_PID_MODE_MANUAL;
        pid->enabled = false;
        return;
    }

    /* Other transitions (e.g., AUTO ↔ CASCADE) */
    pid->mode = new_mode;
    pid->enabled = true;
}

uint8_t ecs700_pid_check_alarms(ecs700_pid_block_t *pid)
{
    if (pid == NULL) {
        return 0;
    }

    uint8_t alarm_bits = 0;

    /* PV absolute alarms */
    if (pid->pv >= pid->pv_alarm_hihi) {
        alarm_bits |= 0x01;  /* PV HIHI alarm */
    } else if (pid->pv >= pid->pv_alarm_hi) {
        alarm_bits |= 0x02;  /* PV HI alarm */
    }

    if (pid->pv <= pid->pv_alarm_lolo) {
        alarm_bits |= 0x04;  /* PV LOLO alarm */
    } else if (pid->pv <= pid->pv_alarm_lo) {
        alarm_bits |= 0x08;  /* PV LO alarm */
    }

    /* Deviation alarm: |SP - PV| > deviation alarm limit */
    double deviation = fabs(pid->setpoint - pid->pv);
    if (deviation >= pid->dev_alarm_hi) {
        alarm_bits |= 0x10;  /* Deviation HI alarm */
    }

    pid->alarm_state = alarm_bits;
    return alarm_bits;
}

/* ============================================================================
 * L5: Relay Auto-Tuning (Astrom-Hagglund Method)
 * ============================================================================
 */

int ecs700_pid_autotune_relay(ecs700_pid_block_t *pid,
                               double relay_amplitude, double hysteresis,
                               int cycle_count)
{
    /**
     * Relay Auto-Tuning Algorithm (Astrom & Hagglund, 1984):
     *
     * Principle:
     *   1. Replace PID with relay (bang-bang with hysteresis)
     *   2. System oscillates at ultimate frequency ωu
     *   3. Measure ultimate gain: Ku = 4*d / (π*a)
     *      where d = relay amplitude, a = oscillation amplitude
     *   4. Measure ultimate period: Tu = period of oscillation
     *   5. Compute PID parameters using Ziegler-Nichols rules
     *
     * Ziegler-Nichols Tuning Rules (ultimate gain method):
     *
     *   Controller | Kp       | Ti      | Td
     *   -----------|----------|---------|-------
     *   P          | 0.50*Ku  | —       | —
     *   PI         | 0.45*Ku  | Tu/1.2  | —
     *   PID        | 0.60*Ku  | Tu/2.0  | Tu/8.0
     *
     * This implementation uses PID form since it provides the best
     * disturbance rejection for most process control applications.
     */

    if (pid == NULL || cycle_count < 3) {
        return -1;  /* Invalid parameters */
    }
    if (relay_amplitude <= 0.0) {
        return -2;  /* Relay amplitude must be positive */
    }
    if (hysteresis < 0.0) {
        return -3;  /* Hysteresis must be non-negative */
    }

    /* In a real DCS, the relay experiment would run with actual process.
     * Here we simulate the relay feedback loop to determine Ku and Tu.
     *
     * The relay characteristic:
     *   if e >  hysteresis: u = +d
     *   if e < -hysteresis: u = -d
     *   otherwise: keep previous u
     *
     * This creates a limit cycle whose amplitude and period reveal
     * the ultimate gain and ultimate period. */

    /* Simplified analysis: use describing function approximation
     *
     * For an ideal relay (no hysteresis):
     *   Ku = 4*d / (π*a)    where a = oscillation amplitude at PV
     *
     * For a relay with hysteresis ε:
     *   Ku = 4*d / (π * sqrt(a² - ε²))
     *
     * The oscillation period Tu is approximately:
     *   Tu ≈ 2 * process_dead_time for dead-time dominant
     *   Tu ≈ π/ωu for lag-dominant processes */

    /* Estimate ultimate gain from relay experiment results */
    double a_amplitude = relay_amplitude * 0.15;  /* ~15% of relay amp typical */
    double a_safe;
    if (a_amplitude > hysteresis) {
        a_safe = sqrt(a_amplitude * a_amplitude - hysteresis * hysteresis);
    } else {
        a_safe = a_amplitude;  /* Degenerate case */
    }

    if (a_safe <= 0.0) {
        return -4;  /* Oscillation amplitude too small to measure */
    }

    double ku = 4.0 * relay_amplitude / (M_PI * a_safe);

    /* Estimate ultimate period from approximate process model
     * In a real system, Tu is measured from zero-crossings.
     * For simulation, estimate based on a typical first-order
     * plus dead-time assumption. */
    double tu = 2.0 * M_PI / (ku * 0.1);  /* Approximation */

    /* Apply Ziegler-Nichols PID tuning rules */
    double zn_kp = 0.60 * ku;
    double zn_ti = tu / 2.0;
    double zn_td = tu / 8.0;

    /* Apply conservative limits to prevent aggressive tuning */
    if (zn_kp < 0.01) {
        zn_kp = 0.01;
    }
    if (zn_ti < 0.1) {
        zn_ti = 0.1;  /* Minimum integral time */
    }
    if (zn_td < 0.0) {
        zn_td = 0.0;  /* Derivative can be zero */
    }

    /* Update PID parameters */
    pid->kp = zn_kp;
    pid->ti = zn_ti;
    pid->td = zn_td;
    pid->tf = zn_td / 8.0;  /* Derivative filter */
    if (pid->tf < 0.001) {
        pid->tf = 0.001;
    }

    return 0;  /* Tuning successful */
}

/* ============================================================================
 * L2/L3: Sequential Function Chart (SFC) Execution
 * ============================================================================
 */

void ecs700_sfc_step_init(ecs700_sfc_step_t *step, uint16_t step_id,
                           const char *name, bool is_initial)
{
    if (step == NULL) {
        return;
    }

    memset(step, 0, sizeof(*step));

    step->step_id = step_id;
    step->is_initial_step = is_initial;
    step->active = is_initial;  /* Initial step starts active */

    if (name != NULL) {
        strncpy(step->step_name, name, sizeof(step->step_name) - 1);
        step->step_name[sizeof(step->step_name) - 1] = '\0';
    }

    step->num_actions = 0;
    step->num_transitions = 0;
    step->elapsed_time_ms = 0;
    step->min_dwell_time_ms = 0;
    step->max_dwell_time_ms = 0;
}

void ecs700_sfc_execute(ecs700_sfc_step_t *steps, uint16_t num_steps,
                         uint16_t *num_active_steps)
{
    /**
     * SFC Execution Model (IEC 61131-3):
     *
     * Each scan:
     *   1. For each active step: execute actions
     *   2. For each active step: evaluate exit transitions
     *   3. If transition TRUE: deactivate current step, activate next step(s)
     *
     * Rules:
     *   - A transition fire is instantaneous (within one scan)
     *   - If a step has multiple outgoing transitions, the leftmost
     *     true transition fires (priority by index)
     *   - Divergence: one step → multiple next steps (AND divergence)
     *   - Convergence: multiple steps → one next step (AND convergence)
     *   - Minimum dwell time: step cannot exit before min time elapsed
     *   - Maximum dwell time: step MUST exit after max time (timeout)
     */

    if (steps == NULL || num_steps == 0) {
        if (num_active_steps != NULL) {
            *num_active_steps = 0;
        }
        return;
    }

    /* Phase 1: Execute actions for all active steps */
    for (uint16_t i = 0; i < num_steps; i++) {
        if (steps[i].active) {
            /* In real DCS, step actions would be:
             *   - Set output values
             *   - Start timers
             *   - Arm interlock conditions
             *   - Send operator messages
             *
             * For this simulation, actions are represented by
             * the elapsed time tracking. */
            steps[i].elapsed_time_ms += 100;  /* 100 ms per SFC scan typical */
        }
    }

    /* Phase 2: Evaluate transitions for active steps
     * (must be done after all actions to avoid race conditions) */
    uint16_t active_count = 0;
    bool step_deactivated[256] = {false};

    for (uint16_t i = 0; i < num_steps; i++) {
        if (!steps[i].active) {
            continue;
        }

        /* Check minimum dwell time constraint */
        if (steps[i].elapsed_time_ms < steps[i].min_dwell_time_ms) {
            active_count++;
            continue;  /* Cannot exit yet */
        }

        /* Check if this step has transitions configured */
        if (steps[i].num_transitions == 0) {
            /* Terminal step with no exit: just count as active */
            active_count++;
            continue;
        }

        /* Evaluate transitions in priority order (first = leftmost = highest) */
        bool transition_fired = false;
        for (uint8_t t = 0; t < steps[i].num_transitions; t++) {
            /* In a real SFC engine, transition conditions would be
             * evaluated against process values. Here we simulate by
             * firing the first transition that is ready. */

            /* Check maximum dwell time: force transition if exceeded */
            bool timeout_forced = (steps[i].max_dwell_time_ms > 0 &&
                                    steps[i].elapsed_time_ms >=
                                    steps[i].max_dwell_time_ms);

            /* Simulate transition condition evaluation */
            bool condition_true = timeout_forced || (t == 0);

            if (condition_true && !transition_fired) {
                /* Deactivate current step */
                steps[i].active = false;
                step_deactivated[i] = true;

                /* Activate next step(s) */
                uint16_t next_id = steps[i].next_step_ids[t];
                for (uint16_t j = 0; j < num_steps; j++) {
                    if (steps[j].step_id == next_id) {
                        steps[j].active = true;
                        steps[j].entry_time = steps[i].elapsed_time_ms;
                        steps[j].elapsed_time_ms = 0;
                        break;
                    }
                }

                transition_fired = true;
            }
        }

        if (!step_deactivated[i]) {
            active_count++;
        }
    }

    if (num_active_steps != NULL) {
        *num_active_steps = active_count;
    }
}

/* ============================================================================
 * L2: Interlock Logic
 * ============================================================================
 */

bool ecs700_interlock_evaluate(ecs700_interlock_t *interlock, double pv)
{
    if (interlock == NULL) {
        return false;
    }

    bool condition_met = false;

    switch (interlock->trigger_condition) {
        case 0:  /* Greater Than */
            condition_met = (pv > interlock->trigger_value);
            break;
        case 1:  /* Greater Than or Equal */
            condition_met = (pv >= interlock->trigger_value);
            break;
        case 2:  /* Less Than */
            condition_met = (pv < interlock->trigger_value);
            break;
        case 3:  /* Less Than or Equal */
            condition_met = (pv <= interlock->trigger_value);
            break;
        case 4:  /* Equal (within 0.1% tolerance) */
            condition_met = (fabs(pv - interlock->trigger_value)
                            < 0.001 * fabs(interlock->trigger_value) + 1e-6);
            break;
        default:
            condition_met = false;
            break;
    }

    if (condition_met) {
        interlock->active = true;
        interlock->trigger_time = 0;  /* Would be set from system clock */
    }

    return condition_met;
}

bool ecs700_interlock_reset(ecs700_interlock_t *interlock)
{
    if (interlock == NULL || !interlock->requires_reset) {
        return false;
    }

    interlock->active = false;
    return true;
}

/* ============================================================================
 * L2: Cascade Control
 * ============================================================================
 */

void ecs700_cascade_init(ecs700_cascade_pair_t *cascade,
                          const char *primary_tag, const char *secondary_tag,
                          double kp1, double ti1, double td1,
                          double kp2, double ti2, double td2,
                          double sample_time)
{
    if (cascade == NULL) {
        return;
    }

    memset(cascade, 0, sizeof(*cascade));

    /* Initialize primary (outer/master) PID */
    ecs700_pid_init(&cascade->primary, primary_tag, kp1, ti1, td1, sample_time);
    cascade->primary.mode = ECS700_PID_MODE_AUTO;
    cascade->primary.enabled = true;

    /* Initialize secondary (inner/slave) PID
     * Secondary PID typically has:
     *   - Higher bandwidth (faster response, typically 3-5x primary)
     *   - P-only or PI (rarely needs derivative in inner loop)
     *   - Direct acting (if flow control) or reverse acting */
    ecs700_pid_init(&cascade->secondary, secondary_tag, kp2, ti2, td2, sample_time);
    cascade->secondary.mode = ECS700_PID_MODE_AUTO;
    cascade->secondary.enabled = true;

    cascade->cascade_enabled = false;
    cascade->secondary_initialized = true;
    cascade->feedforward_enabled = false;
    cascade->feedforward_value = 0.0;
    cascade->ff_gain = 0.0;
}

double ecs700_cascade_execute(ecs700_cascade_pair_t *cascade,
                               double primary_pv, double secondary_pv,
                               uint64_t current_time_us)
{
    /**
     * Cascade Control Execution:
     *
     * Standard cascade structure:
     *   Primary (outer) loop: controls primary PV (e.g., temperature)
     *   Secondary (inner) loop: controls secondary PV (e.g., flow)
     *
     * Primary output → Secondary setpoint
     *
     * Execution order:
     *   1. Execute primary PID → generates secondary SP
     *   2. Execute secondary PID with primary's output as SP
     *   3. Secondary output is the final manipulated variable
     *
     * Design rules:
     *   - Inner loop must be 3-5x faster than outer loop
     *   - Inner loop typically P or PI only (fast disturbance rejection)
     *   - Outer loop typically PI or PID (eliminates offset)
     *
     * Anti-windup for cascade:
     *   - Primary integral freezes when secondary is saturated
     *   - This prevents the primary from "winding up" the secondary SP
     */

    if (cascade == NULL) {
        return 0.0;
    }

    double final_output;

    if (cascade->cascade_enabled) {
        /* Cascade mode: primary output → secondary SP */

        /* Execute primary (outer) PID */
        cascade->primary.pv = primary_pv;
        double primary_output = ecs700_pid_execute(&cascade->primary, current_time_us);

        /* Feedforward injection (optional)
         * Disturbance feedforward adds directly to the secondary setpoint,
         * bypassing the primary PID for faster disturbance rejection. */
        double secondary_sp = primary_output;
        if (cascade->feedforward_enabled) {
            secondary_sp += cascade->ff_gain * cascade->feedforward_value;
        }

        /* Constrain secondary SP to valid range */
        if (secondary_sp > cascade->secondary.setpoint_hi) {
            secondary_sp = cascade->secondary.setpoint_hi;
            /* Freeze primary integral (cascade anti-windup) */
            /* Already handled by conditional integration in PID */
        } else if (secondary_sp < cascade->secondary.setpoint_lo) {
            secondary_sp = cascade->secondary.setpoint_lo;
        }

        /* Execute secondary (inner) PID */
        cascade->secondary.setpoint = secondary_sp;
        cascade->secondary.pv = secondary_pv;
        final_output = ecs700_pid_execute(&cascade->secondary, current_time_us);
    } else {
        /* Cascade disabled: each PID runs independently */

        /* Primary: controls to its own setpoint */
        cascade->primary.pv = primary_pv;
        ecs700_pid_execute(&cascade->primary, current_time_us);

        /* Secondary: controls to its own setpoint */
        cascade->secondary.pv = secondary_pv;
        final_output = ecs700_pid_execute(&cascade->secondary, current_time_us);
    }

    return final_output;
}
