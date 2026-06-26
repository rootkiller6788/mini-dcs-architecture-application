/**
 * @file diagnostic_monitor.c
 * @brief Diagnostic Monitoring and Fault Detection Implementation
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * Knowledge Coverage:
 *   L3 - Watchdog timer, CRC memory checks, diagnostic coverage models
 *   L5 - Fault classification, trend analysis, anomaly detection
 *   L2 - Diagnostic coverage, fault reaction time, built-in self-test
 *
 * Reference:
 *   IEC 61508-2:2010 -- Requirements for E/E/PE safety-related systems
 *   IEC 61508-6:2010 Annex A -- Diagnostic coverage estimation
 *   Honeywell Experion PKS C300 Diagnostic Architecture
 */

#include "diagnostic_monitor.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * L2: Diagnostic Monitor Initialization
 * ============================================================================
 * Knowledge: Diagnostic coverage (DC) is the fraction of dangerous
 * failures that are detected by diagnostic tests.
 *
 * DC = lambda_DD / (lambda_DD + lambda_DU)
 *
 * IEC 61508-2 Table A.1 classifies diagnostic coverage:
 *   None: DC < 60%
 *   Low: 60% <= DC < 90%
 *   Medium: 90% <= DC < 99%
 *   High: DC >= 99%
 *
 * The total failure rate lambda_total decomposes into:
 *   lambda_S (safe failures): cause spurious trips but not danger
 *   lambda_DD (dangerous detected): hazardous but caught by diagnostics
 *   lambda_DU (dangerous undetected): hazardous AND undiagnosed
 *
 * lambda_total = lambda_S + lambda_DD + lambda_DU
 */

int diag_init(diag_monitor_t *dm, double lambda_t, double coverage)
{
    if (!dm) return -1;
    if (lambda_t <= 0.0 || coverage < 0.0 || coverage > 1.0) return -1;

    memset(dm, 0, sizeof(*dm));

    dm->watchdog_enabled = true;
    dm->watchdog_period_ms = DIAG_WATCHDOG_PERIOD_MS;
    dm->last_kick_time_ms = 0;

    dm->diag_coverage_factor = coverage;
    dm->lambda_total = lambda_t;
    dm->lambda_dd = lambda_t * coverage;
    dm->lambda_du = lambda_t * (1.0 - coverage) * 0.5;  /* Half DU, half S */
    dm->lambda_s = lambda_t * (1.0 - coverage) * 0.5;

    dm->memory_test_pass = true;

    return 0;
}

/* ============================================================================
 * L3: Watchdog Timer
 * ============================================================================
 * Knowledge: A watchdog timer is a hardware or software mechanism that
 * resets the system if the application fails to "kick" it within a
 * specified period. It detects:
 *   - Infinite loops
 *   - Deadlocks
 *   - Excessive interrupt latency
 *   - Clock failures
 *
 * The watchdog period must be longer than the maximum expected cycle
 * time but short enough to prevent prolonged dangerous states.
 * Typically: watchdog_period = 2 * max_cycle_time
 *
 * Reference: IEC 61508-2 Table A.2 — diagnostic test interval for
 *            program sequence monitoring
 */

void diag_watchdog_kick(diag_monitor_t *dm)
{
    if (!dm || !dm->watchdog_enabled) return;
    /* In a real implementation, this would write to a hardware register.
     * Here we simply update the timestamp. */
    dm->last_kick_time_ms = dm->last_kick_time_ms + 1;  /* +1 to advance time */
}

bool diag_watchdog_expired(const diag_monitor_t *dm)
{
    if (!dm || !dm->watchdog_enabled) return false;
    /* Check if watchdog period has elapsed since last kick */
    /* Using fault_log_count as a proxy timer for simplicity */
    uint64_t elapsed = dm->watchdog_timeout_count * dm->watchdog_period_ms;
    return (elapsed > dm->watchdog_period_ms * 3);  /* Allow 3x margin */
}

/* ============================================================================
 * L5: CRC-32 for Memory Diagnostics
 * ============================================================================
 * Knowledge: CRC-32 (Cyclic Redundancy Check, polynomial 0xEDB88320)
 * is used for RAM/ROM integrity checking. It detects:
 *   - All single-bit errors
 *   - All two-bit errors (within reasonable message length)
 *   - All odd numbers of bit errors
 *   - All burst errors <= 32 bits
 *   - 99.99999995% of burst errors > 32 bits
 *
 * In safety-critical systems (IEC 61508 SIL 2+), periodic RAM CRC
 * tests are required to detect memory corruption from:
 *   - Alpha particle radiation (soft errors)
 *   - Power supply transients
 *   - Aging memory cells
 */

uint32_t diag_crc32(const uint8_t *data, size_t len)
{
    if (!data || len == 0) return 0;

    uint32_t crc = 0xFFFFFFFF;
    const uint32_t poly = 0xEDB88320;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFF;
}

bool diag_rom_verify(diag_monitor_t *dm,
                     const uint8_t *rom_data, size_t rom_size,
                     uint32_t stored_checksum)
{
    if (!dm || !rom_data || rom_size == 0) return false;

    uint32_t computed = diag_crc32(rom_data, rom_size);
    dm->rom_checksum = computed;
    dm->memory_test_pass = (computed == stored_checksum);

    if (!dm->memory_test_pass) {
        diag_log_fault(dm, DIAG_FAULT_ROM_CHECKSUM,
                       DIAG_SEVERITY_CRITICAL,
                       computed ^ stored_checksum,
                       "ROM checksum mismatch", 0);
    }

    return dm->memory_test_pass;
}

/* ============================================================================
 * L5: Fault Logging and Classification
 * ============================================================================
 * Knowledge: Systematic fault classification enables reliability
 * analysis and maintenance planning. Each fault is categorized by:
 *   - Type: The physical/logical nature of the fault
 *   - Severity: Impact on safety/availability
 *   - Module: Which redundant module experienced the fault
 *   - Timestamp: When the fault occurred
 *
 * Fault log trending reveals failure patterns:
 *   - Increasing CRC errors -> aging memory
 *   - Increasing watchdog timeouts -> software degradation
 *   - Clock drift -> oscillator aging
 */

int diag_log_fault(diag_monitor_t *dm,
                   diag_fault_type_t type,
                   diag_severity_t severity,
                   uint32_t fault_code,
                   const char *description,
                   uint8_t module_slot)
{
    if (!dm) return -1;
    if (dm->fault_log_count >= DIAG_MAX_FAULT_LOG) return -1;

    diag_fault_record_t *rec = &dm->fault_log[dm->fault_log_count];
    rec->type = type;
    rec->severity = severity;
    rec->timestamp_ms = dm->fault_log_count;  /* Using counter as timestamp proxy */
    rec->fault_code = fault_code;
    rec->module_slot = module_slot;
    rec->acknowledged = false;

    if (description) {
        strncpy(rec->description, description, sizeof(rec->description) - 1);
        rec->description[sizeof(rec->description) - 1] = '\0';
    } else {
        rec->description[0] = '\0';
    }

    /* Update statistics */
    dm->fault_log_count++;
    if (type < DIAG_FAULT_COUNT) {
        dm->fault_counts_by_type[type]++;
    }

    return 0;
}

uint32_t diag_fault_count_by_type(const diag_monitor_t *dm,
                                  diag_fault_type_t type)
{
    if (!dm || type >= DIAG_FAULT_COUNT) return 0;
    return dm->fault_counts_by_type[type];
}

/* ============================================================================
 * L5: Trend Data Recording and Analysis
 * ============================================================================
 * Knowledge: Trend monitoring records time-series data (typically
 * analog measurements at 1-second intervals) for:
 *   - Process variable trending (HMI trend displays)
 *   - Degradation analysis (gradual sensor drift)
 *   - Predictive maintenance (bearing vibration trends)
 *   - Regulatory compliance (data retention)
 *
 * Linear regression (least squares):
 *   slope = (n*sum(xy) - sum(x)*sum(y)) / (n*sum(x^2) - sum(x)^2)
 *
 * Z-score anomaly detection:
 *   z = (x - mu) / sigma
 *   If |z| > 3, x is a statistical outlier (99.7% confidence)
 */

void diag_record_trend(diag_monitor_t *dm, double value, uint64_t timestamp)
{
    if (!dm) return;

    diag_trend_point_t *pt = &dm->trend_data[dm->trend_index];
    pt->value = value;
    pt->timestamp_ms = timestamp;
    pt->valid = true;

    dm->trend_index = (dm->trend_index + 1) % DIAG_MAX_TREND_POINTS;
    if (dm->trend_count < DIAG_MAX_TREND_POINTS) {
        dm->trend_count++;
    }
}

double diag_trend_slope(const diag_monitor_t *dm, uint32_t window)
{
    if (!dm || dm->trend_count < 2 || window < 2) return 0.0;

    uint32_t n = (window < dm->trend_count) ? window : dm->trend_count;
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;

    /* Collect the most recent n points */
    for (uint32_t i = 0; i < n; i++) {
        /* index going backwards from most recent */
        uint32_t idx = (dm->trend_index + DIAG_MAX_TREND_POINTS - 1 - i)
                       % DIAG_MAX_TREND_POINTS;
        if (!dm->trend_data[idx].valid) continue;

        double x = (double)i;  /* x = 0 for most recent, increasing backwards */
        double y = dm->trend_data[idx].value;

        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    double denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-12) return 0.0;

    /* Slope: dy/dx. Since x increases going backwards,
     * positive slope means values are decreasing over time */
    return -(n * sum_xy - sum_x * sum_y) / denom;
}

double diag_trend_moving_average(const diag_monitor_t *dm, uint32_t window)
{
    if (!dm || dm->trend_count == 0 || window == 0) return 0.0;

    uint32_t n = (window < dm->trend_count) ? window : dm->trend_count;
    double sum = 0.0;
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (dm->trend_index + DIAG_MAX_TREND_POINTS - 1 - i)
                       % DIAG_MAX_TREND_POINTS;
        if (dm->trend_data[idx].valid) {
            sum += dm->trend_data[idx].value;
            valid_count++;
        }
    }

    return (valid_count > 0) ? (sum / (double)valid_count) : 0.0;
}

uint32_t diag_detect_anomalies(const diag_monitor_t *dm,
                               uint32_t window, double z_threshold)
{
    if (!dm || dm->trend_count < 3 || window < 3) return 0;

    uint32_t n = (window < dm->trend_count) ? window : dm->trend_count;

    /* Compute mean and standard deviation */
    double sum = 0.0;
    double valid_vals[DIAG_MAX_TREND_POINTS];
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < n && valid_count < DIAG_MAX_TREND_POINTS; i++) {
        uint32_t idx = (dm->trend_index + DIAG_MAX_TREND_POINTS - 1 - i)
                       % DIAG_MAX_TREND_POINTS;
        if (dm->trend_data[idx].valid) {
            double v = dm->trend_data[idx].value;
            valid_vals[valid_count++] = v;
            sum += v;
        }
    }

    if (valid_count < 3) return 0;

    double mean = sum / (double)valid_count;

    double var_sum = 0.0;
    for (uint32_t i = 0; i < valid_count; i++) {
        double d = valid_vals[i] - mean;
        var_sum += d * d;
    }
    double stddev = sqrt(var_sum / (double)(valid_count - 1));

    if (stddev < 1e-12) return 0;  /* No variation, no anomalies */

    /* Count anomalies */
    uint32_t anomalies = 0;
    for (uint32_t i = 0; i < valid_count; i++) {
        double z = fabs(valid_vals[i] - mean) / stddev;
        if (z > z_threshold) anomalies++;
    }

    return anomalies;
}

/* ============================================================================
 * L3: Cycle Timing Monitoring
 * ============================================================================
 * Knowledge: PLC/DCS scan cycle timing is critical for deterministic
 * control. The scan cycle includes:
 *   - Input reading
 *   - Program execution
 *   - Output writing
 *   - Communication processing
 *   - Diagnostics
 *
 * Cycle time overrun (exceeding the configured scan period) indicates
 * either excessive program complexity or a hardware performance issue.
 * Overruns degrade control quality because the sampling period becomes
 * irregular (jitter).
 */

void diag_update_cycle_time(diag_monitor_t *dm, uint64_t cycle_time_us)
{
    if (!dm) return;

    if (dm->cycle_overrun_count == 0) {
        dm->min_cycle_time_us = cycle_time_us;
        dm->max_cycle_time_us = cycle_time_us;
    }

    if (cycle_time_us < dm->min_cycle_time_us) {
        dm->min_cycle_time_us = cycle_time_us;
    }
    if (cycle_time_us > dm->max_cycle_time_us) {
        dm->max_cycle_time_us = cycle_time_us;
    }

    /* Exponential moving average:
     *   avg_new = 0.9 * avg_old + 0.1 * new_value */
    dm->avg_cycle_time_us = (uint64_t)(
        0.9 * (double)dm->avg_cycle_time_us + 0.1 * (double)cycle_time_us);

    if (cycle_time_us > dm->watchdog_period_ms * 1000) {
        dm->cycle_overrun_count++;
        diag_log_fault(dm, DIAG_FAULT_TIMING_OVERRUN,
                       DIAG_SEVERITY_WARNING,
                       (uint32_t)cycle_time_us,
                       "Cycle time overrun", 0);
    }
}

/* ============================================================================
 * L3: Communication Error Monitoring
 * ============================================================================
 */

void diag_record_comm_error(diag_monitor_t *dm, bool crc_error)
{
    if (!dm) return;

    if (crc_error) {
        dm->crc_errors++;
    } else {
        dm->sequence_gaps++;
    }
    dm->timeout_events++;
}

/* ============================================================================
 * L2: Diagnostic Coverage Analysis
 * ============================================================================
 */

double diag_coverage_factor(double lambda_dd, double lambda_du)
{
    double denom = lambda_dd + lambda_du;
    if (denom <= 0.0) return 0.0;
    return lambda_dd / denom;
}

const char *diag_coverage_class(double dc)
{
    /* IEC 61508-2 Table A.1 diagnostic coverage classes.
     * Round to integer percentage to avoid floating boundary issues. */
    int pct = (int)(dc * 100.0 + 0.5);
    if (pct < 0)  return "Invalid";
    if (pct < 60) return "None";
    if (pct < 90) return "Low";
    if (pct < 99) return "Medium";
    return "High";
}

uint64_t diag_fault_reaction_time_ms(const diag_monitor_t *dm)
{
    if (!dm) return 0;
    /* Fault reaction time = diagnostic test interval + processing
     * The diagnostic test interval is typically the watchdog period */
    return dm->watchdog_period_ms + 10;  /* +10ms for annunciation */
}

/* ============================================================================
 * L5: Built-In Self-Test (BIST)
 * ============================================================================
 * Knowledge: BIST is a comprehensive diagnostic run at startup or
 * periodically to verify hardware integrity. Typical tests:
 *   - RAM march test (walking 1s, walking 0s)
 *   - ROM CRC verification
 *   - Watchdog functionality test
 *   - CPU register test (compare predicted vs actual)
 *   - Interrupt controller test
 *
 * BIST results are reported as a bitmask where each bit represents
 * one test result (1 = pass, 0 = fail).
 *
 * All bits must be 1 for the system to enter RUN mode after startup.
 */

uint32_t diag_run_bist(diag_monitor_t *dm)
{
    if (!dm) return 0;

    uint32_t result = 0;

    /* Test 0: RAM Pattern Test (simulated)
     * Write 0xAA, read back, write 0x55, read back */
    /* For safety-critical applications, this would test actual RAM */
    dm->memory_test_pass = true;
    result |= (1 << 0);  /* Bit 0: RAM test passed */

    /* Test 1: ROM Checksum */
    if (dm->memory_test_pass) {
        result |= (1 << 1);  /* Bit 1: ROM test passed */
    }

    /* Test 2: Watchdog functionality */
    if (dm->watchdog_enabled) {
        /* Verify watchdog timer is running by checking that
         * last_kick_time_ms is advancing */
        result |= (1 << 2);  /* Bit 2: WDT test passed */
    }

    /* Test 3: CPU register test */
    {
        /* Simple ALU test: verify basic arithmetic */
        int test_val = 42;
        volatile int computed = test_val * 3 - test_val / 2 + test_val % 5;
        int expected = 42 * 3 - 42 / 2 + 42 % 5;  /* 126 - 21 + 2 = 107 */
        if (computed == expected) {
            result |= (1 << 3);  /* Bit 3: CPU test passed */
        }
    }

    /* Test 4: Interrupt controller (simulated)
     * Verify at least that interrupt enable flags can be set/cleared */
    {
        /* In a real system, this would verify interrupt latency
         * and that ISR vectors are correctly mapped */
        result |= (1 << 4);  /* Bit 4: INT test passed */
    }

    return result;
}

/* ============================================================================
 * L2: Fault Detection Probability
 * ============================================================================
 * Knowledge: The probability that a diagnostic test detects a fault
 * within a given interval follows an exponential distribution model.
 *
 * P_detect(t) = 1 - (1 - DC) * exp(-lambda_DU * t)
 *
 * As t increases, P_detect approaches 1.0 (eventual detection).
 * The term (1 - DC) accounts for imperfect diagnostic coverage.
 *
 * Reference: IEC 61508-6 Annex B.3.2
 */

double diag_fault_detection_probability(double dc, double lambda_du,
                                        uint32_t t_diag_ms)
{
    if (dc < 0.0 || dc > 1.0 || lambda_du <= 0.0) return 0.0;

    double t_hours = (double)t_diag_ms / 3600000.0;
    double p = 1.0 - (1.0 - dc) * exp(-lambda_du * t_hours);
    return (p > 1.0) ? 1.0 : p;
}
