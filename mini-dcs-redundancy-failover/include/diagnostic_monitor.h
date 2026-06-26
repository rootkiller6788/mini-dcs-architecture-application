/**
 * @file diagnostic_monitor.h
 * @brief Diagnostic Monitoring and Fault Detection
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover (7. mini-dcs-architecture-application)
 *
 * Knowledge Coverage:
 *   L3 - Engineering structures: watchdog timers, CRC memory checks,
 *        diagnostic coverage models
 *   L5 - Algorithms: fault classification, error logging, trend analysis
 *   L2 - Core concepts: diagnostic coverage, fault reaction time
 *
 * Reference:
 *   - IEC 61508-2:2010 -- Requirements for E/E/PE safety-related systems
 *   - IEC 61508-6 Annex A -- Diagnostic coverage estimation
 */

#ifndef DIAGNOSTIC_MONITOR_H
#define DIAGNOSTIC_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DIAG_MAX_FAULT_LOG 512
#define DIAG_WATCHDOG_PERIOD_MS 50
#define DIAG_MAX_TREND_POINTS 1024

typedef enum {
    DIAG_FAULT_NONE             = 0,
    DIAG_FAULT_MEMORY_CORRUPTION = 1,
    DIAG_FAULT_TIMING_OVERRUN   = 2,
    DIAG_FAULT_COMMUNICATION_LOSS = 3,
    DIAG_FAULT_POWER_VOLTAGE    = 4,
    DIAG_FAULT_TEMPERATURE_HIGH = 5,
    DIAG_FAULT_WATCHDOG_TIMEOUT = 6,
    DIAG_FAULT_CRC_MISMATCH     = 7,
    DIAG_FAULT_ADC_DRIFT        = 8,
    DIAG_FAULT_DAC_STUCK        = 9,
    DIAG_FAULT_RAM_PARITY       = 10,
    DIAG_FAULT_ROM_CHECKSUM     = 11,
    DIAG_FAULT_STACK_OVERFLOW   = 12,
    DIAG_FAULT_CLOCK_DRIFT      = 13,
    DIAG_FAULT_COUNT            = 14
} diag_fault_type_t;

typedef enum {
    DIAG_SEVERITY_INFO     = 0,
    DIAG_SEVERITY_WARNING  = 1,
    DIAG_SEVERITY_ERROR    = 2,
    DIAG_SEVERITY_CRITICAL = 3
} diag_severity_t;

typedef struct {
    diag_fault_type_t type;
    diag_severity_t  severity;
    uint64_t         timestamp_ms;
    uint32_t         fault_code;
    char             description[128];
    uint8_t          module_slot;
    bool             acknowledged;
} diag_fault_record_t;

typedef struct {
    double   value;
    uint64_t timestamp_ms;
    bool     valid;
} diag_trend_point_t;

typedef struct {
    /* Watchdog */
    bool     watchdog_enabled;
    uint32_t watchdog_period_ms;
    uint64_t last_kick_time_ms;
    uint32_t watchdog_timeout_count;

    /* Diagnostic coverage */
    double   diag_coverage_factor;
    double   lambda_total;
    double   lambda_dd;
    double   lambda_du;
    double   lambda_s;

    /* Fault log */
    diag_fault_record_t fault_log[DIAG_MAX_FAULT_LOG];
    uint32_t fault_log_count;
    uint32_t fault_counts_by_type[DIAG_FAULT_COUNT];

    /* Trend monitoring */
    diag_trend_point_t trend_data[DIAG_MAX_TREND_POINTS];
    uint32_t trend_index;
    uint32_t trend_count;

    /* Memory diagnostics */
    uint32_t ram_crc_periodic;
    uint32_t rom_checksum;
    bool     memory_test_pass;

    /* Timing diagnostics */
    uint64_t max_cycle_time_us;
    uint64_t min_cycle_time_us;
    uint64_t avg_cycle_time_us;
    uint32_t cycle_overrun_count;

    /* Communication diagnostics */
    uint32_t crc_errors;
    uint32_t sequence_gaps;
    uint32_t timeout_events;
    uint32_t retransmit_count;
} diag_monitor_t;

/**
 * Initialize the diagnostic monitor.
 * @param dm       Diagnostic monitor
 * @param lambda_t Total failure rate (failures/hour)
 * @param coverage Diagnostic coverage factor [0,1]
 * @return         0 on success, -1 on error
 * Complexity: O(1)
 */
int diag_init(diag_monitor_t *dm, double lambda_t, double coverage);

/**
 * Kick (reset) the watchdog timer. Must be called periodically
 * within watchdog_period_ms to prevent timeout.
 * @param dm Diagnostic monitor
 * Complexity: O(1)
 */
void diag_watchdog_kick(diag_monitor_t *dm);

/**
 * Check if the watchdog has timed out.
 * @param dm Diagnostic monitor
 * @return   true if watchdog expired
 * Complexity: O(1)
 */
bool diag_watchdog_expired(const diag_monitor_t *dm);

/**
 * Compute CRC-32 over a memory region for RAM diagnostics.
 * Polynomial: 0xEDB88320 (IEEE 802.3)
 * @param data Data buffer
 * @param len  Length in bytes
 * @return     CRC-32 value
 * Complexity: O(N)
 */
uint32_t diag_crc32(const uint8_t *data, size_t len);

/**
 * Verify ROM integrity using a stored checksum.
 * @param dm              Diagnostic monitor
 * @param rom_data        ROM data buffer
 * @param rom_size        ROM size in bytes
 * @param stored_checksum Expected checksum
 * @return                true if ROM checksum matches
 * Complexity: O(N)
 */
bool diag_rom_verify(diag_monitor_t *dm,
                     const uint8_t *rom_data, size_t rom_size,
                     uint32_t stored_checksum);

/**
 * Record a diagnostic fault event.
 * @param dm          Diagnostic monitor
 * @param type        Fault type
 * @param severity    Fault severity
 * @param fault_code  Application-specific fault code
 * @param description Human-readable description
 * @param module_slot Affected module slot
 * @return            0 on success, -1 if log full
 * Complexity: O(1)
 */
int diag_log_fault(diag_monitor_t *dm,
                   diag_fault_type_t type,
                   diag_severity_t severity,
                   uint32_t fault_code,
                   const char *description,
                   uint8_t module_slot);

/**
 * Retrieve fault statistics by type.
 * @param dm   Diagnostic monitor
 * @param type Fault type to query
 * @return     Count of faults of this type
 * Complexity: O(1)
 */
uint32_t diag_fault_count_by_type(const diag_monitor_t *dm,
                                  diag_fault_type_t type);

/**
 * Record a trend data point for time-series analysis.
 * @param dm        Diagnostic monitor
 * @param value     Measured value
 * @param timestamp Timestamp in ms
 * Complexity: O(1)
 */
void diag_record_trend(diag_monitor_t *dm, double value, uint64_t timestamp);

/**
 * Compute the linear trend slope over recent trend data.
 * Uses simple linear regression: y = ax + b.
 * @param dm     Diagnostic monitor
 * @param window Number of recent points to use
 * @return       Slope a (rate of change per point)
 * Complexity: O(W) where W = window
 */
double diag_trend_slope(const diag_monitor_t *dm, uint32_t window);

/**
 * Compute moving average of recent trend data.
 * @param dm     Diagnostic monitor
 * @param window Number of recent points to average
 * @return       Moving average, or 0 if insufficient data
 * Complexity: O(W)
 */
double diag_trend_moving_average(const diag_monitor_t *dm, uint32_t window);

/**
 * Detect anomalies in trend data using z-score method.
 * A point is anomalous if |z-score| > threshold.
 * @param dm        Diagnostic monitor
 * @param window    Window for computing mean/stddev
 * @param z_threshold Z-score threshold (typical 3.0)
 * @return          Number of anomaly points found in window
 * Complexity: O(W)
 */
uint32_t diag_detect_anomalies(const diag_monitor_t *dm,
                               uint32_t window, double z_threshold);

/**
 * Update cycle timing statistics.
 * @param dm          Diagnostic monitor
 * @param cycle_time_us Current cycle execution time in microseconds
 * Complexity: O(1)
 */
void diag_update_cycle_time(diag_monitor_t *dm, uint64_t cycle_time_us);

/**
 * Record a communication error event.
 * @param dm     Diagnostic monitor
 * @param crc_error True if CRC error, false if sequence gap
 * Complexity: O(1)
 */
void diag_record_comm_error(diag_monitor_t *dm, bool crc_error);

/**
 * Compute the diagnostic coverage factor from failure rates.
 * DC = lambda_DD / (lambda_DD + lambda_DU)
 * @param lambda_dd Dangerous detected failure rate
 * @param lambda_du Dangerous undetected failure rate
 * @return          Diagnostic coverage [0,1]
 * Complexity: O(1)
 */
double diag_coverage_factor(double lambda_dd, double lambda_du);

/**
 * Determine the diagnostic coverage class per IEC 61508-2 Table A.1.
 * None: DC < 60%
 * Low:  60% <= DC < 90%
 * Medium: 90% <= DC < 99%
 * High: DC >= 99%
 * @param dc Diagnostic coverage factor [0,1]
 * @return   Class string
 * Complexity: O(1)
 */
const char *diag_coverage_class(double dc);

/**
 * Estimate fault reaction time: detection + annunciation.
 * FRT = T_detect + T_annunciate
 * where T_detect depends on diagnostic test interval.
 * @param dm Diagnostic monitor
 * @return   Fault reaction time in milliseconds
 * Complexity: O(1)
 */
uint64_t diag_fault_reaction_time_ms(const diag_monitor_t *dm);

/**
 * Run a comprehensive built-in self-test (BIST).
 * Tests: RAM pattern test, ROM checksum, watchdog functionality,
 * CPU register test, interrupt controller test.
 * @param dm Diagnostic monitor
 * @return   Bitmask of passed tests (bit 0=RAM, 1=ROM, 2=WDT, 3=CPU, 4=INT)
 * Complexity: O(N) where N = memory size tested
 */
uint32_t diag_run_bist(diag_monitor_t *dm);

/**
 * Compute the probability that a fault is detected within the
 * diagnostic test interval, given the diagnostic coverage.
 *
 * P_detect = 1 - (1 - DC) * exp(-lambda_DU * T_diag)
 *
 * @param dc         Diagnostic coverage
 * @param lambda_du  Dangerous undetected failure rate
 * @param t_diag_ms  Diagnostic test interval in ms
 * @return           Detection probability [0,1]
 * Complexity: O(1)
 */
double diag_fault_detection_probability(double dc, double lambda_du,
                                        uint32_t t_diag_ms);

#endif /* DIAGNOSTIC_MONITOR_H */
