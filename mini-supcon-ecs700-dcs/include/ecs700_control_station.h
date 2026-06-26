/**
 * @file    ecs700_control_station.h
 * @brief   SUPCON ECS-700 Control Station — Process Controller
 *
 * The Control Station (CS) is the core processing unit in ECS-700.
 * It executes: PID control loops, sequential logic (SFC/GRAFCET),
 * batch control (ISA-88), alarm detection, interlocking, and
 * communication with I/O subsystems via SBUS.
 *
 * Hardware Platform: Dual-core ARM Cortex-A9 @ 800 MHz (typical)
 * Operating System:   VxWorks real-time OS (or Linux with PREEMPT_RT)
 * Memory:             512 MB DDR3 ECC RAM, 4 GB eMMC flash
 *
 * Knowledge Coverage:
 *   L1: CS definitions, scan model, I/O mapping
 *   L2: PID execution cycle, sequential control engine
 *   L3: IEC 61131-3 task model, data flow in CS
 *   L4: IEC 61131-3 programming languages (LD, FBD, ST, SFC, IL)
 *   L5: PID auto-tuning, feedforward, cascade, override
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#ifndef ECS700_CONTROL_STATION_H
#define ECS700_CONTROL_STATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecs700_system_core.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * L1: Core Definitions — Control Station Parameters
 * ============================================================================
 */

/** Maximum control loops per control station (CPU-dependent) */
#define ECS700_CS_MAX_LOOPS             512

/** Maximum sequential function chart (SFC) steps per program */
#define ECS700_SFC_MAX_STEPS            256

/** Maximum simultaneous transitions in one SFC scan */
#define ECS700_SFC_MAX_TRANSITIONS      64

/** Maximum interlock conditions per tag */
#define ECS700_MAX_INTERLOCKS           8

/** Control station CPU usage warning threshold (percent) */
#define ECS700_CS_CPU_WARNING           80.0

/** Control station CPU usage critical threshold (percent) */
#define ECS700_CS_CPU_CRITICAL          95.0

/* ============================================================================
 * L1: Core Data Structures — PID Control Block
 * ============================================================================
 */

/**
 * @brief PID control algorithm selection
 *
 * ECS-700 supports multiple PID variants optimized for different
 * process dynamics. Positional PID is the default for most loops.
 * Velocity PID is preferred for integrating processes.
 */
typedef enum {
    ECS700_PID_POSITIONAL   = 0,  /**< Positional (absolute output) */
    ECS700_PID_VELOCITY     = 1,  /**< Velocity (incremental output Δu) */
    ECS700_PID_PARALLEL     = 2,  /**< Parallel (independent gains) */
    ECS700_PID_ISA_STANDARD = 3   /**< ISA standard form (Kp, Ti, Td) */
} ecs700_pid_algorithm_t;

/**
 * @brief PID control action direction
 */
typedef enum {
    ECS700_PID_DIRECT_ACTING   = 0,  /**< Output increases as PV increases */
    ECS700_PID_REVERSE_ACTING  = 1   /**< Output decreases as PV increases */
} ecs700_pid_action_t;

/**
 * @brief PID controller operating mode
 */
typedef enum {
    ECS700_PID_MODE_MANUAL  = 0,  /**< Manual mode — operator sets output */
    ECS700_PID_MODE_AUTO    = 1,  /**< Automatic — PID controls to setpoint */
    ECS700_PID_MODE_CASCADE = 2,  /**< Cascade — setpoint from primary PID */
    ECS700_PID_MODE_TRACK   = 3   /**< Tracking — output follows external signal */
} ecs700_pid_mode_t;

/**
 * @brief PID control block — the fundamental control element in ECS-700
 *
 * Implements the ISA standard parallel PID algorithm:
 *   OP = Kp * [e(t) + (1/Ti)*∫e(τ)dτ + Td*de/dt]
 *
 * Knowledge Point: DCS PID Implementation — every DCS has a standard
 * PID function block that serves as the building block for regulatory
 * control. The ECS-700 implementation adds anti-windup, bumpless transfer,
 * output limiting, and deviation alarming.
 */
typedef struct {
    /* --- Identification --- */
    char      tag[ECS700_TAG_LEN_MAX];   /**< Loop tag name */
    char      description[128];           /**< Loop description */
    ecs700_pid_algorithm_t algorithm;     /**< PID algorithm selection */
    ecs700_pid_action_t action;           /**< Direct or reverse acting */

    /* --- Tuning Parameters --- */
    double    kp;                         /**< Proportional gain */
    double    ti;                         /**< Integral time (seconds, > 0) */
    double    td;                         /**< Derivative time (seconds, ≥ 0) */
    double    tf;                         /**< Derivative filter time constant (s) */
    double    deadband;                   /**< Error deadband (engineering units) */

    /* --- Limits --- */
    double    output_hi;                  /**< Output high limit */
    double    output_lo;                  /**< Output low limit */
    double    output_change_rate_max;     /**< Maximum output rate of change (/s) */
    double    setpoint_hi;                /**< Setpoint high limit */
    double    setpoint_lo;                /**< Setpoint low limit */
    double    sp_rate_limit;              /**< Setpoint ramp rate (units/s, 0=off) */

    /* --- Operating State --- */
    ecs700_pid_mode_t mode;               /**< Current operating mode */
    double    setpoint;                   /**< Current setpoint (SP) */
    double    pv;                         /**< Process variable (PV) */
    double    output;                     /**< Controller output (OP) */
    double    error;                      /**< Error = SP - PV */
    double    integral_sum;               /**< Accumulated integral term */
    double    derivative_prev;            /**< Previous derivative term */
    double    pv_prev;                    /**< Previous PV for derivative calc */
    double    track_value;                /**< External tracking value */
    bool      track_enable;               /**< Tracking enable flag */
    uint64_t  last_exec_time;             /**< Last execution timestamp μs */
    double    sample_time;                /**< PID sample time (seconds) */
    bool      enabled;                    /**< Loop enabled */
    bool      anti_windup_enabled;        /**< Anti-reset windup protection */
    uint8_t   alarm_state;                /**< Loop alarm bitmask */
    double    pv_alarm_hi;                /**< PV high alarm limit */
    double    pv_alarm_hihi;              /**< PV high-high alarm limit */
    double    pv_alarm_lo;                /**< PV low alarm limit */
    double    pv_alarm_lolo;              /**< PV low-low alarm limit */
    double    dev_alarm_hi;               /**< Deviation high alarm limit */
} ecs700_pid_block_t;

/**
 * @brief Sequential Function Chart (SFC) step
 *
 * Implements IEC 61131-3 SFC execution model. Each step contains:
 *   - Actions executed while step is active
 *   - Transition conditions to move to next step(s)
 *
 * Knowledge Point: IEC 61131-3 SFC — the standard graphical language
 * for sequential control in DCS. Based on GRAFCET (IEC 60848).
 */
typedef struct {
    uint16_t  step_id;                    /**< Unique step identifier */
    char      step_name[48];              /**< Step description */
    bool      is_initial_step;            /**< Initial step flag (double border) */
    uint8_t   num_actions;                /**< Number of actions in this step */
    uint8_t   num_transitions;            /**< Number of exit transitions */
    uint16_t  next_step_ids[8];           /**< Target step IDs after transitions */
    bool      active;                     /**< Current active state */
    uint64_t  elapsed_time_ms;            /**< Time spent in this step */
    uint64_t  min_dwell_time_ms;          /**< Minimum dwell time (0 = none) */
    uint64_t  max_dwell_time_ms;          /**< Maximum dwell time (0 = none) */
    uint64_t  entry_time;                 /**< Timestamp when step became active */
} ecs700_sfc_step_t;

/**
 * @brief Interlock definition
 *
 * Interlocks are safety-related logic conditions that force outputs
 * to safe states when triggered. Common in burner management (BMS),
 * compressor anti-surge, and reactor emergency shutdown.
 *
 * Knowledge Point: DCS Interlocks — permissive logic that prevents
 * unsafe operations. Required by IEC 61511 for SIS/BMS applications.
 */
typedef struct {
    uint16_t  interlock_id;               /**< Interlock unique ID */
    char      cause_tag[ECS700_TAG_LEN_MAX]; /**< Source tag */
    char      effect_tag[ECS700_TAG_LEN_MAX]; /**< Target tag */
    int       trigger_condition;          /**< Condition code (>=, <=, ==, etc.) */
    double    trigger_value;              /**< Threshold value */
    double    safe_output;                /**< Output value when interlocked */
    bool      active;                     /**< Current interlock state */
    uint64_t  trigger_time;               /**< Timestamp of last trigger */
    uint64_t  min_hold_ms;                /**< Minimum hold time after trigger */
    bool      requires_reset;             /**< Manual reset required to clear */
} ecs700_interlock_t;

/* ============================================================================
 * L2: Core Concepts — PID Execution
 * ============================================================================
 */

/**
 * @brief Initialize a PID control block
 *
 * Sets up default tuning parameters, limits, and initializes
 * the internal state for cold start.
 *
 * @param pid        Pointer to PID block
 * @param tag        Loop tag name
 * @param kp         Proportional gain
 * @param ti         Integral time (seconds)
 * @param td         Derivative time (seconds)
 * @param sample_time PID sample time (seconds)
 */
void ecs700_pid_init(ecs700_pid_block_t *pid, const char *tag,
                      double kp, double ti, double td, double sample_time);

/**
 * @brief Execute one PID control iteration
 *
 * The core PID algorithm implementing the ISA standard form.
 * Includes anti-windup (conditional integration), output rate limiting,
 * bumpless manual-auto transfer, and tracking mode support.
 *
 * Algorithm (positional form):
 *   1. Compute error: e = SP - PV
 *   2. Proportional term: P = Kp * e
 *   3. Integral term (trapezoidal): I += (Kp/Ti) * (e + e_prev) * Ts / 2
 *   4. Derivative term (filtered): D = Kp * Td * (PV_prev - PV) / Ts
 *   5. Output: OP = direction_sign * (P + I + D)
 *   6. Anti-windup: freeze I if OP saturates
 *   7. Output rate limiting: clamp ΔOP to max rate
 *
 * Knowledge Point: PID Execution in DCS — the scan-synchronized,
 * sample-and-hold nature of DCS PID distinguishes it from continuous
 * PID. Anti-windup via conditional integration is the industry-standard
 * method used in ECS-700.
 *
 * @param pid            PID block (contains PV and SP)
 * @param current_time_us Current system time in microseconds
 * @return PID output (OP)
 */
double ecs700_pid_execute(ecs700_pid_block_t *pid, uint64_t current_time_us);

/**
 * @brief Perform PID auto-tuning using relay feedback method
 *
 * Implements Astrom-Hagglund relay auto-tuning:
 *   1. Switch controller to relay mode (bang-bang with hysteresis)
 *   2. Detect stable limit cycle oscillation
 *   3. Measure ultimate gain Ku and ultimate period Tu
 *   4. Compute PID parameters from Ziegler-Nichols tuning rules
 *
 * Knowledge Point: Relay Auto-tuning — the industry-standard method
 * for automatic PID tuning in DCS. Uses a relay with hysteresis to
 * induce stable oscillation, then derives Ku and Tu from the limit
 * cycle amplitude and frequency.
 *
 * Reference: Astrom & Hagglund (1984), "Automatic Tuning of Simple
 * Regulators with Specifications on Phase and Amplitude Margins"
 *
 * @param pid            PID block to tune
 * @param relay_amplitude Amplitude of relay output (engineering units)
 * @param hysteresis      Relay hysteresis band
 * @param cycle_count     Number of limit cycles to average
 * @return 0 on success, non-zero on failure
 */
int ecs700_pid_autotune_relay(ecs700_pid_block_t *pid,
                               double relay_amplitude, double hysteresis,
                               int cycle_count);

/**
 * @brief Set PID output limits
 *
 * @param pid  PID block
 * @param lo   Output low limit
 * @param hi   Output high limit
 */
void ecs700_pid_set_output_limits(ecs700_pid_block_t *pid, double lo, double hi);

/**
 * @brief Transition PID between modes with bumpless transfer
 *
 * When switching from MANUAL to AUTO, the integral term is
 * back-calculated so the PID output equals the current manual
 * output, preventing a control bump.
 *
 * Knowledge Point: Bumpless Transfer — the critical feature that
 * prevents process disturbances when operators change control modes.
 * ECS-700 back-calculates the integral sum: I = OP_manual - P - D.
 *
 * @param pid      PID block
 * @param new_mode Target operating mode
 */
void ecs700_pid_mode_transition(ecs700_pid_block_t *pid,
                                 ecs700_pid_mode_t new_mode);

/**
 * @brief Update PID alarm states based on current PV
 *
 * Checks: PV HI, PV HIHI, PV LO, PV LOLO, Deviation HI
 * Sets alarm bits in pid->alarm_state accordingly.
 *
 * @param pid  PID block
 * @return Alarm bitmask
 */
uint8_t ecs700_pid_check_alarms(ecs700_pid_block_t *pid);

/* ============================================================================
 * L2: Core Concepts — Sequential Control
 * ============================================================================
 */

/**
 * @brief Initialize an SFC step
 *
 * @param step       SFC step structure
 * @param step_id    Unique step ID
 * @param name       Step description
 * @param is_initial Whether this is the initial step
 */
void ecs700_sfc_step_init(ecs700_sfc_step_t *step, uint16_t step_id,
                           const char *name, bool is_initial);

/**
 * @brief Execute one scan of an SFC (Sequential Function Chart)
 *
 * For each active step:
 *   1. Execute step actions
 *   2. Evaluate all exit transitions
 *   3. If transition is true: deactivate step, activate next step(s)
 *
 * Knowledge Point: SFC Execution Engine — the IEC 61131-3 sequential
 * control model used for batch processes, startup/shutdown sequences,
 * and phase logic. Each SFC scan is synchronized with the CS scan cycle.
 *
 * @param steps            Array of SFC steps
 * @param num_steps        Number of steps
 * @param num_active_steps Output: number of currently active steps
 */
void ecs700_sfc_execute(ecs700_sfc_step_t *steps, uint16_t num_steps,
                         uint16_t *num_active_steps);

/**
 * @brief Evaluate an interlock condition
 *
 * @param interlock Interlock definition
 * @param pv        Current process variable value
 * @return true if interlock is triggered
 */
bool ecs700_interlock_evaluate(ecs700_interlock_t *interlock, double pv);

/**
 * @brief Reset a latched interlock (manual reset required)
 *
 * @param interlock Interlock to reset
 * @return true if reset was successful
 */
bool ecs700_interlock_reset(ecs700_interlock_t *interlock);

/* ============================================================================
 * L2: Core Concepts — Cascade Control
 * ============================================================================
 */

/**
 * @brief Cascade control pair: primary (outer) + secondary (inner)
 *
 * Cascade control uses two PID controllers where the primary controller's
 * output becomes the secondary controller's setpoint. This structure
 * rejects disturbances in the inner loop before they affect the primary PV.
 *
 * Knowledge Point: Cascade Control — the most common advanced regulatory
 * control strategy. Inner loop must be 3-5x faster than outer loop.
 */
typedef struct {
    ecs700_pid_block_t primary;           /**< Primary (outer/master) PID */
    ecs700_pid_block_t secondary;         /**< Secondary (inner/slave) PID */
    bool      cascade_enabled;            /**< Whether cascade is active */
    bool      secondary_initialized;      /**< Secondary tuning complete */
    double    feedforward_value;          /**< Optional feedforward signal */
    bool      feedforward_enabled;        /**< Feedforward enable */
    double    ff_gain;                    /**< Feedforward gain */
} ecs700_cascade_pair_t;

/**
 * @brief Initialize a cascade control pair
 *
 * @param cascade      Cascade pair structure
 * @param primary_tag  Primary PID tag
 * @param secondary_tag Secondary PID tag
 * @param kp1, ti1, td1 Primary tuning
 * @param kp2, ti2, td2 Secondary tuning
 * @param sample_time  PID sample time
 */
void ecs700_cascade_init(ecs700_cascade_pair_t *cascade,
                          const char *primary_tag, const char *secondary_tag,
                          double kp1, double ti1, double td1,
                          double kp2, double ti2, double td2,
                          double sample_time);

/**
 * @brief Execute cascade control pair
 *
 * Execution order: inner loop first (higher frequency if different rates),
 * then outer loop. If cascade disabled, each runs independently.
 *
 * @param cascade        Cascade pair
 * @param primary_pv     Primary process variable
 * @param secondary_pv   Secondary process variable
 * @param current_time_us System time
 * @return Final output (secondary PID output)
 */
double ecs700_cascade_execute(ecs700_cascade_pair_t *cascade,
                               double primary_pv, double secondary_pv,
                               uint64_t current_time_us);

#ifdef __cplusplus
}
#endif

#endif /* ECS700_CONTROL_STATION_H */
