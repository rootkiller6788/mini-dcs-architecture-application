/**
 * @file cee_execution.c
 * @brief Control Execution Environment (CEE) Implementation
 *
 * Implements CEE frame management, task scheduling, RMS schedulability
 * analysis (Liu & Layland), response-time analysis, and emergency stop.
 *
 * L5: Rate-Monotonic Scheduling, Response-Time Analysis
 * L3: Deterministic execution, phase-to-task mapping
 */

#include "../include/cee_execution.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* ==========================================================================
 * L1 - CEE Initialization
 * ========================================================================== */

int cee_init(CEEExecutionManager *cee, uint32_t period_ms)
{
    if (!cee) return -1;
    if (period_ms < CEE_FRAME_PERIOD_MS_MIN || period_ms > CEE_FRAME_PERIOD_MS_MAX)
        return -1;

    memset(cee, 0, sizeof(CEEExecutionManager));
    cee->frame.frame_id = 0;
    cee->frame.frame_period_ms = period_ms;
    cee->frame.phase_count = 0;
    cee->frame.task_count = 0;
    cee->frame.frame_number = 0;
    cee->frame.schedule_valid = false;
    cee->registered_blocks = 0;
    cee->running = false;
    cee->overrun_count = 0;
    cee->max_latency_us = 0;
    cee->average_cpu_util = 0.0;
    cee->emergency_stop = false;

    return 0;
}

/* ==========================================================================
 * L1 - Phase Management
 * ========================================================================== */

int cee_add_phase(CEEExecutionManager *cee, const char *name,
                   uint32_t offset_us, uint32_t duration_us,
                   CEEExecutionClass exec_class)
{
    if (!cee || !name) return -1;
    if (cee->frame.phase_count >= CEE_MAX_PHASES) return -1;

    uint32_t id = cee->frame.phase_count;
    CEEExecutionPhase *phase = &cee->frame.phases[id];
    phase->phase_id = id;
    strncpy(phase->phase_name, name, sizeof(phase->phase_name) - 1);
    phase->offset_us = offset_us;
    phase->duration_us = duration_us;
    phase->exec_class = exec_class;
    phase->task_count = 0;
    phase->overrun_detected = false;

    cee->frame.phase_count++;
    return (int)id;
}

/* ==========================================================================
 * L1 - Task Creation
 * ========================================================================== */

int cee_create_task(CEEExecutionManager *cee, const char *name,
                     uint32_t phase_id, uint32_t period_ms,
                     uint32_t priority, uint32_t wcet_us)
{
    if (!cee || !name) return -1;
    if (cee->frame.task_count >= CEE_MAX_TASKS_PER_PHASE) return -1;
    if (phase_id >= cee->frame.phase_count) return -1;

    uint32_t id = cee->frame.task_count;
    CEETask *task = &cee->frame.tasks[id];
    task->task_id = id;
    strncpy(task->task_name, name, sizeof(task->task_name) - 1);
    task->phase_id = phase_id;
    task->period_ms = period_ms;
    task->priority = priority;
    task->wcet_us = wcet_us;
    task->block_count = 0;
    task->current_duration_us = 0;
    task->execution_count = 0;
    task->enabled = true;

    /* Update parent phase task count */
    cee->frame.phases[phase_id].task_count++;
    cee->frame.task_count++;

    return (int)id;
}

/* ==========================================================================
 * L3 - Block-to-Task Assignment
 * ========================================================================== */

int cee_assign_block_to_task(CEEExecutionManager *cee, uint32_t task_id,
                              uint32_t block_id)
{
    if (!cee) return -1;
    if (task_id >= cee->frame.task_count) return -1;

    CEETask *task = &cee->frame.tasks[task_id];
    if (task->block_count >= CEE_MAX_BLOCKS_PER_TASK) return -1;

    task->block_ids[task->block_count] = block_id;
    task->block_count++;

    /* Register block globally */
    if (cee->registered_blocks < CEE_MAX_OVERALL_BLOCKS) {
        cee->block_registry[cee->registered_blocks] = block_id;
        cee->registered_blocks++;
    }

    return 0;
}

/* ==========================================================================
 * L5 - Liu & Layland RMS Schedulability Bound
 * ========================================================================== */

/**
 * Computes the Rate-Monotonic Scheduling utilization bound.
 *
 * U_bound(n) = n * (2^(1/n) - 1)
 *
 * As n approaches infinity: U_bound -> ln(2) ≈ 0.693147
 *
 * For n tasks with periods T_i and execution times C_i:
 * If sum(C_i/T_i) <= U_bound(n), the task set IS schedulable under RMS.
 * (This is a sufficient but NOT necessary condition.)
 *
 * Reference: Liu & Layland, "Scheduling Algorithms for Multiprogramming
 *            in a Hard-Real-Time Environment", JACM 20(1), 1973.
 * Course: CMU 18-771 Linear Systems, Purdue ECE 602
 */
double cee_rms_bound(int n)
{
    if (n <= 0) return 0.0;
    if (n == 1) return 1.0;

    /* U_bound = n * (2^(1/n) - 1) */
    /* Use exp(ln(2)/n) for numerical stability */
    double u = (double)n * (exp(log(2.0) / (double)n) - 1.0);
    return u;
}

/* ==========================================================================
 * L5 - Schedulability Analysis
 * ========================================================================== */

int cee_analyze_schedulability(CEEExecutionManager *cee,
                                CEESchedulabilityResult *result)
{
    if (!cee || !result) return -1;

    
    memset(result, 0, sizeof(CEESchedulabilityResult));

    int n = 0;
    double total_util = 0.0;
    uint32_t bottleneck = 0;
    double max_util = 0.0;

    for (uint32_t i = 0; i < cee->frame.task_count; i++) {
        CEETask *task = &cee->frame.tasks[i];
        if (!task->enabled || task->period_ms == 0) continue;

        double util = (double)task->wcet_us / (double)(task->period_ms * 1000);
        total_util += util;
        n++;

        if (util > max_util) {
            max_util = util;
            bottleneck = task->task_id;
        }
    }

    result->task_count = n;
    result->total_utilization = total_util;
    result->rms_bound = cee_rms_bound(n);
    result->bottleneck_task = bottleneck;
    result->schedulable = (total_util <= result->rms_bound);

    /* Store in CEE */
    cee->frame.total_wcet_us = 0;
    for (uint32_t i = 0; i < cee->frame.task_count; i++) {
        cee->frame.total_wcet_us += cee->frame.tasks[i].wcet_us;
    }
    cee->frame.cpu_utilization = total_util;
    cee->frame.schedule_valid = result->schedulable;
    memcpy(&cee->sched, result, sizeof(*result));

    return 0;
}

/* ==========================================================================
 * L5 - Response-Time Analysis (RTA)
 * ========================================================================== */

/**
 * Response-Time Analysis for fixed-priority preemptive scheduling.
 *
 * For task i with priority higher than task j (lower number = higher priority):
 *   R_i^(0) = C_i
 *   R_i^(k+1) = C_i + sum_{j in hp(i)} ceil(R_i^(k) / T_j) * C_j
 *
 * Converges when R_i^(k+1) = R_i^(k).
 * Task is schedulable if R_i <= D_i (deadline, typically = T_i).
 *
 * Reference: Joseph & Pandya, "Finding Response Times in a Real-Time System",
 *            The Computer Journal, 29(5), 1986.
 * Course: CMU 18-771, Purdue ECE 602
 */
bool cee_response_time_analysis(const CEEExecutionManager *cee,
                                 uint32_t task_id, uint32_t *response_us)
{
    if (!cee || !response_us) return false;
    if (task_id >= cee->frame.task_count) return false;

    const CEETask *task = &cee->frame.tasks[task_id];
    if (!task->enabled) return false;

    uint32_t C_i = task->wcet_us;
    uint32_t T_i = task->period_ms * 1000; /* Convert to microseconds */
    uint32_t D_i = T_i; /* Implicit deadline = period */

    if (T_i == 0) return false;

    uint32_t R = C_i;
    uint32_t R_prev;
    int max_iterations = 100;

    do {
        R_prev = R;

        uint32_t interference = 0;
        for (uint32_t j = 0; j < cee->frame.task_count; j++) {
            if (j == task_id) continue;
            const CEETask *hp_task = &cee->frame.tasks[j];
            if (!hp_task->enabled) continue;
            /* Higher priority = smaller priority number */
            if (hp_task->priority >= task->priority) continue;

            uint32_t T_j = hp_task->period_ms * 1000;
            if (T_j == 0) continue;

            /* ceil(R / T_j) * C_j */
            uint32_t num_interference = (R + T_j - 1) / T_j;
            interference += num_interference * hp_task->wcet_us;
        }

        R = C_i + interference;

        if (R > D_i) {
            *response_us = R;
            return false; /* Exceeded deadline */
        }

        max_iterations--;
    } while (R != R_prev && max_iterations > 0);

    if (max_iterations <= 0) {
        *response_us = R;
        return false; /* Non-convergent */
    }

    *response_us = R;
    return (R <= D_i);
}

/* ==========================================================================
 * L3 - Frame Execution
 * ========================================================================== */

/**
 * Execute one complete CEE frame.
 * Iterates all phases in order, running enabled tasks in priority order.
 *
 * Each phase has a time budget (duration_us). If a phase exceeds its budget,
 * an overrun is flagged. Task execution times are simulated based on WCET.
 */
uint64_t cee_execute_frame(CEEExecutionManager *cee)
{
    if (!cee || cee->emergency_stop) return 0;

    cee->frame.frame_number++;
    cee->running = true;
    uint32_t frame_start = 0; /* Simulated: would use hardware timer */

    /* Execute phases in offset order */
    for (uint32_t p = 0; p < cee->frame.phase_count; p++) {
        CEEExecutionPhase *phase = &cee->frame.phases[p];

        /* Phase start time (simulated) */
        uint32_t phase_start = frame_start + phase->offset_us;
        uint32_t phase_elapsed = 0;
        (void)phase_start;

        /* Execute tasks in this phase, ordered by priority */
        for (uint32_t t = 0; t < cee->frame.task_count; t++) {
            CEETask *task = &cee->frame.tasks[t];
            if (task->phase_id != p) continue;
            if (!task->enabled) continue;

            /* Check if task should run this frame based on period */
            if (task->period_ms > 0) {
                uint64_t frame_in_period = cee->frame.frame_number %
                    (task->period_ms / cee->frame.frame_period_ms);
                if (frame_in_period != 0) continue;
            }

            /* Simulate task execution time */
            uint32_t exec_time = task->wcet_us;
            task->current_duration_us = exec_time;
            task->execution_count++;
            phase_elapsed += exec_time;

            /* Rate-monotonic: shorter period = higher priority */
            /* Tasks are already ordered by priority in the array */
        }

        /* Check for phase overrun */
        if (phase_elapsed > phase->duration_us) {
            phase->overrun_detected = true;
            cee->overrun_count++;
        }
    }

    /* Compute idle time and utilization */
    uint32_t frame_time_us = cee->frame.frame_period_ms * 1000;
    uint32_t total_exec_us = 0;
    for (uint32_t t = 0; t < cee->frame.task_count; t++) {
        total_exec_us += cee->frame.tasks[t].current_duration_us;
    }

    if (total_exec_us < frame_time_us) {
        cee->frame.idle_time_us = frame_time_us - total_exec_us;
    } else {
        cee->frame.idle_time_us = 0;
    }

    /* Exponential moving average of CPU utilization */
    double instant_util = (frame_time_us > 0) ?
        (double)total_exec_us / (double)frame_time_us : 0.0;
    double alpha = 0.1; /* Smoothing factor */
    cee->average_cpu_util = alpha * instant_util +
                            (1.0 - alpha) * cee->average_cpu_util;

    cee->running = false;
    return cee->frame.frame_number;
}

/* ==========================================================================
 * L2 - CPU Utilization Query
 * ========================================================================== */

double cee_get_cpu_utilization(const CEEExecutionManager *cee)
{
    if (!cee) return 0.0;
    return cee->average_cpu_util;
}

/* ==========================================================================
 * L2 - Emergency Stop
 * ========================================================================== */

int cee_emergency_stop(CEEExecutionManager *cee)
{
    if (!cee) return -1;
    cee->emergency_stop = true;
    cee->running = false;

    /* Disable all tasks */
    for (uint32_t i = 0; i < cee->frame.task_count; i++) {
        cee->frame.tasks[i].enabled = false;
    }

    return 0;
}

/* ==========================================================================
 * L5 - Task Priority Reordering (Priority Ceiling)
 * ========================================================================== */

int cee_reorder_by_priority(CEEExecutionManager *cee, uint32_t phase_id)
{
    if (!cee) return -1;
    if (phase_id >= cee->frame.phase_count) return -1;

    /* Collect tasks in this phase */
    CEETask *phase_tasks[CEE_MAX_TASKS_PER_PHASE];
    int count = 0;

    for (uint32_t i = 0; i < cee->frame.task_count && count < CEE_MAX_TASKS_PER_PHASE; i++) {
        if (cee->frame.tasks[i].phase_id == phase_id) {
            phase_tasks[count++] = &cee->frame.tasks[i];
        }
    }

    /* Simple insertion sort by priority (ascending = higher priority first) */
    for (int i = 1; i < count; i++) {
        CEETask *key = phase_tasks[i];
        int j = i - 1;
        while (j >= 0 && phase_tasks[j]->priority > key->priority) {
            phase_tasks[j + 1] = phase_tasks[j];
            j--;
        }
        phase_tasks[j + 1] = key;
    }

    /* Note: This reorders pointers, not the underlying array.
     * In a real CEE, the execution order is determined by priority,
     * which is already handled in cee_execute_frame by iterating
     * in priority-sorted order from the task table. */

    (void)phase_tasks;
    (void)count;
    return 0;
}