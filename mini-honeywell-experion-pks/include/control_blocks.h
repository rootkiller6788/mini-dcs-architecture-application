/**
 * @file control_blocks.h
 * @brief Experion PKS Control Blocks - PID, Cascade, Ratio, Feedforward,
 *        Override Selector, Split-Range, and Regulatory Control Blocks
 *
 * L1: Control block type definitions (PID, RATIO, CASCADE, FF, SPLIT, OVRD)
 * L2: Bumpless transfer, anti-windup, initialization, tracking
 * L3: Block interconnection structures, parameter data model
 * L4: ISA-88 control module model, IEC 61131-3 function blocks
 * L5: PID algorithms (interactive/non-interactive, velocity/positional)
 *
 * Reference: Honeywell Control Builder Reference (EP-CB-600)
 * Course: MIT 2.171 Digital Control, Stanford ENGR205 Process Control
 */

#ifndef CONTROL_BLOCKS_H
#define CONTROL_BLOCKS_H

#include "experion_system.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1 - Control Block Type Definitions
 * ========================================================================== */

/** Experion PKS control block types — the core regulatory control
 *  function blocks available in Control Builder. */
typedef enum {
    CB_REG_PID        = 0x01,  /* Standard PID controller */
    CB_REG_PID_GAP    = 0x02,  /* PID with gap action (deadband on error) */
    CB_REG_PID_PV     = 0x03,  /* PID on PV (derivative on PV, not error) */
    CB_RATIO          = 0x10,  /* Ratio control block */
    CB_CASCADE        = 0x20,  /* Cascade master block */
    CB_FEEDFORWARD    = 0x30,  /* Feedforward summer (static + dynamic) */
    CB_SPLIT_RANGE    = 0x40,  /* Split-range output block */
    CB_OVRD_SELECTOR  = 0x50,  /* Override selector (hi/lo/med select) */
    CB_AUTOMANUAL     = 0x60,  /* Auto/Manual station (A/M bias) */
    CB_SIGNAL_CHAR    = 0x70,  /* Signal characterizer (piecewise linear) */
    CB_LEAD_LAG       = 0x80,  /* Lead-lag dynamic compensator */
    CB_DEADTIME       = 0x90,  /* Deadtime / delay block */
    CB_CALCULATOR     = 0xA0,  /* General calculation block (formula) */
    CB_TOTALIZER      = 0xB0,  /* Flow totalizer / integrator */
    CB_ANALOG_ALARM   = 0xC0,  /* Analog alarm detection */
    CB_DIGITAL_COMP   = 0xD0,  /* Digital composite (boolean logic) */
    CB_TIMER          = 0xE0,  /* On-delay / Off-delay timer */
    CB_COUNTER        = 0xF0   /* Up/Down counter block */
} ControlBlockType;

/* ==========================================================================
 * L1 - PID Algorithm Forms
 * ========================================================================== */

/** PID equation form (ISA standard vs. parallel).
 *  ISA (Standard):  Gc(s) = Kc * (1 + 1/(Ti*s) + Td*s)
 *  Parallel:        Gc(s) = Kp + Ki/s + Kd*s
 *  Interactive:     Gc(s) = Kc * (1 + 1/(Ti*s)) * (1 + Td*s)  */
typedef enum {
    PID_FORM_ISA_STANDARD   = 0,  /* ISA standard form (non-interacting) */
    PID_FORM_PARALLEL       = 1,  /* Parallel (ideal) form */
    PID_FORM_INTERACTIVE    = 2,  /* Interactive (series) form */
    PID_FORM_ISA_STANDARD_D  = 3  /* ISA standard, derivative on PV */
} PIDEquationForm;

/** PID action direction */
typedef enum {
    PID_DIRECT  = 0,  /* Output increases with increasing PV (cooling) */
    PID_REVERSE = 1   /* Output decreases with increasing PV (heating) */
} PIDActionDirection;

/** PID operating mode */
typedef enum {
    PID_MANUAL       = 0,  /* Operator sets output directly */
    PID_AUTO         = 1,  /* Local setpoint control */
    PID_CASCADE      = 2,  /* Remote setpoint from master */
    PID_REMOTE_OUT   = 3,  /* Remote output (from override) */
    PID_INITIALIZE   = 4,  /* Initialization / tracking */
    PID_EMERGENCY    = 5   /* Emergency manual (ESD) */
} PIDMode;

/* ==========================================================================
 * L1 - PID Parameter Structure
 * ========================================================================== */

/** PID tuning parameters — the complete set of parameters
 *  for a single Experion PKS PID control block.
 *
 *  ISA Standard Form (PID_FORM_ISA_STANDARD):
 *    OP(s) = Kc * (1 + 1/(Ti*s) + Td*s) * E(s)
 *
 *  Discretized as velocity form (incremental):
 *    delta_OP = Kc * (delta_e + (Ts/Ti)*e_k + (Td/Ts)*delta2_e)
 *
 *  where delta_e = e_k - e_{k-1}, delta2_e = e_k - 2*e_{k-1} + e_{k-2}
 */
typedef struct {
    /* Tuning constants */
    double      kc;             /* Proportional gain (dimensionless or %/%) */
    double      ti_sec;         /* Integral time in seconds (0 = no integral) */
    double      td_sec;         /* Derivative time in seconds (0 = no derivative) */
    double      ts_sec;         /* Sample/execution period in seconds */

    /* Algorithm configuration */
    PIDEquationForm form;       /* PID equation form */
    PIDActionDirection action;  /* Direct or reverse acting */
    double      sp_hi_limit;    /* Setpoint high limit (EU) */
    double      sp_lo_limit;    /* Setpoint low limit (EU) */
    double      op_hi_limit;    /* Output high limit (%) */
    double      op_lo_limit;    /* Output low limit (%) */
    double      op_rate_limit;  /* Output rate-of-change limit (%/sec, 0=none) */
    double      deadband;       /* Error deadband (0 = disabled) */
    double      gap_width;      /* Gap action width (for PID_GAP only) */

    /* Anti-windup */
    bool        anti_windup_enabled;
    double      windup_limit_hi; /* High clamp for integral term */
    double      windup_limit_lo; /* Low clamp for integral term */

    /* Filtering */
    double      pv_filter_time_sec; /* PV first-order filter time */
    double      d_filter_time_sec;  /* Derivative action filter time */
} PIDParams;

/* ==========================================================================
 * L1 - PID Runtime State
 * ========================================================================== */

/** PID runtime state — dynamic variables updated each execution.
 *  This implements the position/velocity PID state machine. */
typedef struct {
    PIDMode     mode;           /* Current operating mode */
    double      pv;             /* Process variable (EU) */
    double      sp;             /* Setpoint (EU) — local or remote */
    double      remote_sp;      /* Remote setpoint from cascade master */
    double      op;             /* Output value (%) */
    double      track_op;       /* Tracking output (initialization) */
    double      error;          /* Current error = (SP - PV) for reverse */
    double      prev_error;     /* Error at previous execution */
    double      prev2_error;    /* Error two executions ago */
    double      integral_term;  /* Accumulated integral contribution */
    double      derivative_term;/* Current derivative contribution */
    double      p_term;         /* Proportional contribution */
    double      i_term;         /* Integral contribution scaled */
    double      d_term;         /* Derivative contribution scaled */
    double      prev_pv;        /* Previous PV (for derivative-on-PV) */
    double      prev_op;        /* Previous output value */
    bool        initialized;    /* Has block been initialized */
    ExperionPointQuality pv_quality; /* PV quality flag */
} PIDState;

/** Complete PID control block.
 *  This is the primary regulatory control element in Experion PKS. */
typedef struct {
    uint32_t        block_id;       /* Unique block identifier */
    char            tag[24];        /* Block tag name */
    ControlBlockType type;          /* Block type (PID variant) */
    PIDParams       params;         /* Static configuration parameters */
    PIDState        state;          /* Dynamic runtime state */
    bool            enabled;        /* Block enabled / active */
    uint32_t        exec_order;     /* Execution order within CEE phase */
    uint32_t        period_mult;    /* Period multiplier (1=fast, N=every N cycles) */
} PIDControlBlock;

/* ==========================================================================
 * L2 - Cascade Control Structure
 * ========================================================================== */

/** Cascade connection: connects a master PID output to a slave PID
 *  remote setpoint.  Classic example: reactor temperature (master)
 *  manipulating jacket temperature setpoint (slave).
 *
 *  Bumpless transfer requirements:
 *  1. Slave must be in CASCADE mode before master engages
 *  2. On master mode change, slave SP is initialized to current PV
 *  3. On slave failure, master must switch to MANUAL
 */
typedef struct {
    uint32_t        master_block_id; /* Master PID block ID */
    uint32_t        slave_block_id;  /* Slave PID block ID */
    bool            cascade_active;  /* Cascade loop engaged */
    double          sp_ratio;        /* SP scaling factor (default 1.0) */
    double          sp_bias;         /* SP offset after scaling */
    bool            bumpless_on_master_fail; /* Slave to AUTO on master fail */
} CascadePair;

/* ==========================================================================
 * L2 - Feedforward Control Structure
 * ========================================================================== */

/** Feedforward summer block.
 *  Output = PID_output + FF_gain * FF_signal * FF_dynamic_comp
 *
 *  The dynamic compensator is a lead-lag filter:
 *    H(s) = (T_lead * s + 1) / (T_lag * s + 1)
 *
 *  Discretized via bilinear (Tustin) transform:
 *    H(z) = ( (2*T_lead+Ts)*z + (Ts-2*T_lead) ) / ( (2*T_lag+Ts)*z + (Ts-2*T_lag) )
 */
typedef struct {
    double      ff_gain;            /* Static feedforward gain */
    double      ff_bias;            /* Feedforward bias */
    double      lead_time_sec;      /* Lead time constant */
    double      lag_time_sec;       /* Lag time constant */
    double      ff_signal;          /* Current feedforward input */
    double      prev_ff_signal;     /* Previous FF signal (for dynamic comp) */
    double      prev_ff_output;     /* Previous dynamic comp output */
    double      ts_sec;             /* Sample time */
    bool        dynamic_enabled;    /* Enable dynamic compensation */
    bool        static_only;        /* Static gain only (no dynamics) */
} FeedforwardBlock;

/* ==========================================================================
 * L2 - Ratio Control Structure
 * ========================================================================== */

/** Ratio control block.
 *  Maintains a fixed ratio between two flows (or other variables).
 *  Controlled_flow_SP = Ratio * Wild_flow
 *
 *  Common application: fuel-air ratio control in furnaces/boilers.
 *  The "wild" flow is uncontrolled; the "controlled" flow is adjusted
 *  to maintain the specified ratio. */
typedef struct {
    double      ratio;              /* Target ratio (controlled/wild) */
    double      wild_flow;          /* Current wild (uncontrolled) flow */
    double      controlled_sp;      /* Calculated controlled flow SP */
    double      ratio_min;          /* Minimum allowed ratio */
    double      ratio_max;          /* Maximum allowed ratio */
    bool        ratio_clamp;        /* Clamp calculated SP to wild*ratio range */
    double      bias;               /* Ratio bias (SP = ratio*wild + bias) */
} RatioBlock;

/* ==========================================================================
 * L2 - Split-Range Control Structure
 * ========================================================================== */

/** Split-range output block.
 *  Maps a single PID output (0-100%) to two or more final control
 *  elements with different operating ranges.
 *
 *  Common split: 0-50% → heating valve (air-to-open), 50-100% → cooling valve
 *  Output mapping uses piecewise linear interpolation between breakpoints. */
#define SPLIT_MAX_RANGES 4

typedef struct {
    double      pid_input;          /* Input from PID block (0-100%) */
    double      range_start[SPLIT_MAX_RANGES]; /* Start of each output range (%) */
    double      range_end[SPLIT_MAX_RANGES];   /* End of each output range (%) */
    double      output[SPLIT_MAX_RANGES];      /* Scaled output for each range */
    int         active_ranges;      /* Number of active output ranges */
    bool        overlap_allowed;    /* Whether ranges may overlap */
} SplitRangeBlock;

/* ==========================================================================
 * L2 - Override Selector Structure
 * ========================================================================== */

/** Override selector block (high/low/median select).
 *  Selects one of multiple PID outputs based on selection criterion.
 *  Used for constraint control (e.g., compressor surge protection).
 *
 *  Selection types:
 *  - High select: protect against exceeding max limit (e.g., max pressure)
 *  - Low select: protect against falling below min limit (e.g., min flow)
 *  - Median select: select middle value for voting (2oo3) */
typedef enum {
    OVRD_HIGH_SELECT   = 0,  /* Select highest output */
    OVRD_LOW_SELECT    = 1,  /* Select lowest output */
    OVRD_MEDIAN_SELECT = 2   /* Select median output */
} OverrideSelectType;

#define OVRD_MAX_INPUTS 8

typedef struct {
    OverrideSelectType  select_type;
    int                 input_count;
    double              inputs[OVRD_MAX_INPUTS];
    double              selected_output;
    int                 selected_index;
    bool                initialization_required;
} OverrideSelector;

/* ==========================================================================
 * L3 - Signal Characterizer (Piecewise Linear Function)
 * ========================================================================== */

/** Signal characterizer — implements a piecewise linear function
 *  y = f(x) defined by (x_i, y_i) breakpoint pairs.
 *
 *  Uses: sensor linearization, valve characterization, custom SP profiles.
 *  For x between x_i and x_{i+1}:
 *    y = y_i + (y_{i+1} - y_i) * (x - x_i) / (x_{i+1} - x_i) */
#define CHAR_MAX_BREAKPOINTS 21

typedef struct {
    int         num_points;                           /* Number of breakpoints (2..21) */
    double      x[CHAR_MAX_BREAKPOINTS];              /* X-axis breakpoints (must be monotonic) */
    double      y[CHAR_MAX_BREAKPOINTS];              /* Y-axis breakpoints */
    double      output;                               /* Current output */
} SignalCharacterizer;

/* ==========================================================================
 * L2 - Lead-Lag Dynamic Compensator
 * ========================================================================== */

/** Lead-lag block — dynamic compensator for feedforward and
 *  advanced regulatory control.
 *
 *  Continuous transfer function:
 *    H(s) = K * (T_lead * s + 1) / (T_lag * s + 1)
 *
 *  Discretized via Tustin (bilinear) transform for stability at all Ts. */
typedef struct {
    double      gain;           /* Static gain K */
    double      lead_time_sec;  /* Lead time constant */
    double      lag_time_sec;   /* Lag time constant */
    double      ts_sec;         /* Sample time */
    double      input;          /* Current input */
    double      output;         /* Current output */
    double      prev_input;     /* Input at t-1 */
    double      prev_output;    /* Output at t-1 */
    bool        initialized;    /* First-call flag */
} LeadLagBlock;

/* ==========================================================================
 * L5 - Control Execution Order (Algorithm)
 * ========================================================================== */

/** Control execution order within a CEE phase.
 *  Blocks are ordered by: (1) source-sink dependency, (2) user priority.
 *  This implements topological sort of the block connection graph. */
typedef struct {
    uint32_t    block_id;
    uint32_t    order;          /* Execution sequence number */
    uint32_t    upstream_count; /* Number of blocks this depends on */
    uint32_t    upstream_ids[8];/* IDs of upstream (source) blocks */
} BlockExecutionOrder;

/* ==========================================================================
 * API - Control Block Functions
 * ========================================================================== */

/* PID Block */
void pid_params_init(PIDParams *params);
void pid_state_init(PIDState *state);
int  pid_block_init(PIDControlBlock *block, uint32_t id, const char *tag);
int  pid_set_tuning(PIDControlBlock *block, double kc, double ti_sec, double td_sec);
int  pid_set_limits(PIDControlBlock *block, double sp_lo, double sp_hi, double op_lo, double op_hi);
int  pid_set_mode(PIDControlBlock *block, PIDMode mode);
int  pid_execute(PIDControlBlock *block, double pv, double dt_sec, double *output);
int  pid_execute_velocity(PIDControlBlock *block, double pv, double dt_sec, double *delta_op);
int  pid_bumpless_transfer(PIDControlBlock *block, double current_op);
int  pid_get_terms(const PIDControlBlock *block, double *p, double *i, double *d);

/* Cascade */
int  cascade_pair_init(CascadePair *cp, uint32_t master_id, uint32_t slave_id);
int  cascade_engage(CascadePair *cp, bool engage);
int  cascade_calculate_sp(const CascadePair *cp, double master_op, double *slave_sp);

/* Feedforward */
void feedforward_init(FeedforwardBlock *ff, double ff_gain, double lead_sec, double lag_sec, double ts_sec);
int  feedforward_execute(FeedforwardBlock *ff, double ff_signal, double pid_output, double *total_output);

/* Ratio */
int  ratio_block_init(RatioBlock *ratio, double target_ratio);
int  ratio_execute(RatioBlock *ratio, double wild_flow, double *controlled_sp);

/* Split Range */
int  split_range_init(SplitRangeBlock *sr, int num_ranges);
int  split_range_set_breakpoint(SplitRangeBlock *sr, int range_idx, double start_pct, double end_pct);
int  split_range_execute(SplitRangeBlock *sr, double pid_output, double *outputs);

/* Override Selector */
int  override_selector_init(OverrideSelector *os, OverrideSelectType sel_type, int n_inputs);
int  override_selector_set_input(OverrideSelector *os, int idx, double value);
int  override_selector_execute(OverrideSelector *os, double *selected);

/* Signal Characterizer */
int  signal_char_init(SignalCharacterizer *sc);
int  signal_char_add_point(SignalCharacterizer *sc, double x, double y);
int  signal_char_evaluate(const SignalCharacterizer *sc, double x, double *y);

/* Lead-Lag */
int  leadlag_init(LeadLagBlock *ll, double gain, double lead_sec, double lag_sec, double ts_sec);
int  leadlag_execute(LeadLagBlock *ll, double input, double *output);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_BLOCKS_H */
