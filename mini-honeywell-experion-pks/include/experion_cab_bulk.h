/**
 * @file experion_cab_bulk.h
 * @brief Experion PKS CAB (Custom Algorithm Block) and Bulk Engineering
 *
 * L1: CAB types, bulk build configuration, I/O assignment templates
 * L2: Custom algorithm development, bulk I/O mapping, import/export
 * L3: CAB execution model, data type mapping, parameter passing
 * L5: User-defined algorithms in CEE, CAB lifecycle management
 * L7: Honeywell Control Builder CAB development workflow
 *
 * Reference: Honeywell Custom Algorithm Block Programming Guide (EP-CAB-200)
 * Course: RWTH Aachen Industrial Control, Georgia Tech ECE 6550
 */

#ifndef EXPERION_CAB_BULK_H
#define EXPERION_CAB_BULK_H

#include "experion_system.h"
#include "control_blocks.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1 - CAB Type Definitions
 * ========================================================================== */

#define CAB_MAX_INPUTS      16
#define CAB_MAX_OUTPUTS     8
#define CAB_MAX_PARAMETERS  64
#define CAB_MAX_LOCAL_VARS  32
#define CAB_MAX_CODE_SIZE   4096
#define CAB_NAME_MAX_LEN    32

/** CAB parameter data types. */
typedef enum {
    CAB_PARAM_FLOAT64   = 0,  /* IEEE 754 double precision */
    CAB_PARAM_FLOAT32   = 1,  /* IEEE 754 single precision */
    CAB_PARAM_INT32     = 2,  /* 32-bit signed integer */
    CAB_PARAM_UINT32    = 3,  /* 32-bit unsigned integer */
    CAB_PARAM_BOOL      = 4,  /* Boolean (true/false) */
    CAB_PARAM_ENUM      = 5,  /* Enumerated value (0..N-1) */
    CAB_PARAM_STRING    = 6,  /* Fixed-length string */
    CAB_PARAM_TIMESTAMP = 7   /* Time value (seconds since epoch) */
} CABParameterType;

/** CAB parameter definition. */
typedef struct {
    char            name[32];           /* Parameter name */
    CABParameterType type;              /* Data type */
    union {
        double      f64_val;
        float       f32_val;
        int32_t     i32_val;
        uint32_t    u32_val;
        bool        bool_val;
        int32_t     enum_val;
        char        str_val[40];
        time_t      ts_val;
    } default_value;                    /* Initial value */
    double          min_val;            /* Minimum limit (numeric types) */
    double          max_val;            /* Maximum limit (numeric types) */
    bool            configurable;       /* Can be changed online */
    bool            retain;             /* Retain value across restarts */
    char            description[64];    /* Human-readable description */
    char            eu[10];             /* Engineering units */
} CABParameter;

/** CAB I/O terminal definition.
 *  Inputs and outputs connect CAB to standard control blocks. */
typedef struct {
    char            name[32];
    CABParameterType type;
    double          value;              /* Current value */
    double          prev_value;         /* Value from previous execution */
    ExperionPointQuality quality;       /* Connection quality */
    bool            connected;          /* Wired in Control Builder */
    char            source_tag[24];     /* Source/destination point tag */
} CABTerminal;

/** CAB execution state. */
typedef enum {
    CAB_STATE_IDLE        = 0,
    CAB_STATE_INITIALIZED = 1,
    CAB_STATE_RUNNING     = 2,
    CAB_STATE_ERROR       = 3,
    CAB_STATE_DISABLED    = 4
} CABExecutionState;

/** Custom Algorithm Block - complete definition.
 *  CABs extend the standard C300 function block library with
 *  user-defined algorithms written in a C-like language. */
typedef struct {
    uint32_t        cab_id;
    char            name[CAB_NAME_MAX_LEN];
    char            description[128];
    CABExecutionState state;
    uint32_t        execution_period_ms;
    uint32_t        phase_id;

    /* I/O terminals */
    int             input_count;
    CABTerminal     inputs[CAB_MAX_INPUTS];
    int             output_count;
    CABTerminal     outputs[CAB_MAX_OUTPUTS];

    /* Parameters */
    int             param_count;
    CABParameter    params[CAB_MAX_PARAMETERS];

    /* Local scratchpad */
    int             local_var_count;
    double          local_vars[CAB_MAX_LOCAL_VARS];

    /* Execution tracking */
    uint32_t        execution_count;
    uint32_t        error_count;
    uint32_t        last_error_code;
    uint32_t        max_exec_time_us;
    uint32_t        avg_exec_time_us;
    bool            online_change_pending;
    double          cpu_budget_us;      /* CPU time budget in microseconds */
} CustomAlgorithmBlock;

/* ==========================================================================
 * L3 - CAB Build Configuration (Engineering Structure)
 * ========================================================================== */

/** CAB build target - where the CAB runs. */
typedef enum {
    CAB_TARGET_C300       = 0,  /* C300 Controller native */
    CAB_TARGET_ACE        = 1,  /* Application Control Environment */
    CAB_TARGET_SIMULATION = 2   /* Offline simulation only */
} CABBuildTarget;

/** CAB build configuration. */
typedef struct {
    CABBuildTarget  target;
    uint32_t        optimization_level;  /* 0=debug, 1=size, 2=speed */
    bool            generate_debug_info;
    bool            enable_profiling;
    bool            enable_bounds_check;
    bool            enable_divide_by_zero_check;
    uint32_t        stack_size_bytes;
    uint32_t        heap_size_bytes;
    char            output_path[256];
} CABBuildConfig;

/** CAB build result. */
typedef struct {
    bool            success;
    uint32_t        code_size_bytes;
    uint32_t        data_size_bytes;
    uint32_t        compile_time_ms;
    int             warning_count;
    int             error_count;
    char            error_messages[1024];
    char            output_file[256];
} CABBuildResult;

/* ==========================================================================
 * L3 - Bulk Engineering (I/O Configuration Templates)
 * ========================================================================== */

/** Bulk I/O point template - used for mass configuration.
 *  Experion PKS supports Excel-based bulk engineering where
 *  hundreds of I/O points are configured from a spreadsheet. */
#define BULK_MAX_POINTS_PER_FILE 10000

/** Signal type for bulk engineering. */
typedef enum {
    BULK_SIG_AI_420MA    = 0,  /* 4-20mA analog input */
    BULK_SIG_AI_TC       = 1,  /* Thermocouple input */
    BULK_SIG_AI_RTD      = 2,  /* RTD input */
    BULK_SIG_AO_420MA    = 3,  /* 4-20mA analog output */
    BULK_SIG_DI_24VDC    = 4,  /* 24VDC digital input */
    BULK_SIG_DO_RELAY    = 5,  /* Relay output */
    BULK_SIG_PULSE_IN    = 6,  /* Pulse/frequency input */
    BULK_SIG_HART_AI     = 7,  /* HART analog input */
    BULK_SIG_FOUNDATION_FB = 8 /* Foundation Fieldbus */
} BulkSignalType;

/** Single point in a bulk engineering spreadsheet. */
typedef struct {
    int             row_number;         /* Spreadsheet row */
    char            tag[24];            /* Point tag name */
    char            description[40];    /* Point description */
    BulkSignalType  signal_type;        /* Signal type */
    uint32_t        controller_id;      /* C300 controller assignment */
    uint8_t         slot_number;        /* I/O slot */
    uint8_t         channel_number;     /* I/O channel */
    double          range_lo;           /* EU range low */
    double          range_hi;           /* EU range high */
    char            eu[10];             /* Engineering units */
    double          alarm_lo;           /* Low alarm limit */
    double          alarm_lolo;         /* Low-low alarm limit */
    double          alarm_hi;           /* High alarm limit */
    double          alarm_hihi;         /* High-high alarm limit */
    int32_t         decimal_places;     /* Display precision */
    bool            totalize;           /* Enable totalization */
    bool            trending;           /* Enable historian trending */
    bool            validated;          /* Configuration validated */
    char            error_msg[128];     /* Validation error, if any */
} BulkWiringTable;

/** Bulk engineering workbook. */
typedef struct {
    char            workbook_name[128];
    char            author[64];
    int             point_count;
    BulkWiringTable points[100]; /* Representative sample - real system has 10K+ */
    int             controller_count;
    uint32_t        controller_ids[16];
    int             validation_errors;
    int             validation_warnings;
    bool            ready_for_download;
    time_t          last_modified;
} BulkEngineeringConfig;

/* ==========================================================================
 * L5 - CAB Algorithm Library Functions
 * ========================================================================== */

/** Moving average filter for CAB use.
 *  Implements a sliding window moving average of window_size samples.
 *  y[n] = (1/N) * sum(x[n-i], i=0..N-1)
 *
 *  Commonly used in CABs for signal conditioning before control logic. */
typedef struct {
    double      *buffer;        /* Circular buffer of samples */
    int         window_size;    /* Number of samples in window */
    int         index;          /* Current write position */
    double      sum;            /* Running sum for O(1) update */
    int         count;          /* Samples collected (<= window_size) */
    double      current_avg;    /* Current moving average */
} CABMovingAverage;

/** Polynomial evaluation for CAB nonlinear compensation.
 *  y = a0 + a1*x + a2*x^2 + ... + aN*x^N
 *  Uses Horner's method for numerical stability:
 *  y = a0 + x*(a1 + x*(a2 + ... + x*aN)) */
#define CAB_POLY_MAX_ORDER 10

typedef struct {
    int         order;                      /* Polynomial order (0..10) */
    double      coeffs[CAB_POLY_MAX_ORDER + 1]; /* a0, a1, ..., aN */
    double      x_min;                      /* Input range minimum */
    double      x_max;                      /* Input range maximum */
} CABPolynomial;

/** Deadband / hysteresis block for CABs.
 *  Output follows input only when change exceeds deadband.
 *  Used to prevent chatter from noisy measurements. */
typedef struct {
    double      deadband;       /* Deadband width */
    double      hysteresis;     /* Hysteresis (asymmetric if != 0) */
    double      last_output;    /* Last passed-through value */
    double      last_input;     /* Input at last update */
    bool        rising;         /* Direction of last crossing */
} CABDeadband;

/** Rate limiter for CAB output signals.
 *  Limits the rate of change of a signal to +/- max_rate per second.
 *  Used for valve positioning to prevent water hammer. */
typedef struct {
    double      max_rise_rate;     /* Maximum positive rate (EU/sec) */
    double      max_fall_rate;     /* Maximum negative rate (EU/sec) */
    double      last_output;       /* Last output value */
    double      last_time_sec;     /* Timestamp of last update */
    bool        initialized;
} CABRateLimiter;

/* ==========================================================================
 * L7 - Honeywell-specific CAB Lifecycle
 * ========================================================================== */

/** CAB lifecycle phase per Honeywell development workflow. */
typedef enum {
    CAB_LIFECYCLE_DEVELOPMENT  = 0,  /* In development offline */
    CAB_LIFECYCLE_UNIT_TEST    = 1,  /* Unit testing in simulation */
    CAB_LIFECYCLE_INTEGRATION  = 2,  /* Integration testing with other blocks */
    CAB_LIFECYCLE_FAT          = 3,  /* Factory Acceptance Test */
    CAB_LIFECYCLE_SAT          = 4,  /* Site Acceptance Test */
    CAB_LIFECYCLE_COMMISSIONED = 5,  /* Commissioned and operational */
    CAB_LIFECYCLE_DECOMMISSION = 6   /* Decommissioned / retired */
} CABLifecyclePhase;

/** CAB version management for change control. */
typedef struct {
    uint32_t    major_version;
    uint32_t    minor_version;
    uint32_t    build_number;
    char        change_description[256];
    char        author[64];
    time_t      build_timestamp;
    char        source_checksum[64];     /* SHA-256 of source */
} CABVersion;

/* ==========================================================================
 * API - CAB and Bulk Engineering Functions
 * ========================================================================== */

/* CAB Management */
int  cab_init(CustomAlgorithmBlock *cab, uint32_t id, const char *name);
int  cab_add_input(CustomAlgorithmBlock *cab, const char *name, CABParameterType type);
int  cab_add_output(CustomAlgorithmBlock *cab, const char *name, CABParameterType type);
int  cab_add_parameter(CustomAlgorithmBlock *cab, const char *name, CABParameterType type,
                       double min_val, double max_val, double default_val);
int  cab_build(const CustomAlgorithmBlock *cab, const CABBuildConfig *config,
               CABBuildResult *result);
int  cab_execute(CustomAlgorithmBlock *cab);
int  cab_set_state(CustomAlgorithmBlock *cab, CABExecutionState state);
int  cab_get_execution_stats(const CustomAlgorithmBlock *cab, uint32_t *exec_count,
                             uint32_t *avg_time_us, uint32_t *max_time_us);

/* CAB Algorithm Utilities */
int  cab_moving_average_init(CABMovingAverage *ma, int window_size);
int  cab_moving_average_update(CABMovingAverage *ma, double new_sample, double *avg);
int  cab_moving_average_reset(CABMovingAverage *ma);
int  cab_polynomial_init(CABPolynomial *poly, int order, const double *coeffs);
double cab_polynomial_eval(const CABPolynomial *poly, double x);
int  cab_deadband_init(CABDeadband *db, double deadband, double hysteresis);
double cab_deadband_update(CABDeadband *db, double input);
int  cab_rate_limiter_init(CABRateLimiter *rl, double max_rise, double max_fall);
double cab_rate_limiter_update(CABRateLimiter *rl, double target, double current_time_sec);

/* Bulk Engineering */
int  bulk_config_init(BulkEngineeringConfig *cfg, const char *name, const char *author);
int  bulk_add_point(BulkEngineeringConfig *cfg, const BulkWiringTable *point);
int  bulk_validate(BulkEngineeringConfig *cfg);
int  bulk_export_csv(const BulkEngineeringConfig *cfg, const char *filepath);
int  bulk_get_validation_summary(const BulkEngineeringConfig *cfg, int *errors, int *warnings);
int  bulk_assign_controller(BulkEngineeringConfig *cfg, int point_index, uint32_t controller_id);

/* CAB Version Control */
int  cab_version_init(CABVersion *ver, uint32_t major, uint32_t minor);
int  cab_version_increment(CABVersion *ver, const char *change_desc, const char *author);
int  cab_version_compare(const CABVersion *a, const CABVersion *b);

#ifdef __cplusplus
}
#endif

#endif /* EXPERION_CAB_BULK_H */