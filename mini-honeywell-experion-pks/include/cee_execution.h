/**
 * @file cee_execution.h
 * @brief Control Execution Environment (CEE) - Scheduling, Phases, and
 *        Deterministic Execution Model for Experion PKS
 *
 * L1: CEE definitions — execution phases, slot timing, task scheduling
 * L2: Deterministic execution, jitter control, overrun detection
 * L3: CEE cycle structure, phase-to-task mapping, priority inversion prevention
 * L4: IEC 61131-3 execution model, real-time constraints
 * L5: Rate-monotonic scheduling analysis, Liu & Layland bound
 *
 * The CEE (Control Execution Environment) is the runtime that executes
 * control blocks inside the C300 controller. It provides a deterministic,
 * cyclic execution framework with guaranteed worst-case execution time.
 *
 * Reference: Honeywell C300 CEE Architecture (EP-CEE-400)
 * Course: MIT 6.302 Feedback Systems, Purdue ECE 602 Lumped Systems
 */

#ifndef CEE_EXECUTION_H
#define CEE_EXECUTION_H

#include "experion_system.h"
#include "control_blocks.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1 - CEE Execution Frame Definitions
 * ========================================================================== */

#define CEE_MAX_PHASES           8
#define CEE_MAX_TASKS_PER_PHASE  128
#define CEE_MAX_BLOCKS_PER_TASK  64
#define CEE_MAX_OVERALL_BLOCKS   2048
#define CEE_FRAME_PERIOD_MS_MAX  2000
#define CEE_FRAME_PERIOD_MS_MIN  10

/** Execution accuracy classes — defines the timing precision
 *  expected for blocks assigned to each class. */
typedef enum {
    CEE_CLASS_CRITICAL    = 0,  /* <1ms jitter, safety-related, ESD */
    CEE_CLASS_FAST        = 1,  /* <5ms jitter, critical control loops */
    CEE_CLASS_REGULATORY  = 2,  /* <50ms jitter, standard PID loops */
    CEE_CLASS_SUPERVISORY = 3,  /* <250ms jitter, APC, optimization */
    CEE_CLASS_BACKGROUND  = 4   /* <1000ms jitter, logging, diagnostics */
} CEEExecutionClass;

/** CEE execution phase — a time-triggered window within a frame
 *  during which a set of tasks/blocks executes. */
typedef struct {
    uint32_t        phase_id;           /* Phase number (0..7) */
    char            phase_name[32];     /* e.g., "INPUT", "REG_CTRL", "SEQ" */
    uint32_t        offset_us;          /* Start offset from frame start (us) */
    uint32_t        duration_us;        /* Maximum allocated duration (us) */
    uint32_t        task_count;         /* Number of tasks in this phase */
    bool            overrun_detected;   /* Overrun flag for this frame */
    CEEExecutionClass exec_class;       /* Timing accuracy class */
} CEEExecutionPhase;

/** CEE task — a group of control blocks sharing the same
 *  execution period and phase. Tasks are the schedulable unit. */
typedef struct {
    uint32_t        task_id;
    char            task_name[32];
    uint32_t        phase_id;           /* Assigned execution phase */
    uint32_t        period_ms;          /* Task period (must divide frame period) */
    uint32_t        priority;           /* Static priority (0=highest) */
    uint32_t        block_count;        /* Number of blocks in this task */
    uint32_t        block_ids[CEE_MAX_BLOCKS_PER_TASK];
    uint32_t        wcet_us;            /* Worst-case execution time (us) */
    uint32_t        current_duration_us;/* Measured duration last execution */
    uint64_t        execution_count;    /* Total invocations */
    bool            enabled;
} CEETask;

/** CEE frame — the outermost execution container.
 *  One frame = one complete cycle of all control logic.
 *  Frame period = LCM of all task periods (typically 50ms-1000ms). */
typedef struct {
    uint32_t        frame_id;
    uint32_t        frame_period_ms;    /* Master frame period */
    uint32_t        phase_count;
    CEEExecutionPhase phases[CEE_MAX_PHASES];
    uint32_t        task_count;
    CEETask         tasks[CEE_MAX_TASKS_PER_PHASE];
    uint64_t        frame_number;       /* Monotonic frame counter */
    uint32_t        total_wcet_us;      /* Sum of all task WCETs */
    uint32_t        idle_time_us;       /* Average idle time per frame */
    double          cpu_utilization;    /* total_wcet / frame_period */
    bool            schedule_valid;     /* Schedulability verified */
} CEEFrame;

/* ==========================================================================
 * L2 - Schedulability Analysis (Core Concept)
 * ========================================================================== */

/**
 * Rate-Monotonic Scheduling (RMS) — Liu & Layland (1973)
 *
 * For n independent, periodic tasks with:
 *   - Period = deadline
 *   - Static priority = 1/period (shorter period = higher priority)
 *   - Preemptive scheduling
 *
 * Sufficient (but not necessary) schedulability test:
 *   U = sum(Ci/Ti) <= n * (2^(1/n) - 1)
 *   where Ci = WCET, Ti = period of task i
 *
 * As n -> infinity: U_bound → ln(2) ≈ 0.693
 */

/** Schedulability test result */
typedef struct {
    bool        schedulable;        /* Overall schedulability verdict */
    double      total_utilization;  /* Sum(Ci/Ti) for all tasks */
    double      rms_bound;          /* n * (2^(1/n) - 1) */
    int         task_count;         /* Number of tasks analyzed */
    uint32_t    bottleneck_task;    /* ID of task causing worst violation */
} CEESchedulabilityResult;

/* ==========================================================================
 * L3 - Execution Frame Manager
 * ========================================================================== */

/** CEE execution manager — the runtime engine that orchestrates
 *  the deterministic execution of all control blocks.
 *  This maps to the C300 Control Builder runtime. */
typedef struct {
    CEEFrame        frame;              /* Active execution frame */
    CEESchedulabilityResult sched;     /* Latest schedulability analysis */
    uint32_t        block_registry[CEE_MAX_OVERALL_BLOCKS]; /* All block IDs */
    uint32_t        registered_blocks;  /* Count of registered blocks */
    bool            running;            /* Frame execution active */
    uint32_t        overrun_count;      /* Total overruns detected */
    uint32_t        max_latency_us;     /* Maximum task start latency */
    double          average_cpu_util;   /* Exponential moving average of CPU util */
    bool            emergency_stop;     /* ESD/emergency condition active */
} CEEExecutionManager;

/* ==========================================================================
 * API - CEE Execution Functions
 * ========================================================================== */

/** Initialize a CEE execution manager with a given frame period.
 *  @param cee        Uninitialized CEE execution manager.
 *  @param period_ms  Frame period in milliseconds.
 *  @return 0 on success. */
int  cee_init(CEEExecutionManager *cee, uint32_t period_ms);

/** Add an execution phase to the frame.
 *  Phases are executed in order of their offset_us.
 *  @param cee        Initialized CEE manager.
 *  @param name       Phase name.
 *  @param offset_us  Start offset from frame beginning.
 *  @param duration_us Maximum duration allocated.
 *  @param exec_class Timing accuracy class.
 *  @return Phase ID on success, -1 on error. */
int  cee_add_phase(CEEExecutionManager *cee, const char *name,
                   uint32_t offset_us, uint32_t duration_us,
                   CEEExecutionClass exec_class);

/** Create a task within a phase.
 *  @param cee       CEE manager.
 *  @param name      Task name.
 *  @param phase_id  Parent phase ID.
 *  @param period_ms Task execution period.
 *  @param priority  Static priority (0=highest).
 *  @param wcet_us   Worst-case execution time estimate.
 *  @return Task ID on success, -1 on error. */
int  cee_create_task(CEEExecutionManager *cee, const char *name,
                     uint32_t phase_id, uint32_t period_ms,
                     uint32_t priority, uint32_t wcet_us);

/** Assign a control block to a task for execution.
 *  @param cee      CEE manager.
 *  @param task_id  Target task ID.
 *  @param block_id Control block ID to assign.
 *  @return 0 on success. */
int  cee_assign_block_to_task(CEEExecutionManager *cee, uint32_t task_id,
                              uint32_t block_id);

/** Perform Liu & Layland RMS schedulability analysis.
 *  Computes: U = sum(Ci/Ti) and compares to n*(2^(1/n)-1).
 *  @param cee    CEE manager with configured tasks.
 *  @param result Output: schedulability analysis result.
 *  @return 0 on success. */
int  cee_analyze_schedulability(CEEExecutionManager *cee,
                                CEESchedulabilityResult *result);

/** Execute one complete frame — all phases in order.
 *  Each phase runs all enabled tasks in priority order.
 *  @param cee Running CEE manager.
 *  @return Frame number after execution. */
uint64_t cee_execute_frame(CEEExecutionManager *cee);

/** Get the current CPU utilization (exponential moving average).
 *  @param cee Running CEE manager.
 *  @return Utilization ratio [0.0, 1.0]. */
double cee_get_cpu_utilization(const CEEExecutionManager *cee);

/** Emergency stop — halt all execution immediately.
 *  All outputs are driven to their fail-safe state.
 *  @param cee Running CEE manager.
 *  @return 0 on success. */
int  cee_emergency_stop(CEEExecutionManager *cee);

/** Compute the Liu & Layland utilization bound for n tasks.
 *  U_bound(n) = n * (2^(1/n) - 1)
 *  @param n Number of tasks.
 *  @return The RMS utilization bound. */
double cee_rms_bound(int n);

/** Response-Time Analysis (RTA) for a single task.
 *  Iterative computation: R_i^(k+1) = C_i + sum_{j<hp(i)} ceil(R_i^k / T_j) * C_j
 *  Converges when R_i^(k+1) = R_i^k or exceeds deadline Di.
 *  @param cee          CEE manager.
 *  @param task_id      Task to analyze.
 *  @param response_us  Output: worst-case response time (us).
 *  @return true if task is schedulable (R <= D). */
bool cee_response_time_analysis(const CEEExecutionManager *cee,
                                uint32_t task_id, uint32_t *response_us);

/** Reorder tasks within a phase by priority (priority ceiling).
 *  Ensures higher-priority tasks run first.
 *  @param cee      CEE manager.
 *  @param phase_id Phase to reorder.
 *  @return 0 on success. */
int  cee_reorder_by_priority(CEEExecutionManager *cee, uint32_t phase_id);

#ifdef __cplusplus
}
#endif

#endif /* CEE_EXECUTION_H */
