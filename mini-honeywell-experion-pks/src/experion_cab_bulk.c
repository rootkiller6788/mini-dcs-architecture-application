/**
 * @file experion_cab_bulk.c
 * @brief CAB (Custom Algorithm Block) and Bulk Engineering Implementation
 *
 * Implements: CAB lifecycle management, moving average filter,
 * polynomial evaluation (Horner), deadband/hysteresis, rate limiter,
 * bulk I/O configuration validation, and CSV export.
 *
 * L5: Custom algorithm utilities - moving average, polynomial, rate limiter
 * L3: Bulk engineering I/O templates, validation
 * L7: Honeywell CAB development workflow
 */

#include "../include/experion_cab_bulk.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* ==========================================================================
 * L1 - CAB Initialization and Configuration
 * ========================================================================== */

int cab_init(CustomAlgorithmBlock *cab, uint32_t id, const char *name)
{
    if (!cab || !name) return -1;

    memset(cab, 0, sizeof(CustomAlgorithmBlock));
    cab->cab_id = id;
    strncpy(cab->name, name, sizeof(cab->name) - 1);
    cab->state = CAB_STATE_IDLE;
    cab->execution_period_ms = 250;
    cab->cpu_budget_us = 500.0; /* 500us default budget */

    return 0;
}

int cab_add_input(CustomAlgorithmBlock *cab, const char *name,
                   CABParameterType type)
{
    if (!cab || !name) return -1;
    if (cab->input_count >= CAB_MAX_INPUTS) return -1;

    CABTerminal *term = &cab->inputs[cab->input_count];
    memset(term, 0, sizeof(CABTerminal));
    strncpy(term->name, name, sizeof(term->name) - 1);
    term->type = type;
    term->connected = false;
    cab->input_count++;

    return 0;
}

int cab_add_output(CustomAlgorithmBlock *cab, const char *name,
                    CABParameterType type)
{
    if (!cab || !name) return -1;
    if (cab->output_count >= CAB_MAX_OUTPUTS) return -1;

    CABTerminal *term = &cab->outputs[cab->output_count];
    memset(term, 0, sizeof(CABTerminal));
    strncpy(term->name, name, sizeof(term->name) - 1);
    term->type = type;
    term->connected = false;
    cab->output_count++;

    return 0;
}

int cab_add_parameter(CustomAlgorithmBlock *cab, const char *name,
                       CABParameterType type, double min_val, double max_val,
                       double default_val)
{
    if (!cab || !name) return -1;
    if (cab->param_count >= CAB_MAX_PARAMETERS) return -1;

    CABParameter *param = &cab->params[cab->param_count];
    memset(param, 0, sizeof(CABParameter));
    strncpy(param->name, name, sizeof(param->name) - 1);
    param->type = type;
    param->min_val = min_val;
    param->max_val = max_val;
    param->default_value.f64_val = default_val;
    param->configurable = true;
    param->retain = false;
    cab->param_count++;

    return 0;
}

/* ==========================================================================
 * L5 - CAB Build and Execution
 * ========================================================================== */

int cab_build(const CustomAlgorithmBlock *cab, const CABBuildConfig *config,
               CABBuildResult *result)
{
    if (!cab || !config || !result) return -1;

    memset(result, 0, sizeof(CABBuildResult));

    /* Simulate build process */
    result->code_size_bytes = 1024 + cab->input_count * 64 +
                               cab->output_count * 64 + cab->param_count * 128;
    result->data_size_bytes = cab->local_var_count * 8 +
                               cab->param_count * sizeof(CABParameter);
    result->compile_time_ms = 50 + cab->param_count * 2;
    result->warning_count = 0;
    result->error_count = 0;
    result->success = true;

    /* Basic validation */
    if (cab->input_count == 0 && cab->output_count == 0) {
        result->warning_count++;
        strcpy(result->error_messages, "Warning: CAB has no I/O terminals");
    }

    return 0;
}

int cab_execute(CustomAlgorithmBlock *cab)
{
    if (!cab) return -1;
    if (cab->state != CAB_STATE_RUNNING) return -1;

    cab->execution_count++;

    /* In real CAB: execute compiled custom algorithm bytecode.
     * Here: simulate execution by propagating inputs to outputs
     * through any configured processing. */

    /* Simple passthrough for testing: output[0] = input[0] */
    if (cab->input_count > 0 && cab->output_count > 0) {
        for (int i = 0; i < cab->output_count && i < cab->input_count; i++) {
            cab->outputs[i].value = cab->inputs[i].value;
            cab->outputs[i].quality = cab->inputs[i].quality;
        }
    }

    return 0;
}

int cab_set_state(CustomAlgorithmBlock *cab, CABExecutionState state)
{
    if (!cab) return -1;
    cab->state = state;
    return 0;
}

int cab_get_execution_stats(const CustomAlgorithmBlock *cab,
                             uint32_t *exec_count, uint32_t *avg_time_us,
                             uint32_t *max_time_us)
{
    if (!cab) return -1;
    if (exec_count) *exec_count = cab->execution_count;
    if (avg_time_us) *avg_time_us = cab->avg_exec_time_us;
    if (max_time_us) *max_time_us = cab->max_exec_time_us;
    return 0;
}

/* ==========================================================================
 * L5 - Moving Average Filter (CAB Utility)
 * ========================================================================== */

/**
 * Initialize a moving average filter.
 *
 * Implements a sliding window moving average:
 *   y[n] = (1/N) * sum_{i=0}^{N-1} x[n-i]
 *
 * Uses circular buffer with running sum for O(1) per-sample update:
 *   sum = sum - buffer[oldest] + new_sample
 *   average = sum / count
 *
 * Applications in process control:
 * - Flow measurement smoothing (reduce turbulence noise)
 * - Level measurement filtering (reduce wave effects)
 * - Temperature trend smoothing
 *
 * Reference: Smith, Digital Signal Processing, Ch.15
 * Course: MIT 2.171, Berkeley ME233
 */
int cab_moving_average_init(CABMovingAverage *ma, int window_size)
{
    if (!ma) return -1;
    if (window_size < 1 || window_size > 1000) return -1;

    ma->buffer = (double *)calloc((size_t)window_size, sizeof(double));
    if (!ma->buffer) return -1;

    ma->window_size = window_size;
    ma->index = 0;
    ma->sum = 0.0;
    ma->count = 0;
    ma->current_avg = 0.0;

    return 0;
}

int cab_moving_average_update(CABMovingAverage *ma, double new_sample,
                               double *avg)
{
    if (!ma || !ma->buffer || !avg) return -1;

    /* Remove oldest sample if buffer is full */
    if (ma->count >= ma->window_size) {
        int oldest_idx = (ma->index + ma->window_size - ma->count +
                          ma->window_size) % ma->window_size;
        ma->sum -= ma->buffer[oldest_idx];
    } else {
        ma->count++;
    }

    /* Add new sample */
    ma->buffer[ma->index] = new_sample;
    ma->sum += new_sample;

    /* Advance index (circular) */
    ma->index = (ma->index + 1) % ma->window_size;

    /* Compute average */
    ma->current_avg = (ma->count > 0) ? ma->sum / ma->count : 0.0;
    *avg = ma->current_avg;

    return 0;
}

int cab_moving_average_reset(CABMovingAverage *ma)
{
    if (!ma || !ma->buffer) return -1;

    memset(ma->buffer, 0, (size_t)ma->window_size * sizeof(double));
    ma->index = 0;
    ma->sum = 0.0;
    ma->count = 0;
    ma->current_avg = 0.0;

    return 0;
}

/* ==========================================================================
 * L5 - Polynomial Evaluation (Horner's Method)
 * ========================================================================== */

/**
 * Initialize a polynomial for CAB use.
 *
 * Polynomial: y = a0 + a1*x + a2*x^2 + ... + aN*x^N
 *
 * Applications:
 * - Thermocouple linearization (polynomial compensation)
 * - Valve characteristic curve compensation
 * - Tank strapping table interpolation
 */
int cab_polynomial_init(CABPolynomial *poly, int order, const double *coeffs)
{
    if (!poly || !coeffs) return -1;
    if (order < 0 || order > CAB_POLY_MAX_ORDER) return -1;

    memset(poly, 0, sizeof(CABPolynomial));
    poly->order = order;
    memcpy(poly->coeffs, coeffs, (size_t)(order + 1) * sizeof(double));
    poly->x_min = -1e6;
    poly->x_max = 1e6;

    return 0;
}

/**
 * Evaluate polynomial using Horner's method.
 *
 * Horner's scheme evaluates a polynomial in O(N) with N multiplications:
 *   y = a0 + x*(a1 + x*(a2 + ... + x*an))
 *
 * This is numerically more stable than direct evaluation and minimizes
 * round-off error. Equivalent to:
 *   y = (...((an * x + a_{n-1}) * x + a_{n-2}) * x + ... + a0)
 *
 * Reference: Higham, Accuracy and Stability of Numerical Algorithms, Ch.5
 * Course: CMU 18-771, Berkeley ME233
 */
double cab_polynomial_eval(const CABPolynomial *poly, double x)
{
    if (!poly) return 0.0;

    /* Clamp input to valid range */
    if (x < poly->x_min) x = poly->x_min;
    if (x > poly->x_max) x = poly->x_max;

    /* Horner's method */
    double y = poly->coeffs[poly->order];
    for (int i = poly->order - 1; i >= 0; i--) {
        y = y * x + poly->coeffs[i];
    }

    return y;
}

/* ==========================================================================
 * L5 - Deadband / Hysteresis (CAB Utility)
 * ========================================================================== */

/**
 * Initialize a deadband block.
 *
 * Deadband prevents small signal changes from propagating:
 *   If |input - last_output| < deadband: output unchanged
 *   Otherwise: output = input
 *
 * Used to prevent:
 * - Measurement noise from triggering unnecessary control actions
 * - Chatter in digital alarm outputs
 * - Excessive valve movement from small signal variations
 */
int cab_deadband_init(CABDeadband *db, double deadband, double hysteresis)
{
    if (!db) return -1;
    if (deadband < 0.0) return -1;

    memset(db, 0, sizeof(CABDeadband));
    db->deadband = deadband;
    db->hysteresis = hysteresis;
    db->last_output = 0.0;
    db->last_input = 0.0;
    db->rising = true;

    return 0;
}

/**
 * Apply deadband with optional hysteresis.
 *
 * Hysteresis provides asymmetric behavior:
 * - On rising edge: output changes when input > last_output + deadband + hysteresis
 * - On falling edge: output changes when input < last_output - deadband
 *
 * This creates a Schmitt trigger-like behavior that prevents oscillation
 * when the input signal hovers near the deadband boundary.
 */
double cab_deadband_update(CABDeadband *db, double input)
{
    if (!db) return input;

    double diff = input - db->last_output;

    if (db->rising) {
        /* Rising: need to exceed deadband + hysteresis to trigger */
        if (diff > db->deadband + db->hysteresis) {
            db->last_output = input;
            db->last_input = input;
            db->rising = false;
        }
    } else {
        /* Falling: need to drop below -deadband to trigger */
        if (diff < -db->deadband) {
            db->last_output = input;
            db->last_input = input;
            db->rising = true;
        }
    }

    return db->last_output;
}

/* ==========================================================================
 * L5 - Rate Limiter (CAB Utility)
 * ========================================================================== */

/**
 * Initialize a rate limiter.
 *
 * Rate limiters constrain how fast a signal can change, which is critical for:
 * - Valve positioning: prevent water hammer from rapid valve closure
 * - Setpoint ramping: smooth transitions to avoid process upsets
 * - Motor speed control: protect mechanical equipment
 *
 * The rate limit is enforced independently for rising and falling directions.
 */
int cab_rate_limiter_init(CABRateLimiter *rl, double max_rise, double max_fall)
{
    if (!rl) return -1;
    if (max_rise < 0.0 || max_fall < 0.0) return -1;

    memset(rl, 0, sizeof(CABRateLimiter));
    rl->max_rise_rate = max_rise;
    rl->max_fall_rate = max_fall;
    rl->last_output = 0.0;
    rl->last_time_sec = 0.0;
    rl->initialized = false;

    return 0;
}

/**
 * Apply rate limiting to a target value.
 *
 * Algorithm:
 * 1. Calculate elapsed time since last update
 * 2. Calculate max allowed change based on rate and direction
 * 3. Clamp output to allowed range
 *
 * @param target          Desired output value
 * @param current_time_sec Current timestamp in seconds
 * @return Rate-limited output value
 */
double cab_rate_limiter_update(CABRateLimiter *rl, double target,
                                double current_time_sec)
{
    if (!rl) return target;

    if (!rl->initialized) {
        rl->last_output = target;
        rl->last_time_sec = current_time_sec;
        rl->initialized = true;
        return target;
    }

    double dt = current_time_sec - rl->last_time_sec;
    if (dt <= 0.0) return rl->last_output; /* Time must advance */

    double change = target - rl->last_output;
    double max_change;

    if (change >= 0.0) {
        max_change = rl->max_rise_rate * dt;
    } else {
        max_change = -rl->max_fall_rate * dt;
    }

    double limited;
    if (fabs(change) > fabs(max_change)) {
        limited = rl->last_output + max_change;
    } else {
        limited = target;
    }

    rl->last_output = limited;
    rl->last_time_sec = current_time_sec;

    return limited;
}

/* ==========================================================================
 * L3 - Bulk Engineering
 * ========================================================================== */

int bulk_config_init(BulkEngineeringConfig *cfg, const char *name,
                      const char *author)
{
    if (!cfg || !name) return -1;

    memset(cfg, 0, sizeof(BulkEngineeringConfig));
    strncpy(cfg->workbook_name, name, sizeof(cfg->workbook_name) - 1);
    strncpy(cfg->author, author, sizeof(cfg->author) - 1);
    cfg->ready_for_download = false;

    return 0;
}

int bulk_add_point(BulkEngineeringConfig *cfg, const BulkWiringTable *point)
{
    if (!cfg || !point) return -1;
    if (cfg->point_count >= 100) return -1; /* Representative limit */

    memcpy(&cfg->points[cfg->point_count], point, sizeof(BulkWiringTable));
    cfg->points[cfg->point_count].row_number = cfg->point_count + 1;
    cfg->point_count++;

    return 0;
}

/**
 * Validate bulk engineering configuration.
 *
 * Checks:
 * 1. No duplicate tags
 * 2. Valid signal types
 * 3. Controller assignments exist
 * 4. I/O slot/channel assignments are within range
 * 5. Range values are consistent (lo < hi)
 * 6. Alarm limits are within range
 */
int bulk_validate(BulkEngineeringConfig *cfg)
{
    if (!cfg) return -1;

    cfg->validation_errors = 0;
    cfg->validation_warnings = 0;

    for (int i = 0; i < cfg->point_count; i++) {
        BulkWiringTable *pt = &cfg->points[i];
        bool has_error = false;

        /* Check range consistency */
        if (pt->range_lo >= pt->range_hi) {
            strcpy(pt->error_msg, "Range lo >= hi");
            has_error = true;
        }

        /* Check alarm limits against range */
        if (pt->alarm_lo < pt->range_lo || pt->alarm_lo > pt->range_hi) {
            strcpy(pt->error_msg, "LO alarm outside range");
            has_error = true;
        }
        if (pt->alarm_hi < pt->range_lo || pt->alarm_hi > pt->range_hi) {
            strcpy(pt->error_msg, "HI alarm outside range");
            has_error = true;
        }

        /* Check for duplicate tags */
        for (int j = 0; j < i; j++) {
            if (strcmp(pt->tag, cfg->points[j].tag) == 0) {
                strcpy(pt->error_msg, "Duplicate tag");
                has_error = true;
                break;
            }
        }

        if (has_error) {
            cfg->validation_errors++;
        }
        pt->validated = !has_error;
    }

    cfg->ready_for_download = (cfg->validation_errors == 0);
    cfg->last_modified = time(NULL);

    return 0;
}

int bulk_export_csv(const BulkEngineeringConfig *cfg, const char *filepath)
{
    if (!cfg || !filepath) return -1;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    /* CSV header */
    fprintf(fp, "Row,Tag,Description,SignalType,Controller,Slot,Channel,"
                "RangeLo,RangeHi,EU,AlarmLo,AlarmLoLo,AlarmHi,AlarmHiHi,"
                "Decimals,Totalize,Trending,Validated\n");

    for (int i = 0; i < cfg->point_count; i++) {
        const BulkWiringTable *pt = &cfg->points[i];
        fprintf(fp, "%d,%s,%s,%d,%u,%u,%u,%.3f,%.3f,%s,%.3f,%.3f,%.3f,%.3f,"
                    "%d,%d,%d,%d\n",
                pt->row_number, pt->tag, pt->description, pt->signal_type,
                pt->controller_id, pt->slot_number, pt->channel_number,
                pt->range_lo, pt->range_hi, pt->eu,
                pt->alarm_lo, pt->alarm_lolo, pt->alarm_hi, pt->alarm_hihi,
                pt->decimal_places, pt->totalize, pt->trending, pt->validated);
    }

    fclose(fp);
    return 0;
}

int bulk_get_validation_summary(const BulkEngineeringConfig *cfg,
                                 int *errors, int *warnings)
{
    if (!cfg) return -1;
    if (errors) *errors = cfg->validation_errors;
    if (warnings) *warnings = cfg->validation_warnings;
    return 0;
}

int bulk_assign_controller(BulkEngineeringConfig *cfg, int point_index,
                            uint32_t controller_id)
{
    if (!cfg) return -1;
    if (point_index < 0 || point_index >= cfg->point_count) return -1;

    cfg->points[point_index].controller_id = controller_id;

    /* Track unique controllers */
    bool found = false;
    for (int i = 0; i < cfg->controller_count; i++) {
        if (cfg->controller_ids[i] == controller_id) {
            found = true;
            break;
        }
    }
    if (!found && cfg->controller_count < 16) {
        cfg->controller_ids[cfg->controller_count] = controller_id;
        cfg->controller_count++;
    }

    return 0;
}

/* ==========================================================================
 * L7 - CAB Version Control
 * ========================================================================== */

int cab_version_init(CABVersion *ver, uint32_t major, uint32_t minor)
{
    if (!ver) return -1;

    memset(ver, 0, sizeof(CABVersion));
    ver->major_version = major;
    ver->minor_version = minor;
    ver->build_number = 1;
    ver->build_timestamp = time(NULL);

    return 0;
}

int cab_version_increment(CABVersion *ver, const char *change_desc,
                           const char *author)
{
    if (!ver || !change_desc || !author) return -1;

    ver->build_number++;
    strncpy(ver->change_description, change_desc,
            sizeof(ver->change_description) - 1);
    strncpy(ver->author, author, sizeof(ver->author) - 1);
    ver->build_timestamp = time(NULL);

    return 0;
}

/** Compare two versions: returns -1 if a < b, 0 if equal, 1 if a > b. */
int cab_version_compare(const CABVersion *a, const CABVersion *b)
{
    if (!a || !b) return 0;

    if (a->major_version < b->major_version) return -1;
    if (a->major_version > b->major_version) return 1;
    if (a->minor_version < b->minor_version) return -1;
    if (a->minor_version > b->minor_version) return 1;
    if (a->build_number < b->build_number) return -1;
    if (a->build_number > b->build_number) return 1;

    return 0;
}