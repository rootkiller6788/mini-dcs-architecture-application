/**
 * @file    ecs700_io_subsystem.h
 * @brief   SUPCON ECS-700 I/O Subsystem — SBUS and I/O Modules
 *
 * The I/O subsystem connects field instruments to the DCS control
 * station via the SBUS (System Bus) redundant fieldbus.
 *
 * I/O Module Types (SUPCON ECS-700 Series):
 *   - AI711-S: 8-ch Analog Input (4-20 mA, isolated)
 *   - AI713-S: 8-ch Analog Input (TC/mV, isolated)
 *   - AI721-S: 8-ch Analog Input (RTD, isolated)
 *   - AO711-S: 8-ch Analog Output (4-20 mA, isolated)
 *   - DI711-S: 16-ch Digital Input (24 VDC, isolated)
 *   - DI721-S: 16-ch Digital Input (220 VAC, isolated)
 *   - DO711-S: 16-ch Digital Output (Relay, isolated)
 *   - PI721-S: 8-ch Pulse Input (frequency/totalizer)
 *
 * SBUS Communication:
 *   - Physical: RS-485 differential pair (redundant A/B)
 *   - Protocol: HDLC-based with address polling
 *   - Speed: 1 Mbps
 *   - Max distance: 500 m (RS-485 specification)
 *   - Max modules per bus: 32 (standard), 128 (with repeaters)
 *
 * Knowledge Coverage:
 *   L1: I/O module types, signal conditioning, channel mapping
 *   L2: Scan cycle, signal quality, fault detection
 *   L3: SBUS protocol, I/O addressing scheme
 *   L4: IEC 61131-2 (equipment requirements), accuracy specifications
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#ifndef ECS700_IO_SUBSYSTEM_H
#define ECS700_IO_SUBSYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecs700_system_core.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * L1: Core Definitions — I/O Subsystem Parameters
 * ============================================================================
 */

/** Maximum I/O channels per module (hardware limit) */
#define ECS700_IO_MAX_CHANNELS_PER_MODULE 32

/** Maximum I/O modules per SBUS segment */
#define ECS700_SBUS_MAX_MODULES          32

/** SBUS communication speed in bps */
#define ECS700_SBUS_SPEED_BPS            1000000UL /* 1 Mbps */

/** SBUS scan time target per module (μs) */
#define ECS700_SBUS_SCAN_PER_MODULE_US   2000UL   /* 2 ms */

/** Analog input ADC resolution (bits) */
#define ECS700_AI_ADC_RESOLUTION         16

/** Analog output DAC resolution (bits) */
#define ECS700_AO_DAC_RESOLUTION         16

/** Default signal filter time constant (seconds) */
#define ECS700_DEFAULT_FILTER_TC         0.5

/** Open-wire detection threshold (4-20 mA) */
#define ECS700_AI_OPEN_WIRE_MA           3.6

/** Over-range detection threshold (4-20 mA) */
#define ECS700_AI_OVERRANGE_MA           20.8

/* ============================================================================
 * L1: Core Data Structures — I/O Subsystem
 * ============================================================================
 */

/**
 * @brief I/O module operational states
 */
typedef enum {
    ECS700_IO_MODULE_OFFLINE    = 0, /**< Not detected / powered off */
    ECS700_IO_MODULE_BOOTING    = 1, /**< Firmware loading */
    ECS700_IO_MODULE_ONLINE     = 2, /**< Operational */
    ECS700_IO_MODULE_DEGRADED   = 3, /**< Some channels failed */
    ECS700_IO_MODULE_MAINTENANCE = 4, /**< Maintenance mode */
    ECS700_IO_MODULE_FAULT      = 5  /**< Module failure */
} ecs700_io_module_state_t;

/**
 * @brief I/O channel signal quality
 *
 * Per-channel quality assessment for fault detection and
 * control strategy adaptation (e.g., PV bad → hold last output).
 */
typedef enum {
    ECS700_IO_QUALITY_GOOD      = 0xC0, /**< Good quality (OPC UA standard) */
    ECS700_IO_QUALITY_UNCERTAIN = 0x40, /**< Uncertain (noisy, drifting) */
    ECS700_IO_QUALITY_BAD       = 0x00, /**< Bad (fault, open wire, overrange) */
    ECS700_IO_QUALITY_MANUAL    = 0x80  /**< Manual value substituted */
} ecs700_io_quality_t;

/**
 * @brief I/O module type identifiers (SUPCON catalog)
 */
typedef enum {
    ECS700_MODULE_AI711       = 0x1001, /**< 8-ch AI 4-20 mA */
    ECS700_MODULE_AI713       = 0x1002, /**< 8-ch AI TC/mV */
    ECS700_MODULE_AI721       = 0x1003, /**< 8-ch AI RTD Pt100/Cu50 */
    ECS700_MODULE_AO711       = 0x2001, /**< 8-ch AO 4-20 mA */
    ECS700_MODULE_DI711       = 0x3001, /**< 16-ch DI 24 VDC */
    ECS700_MODULE_DI721       = 0x3002, /**< 16-ch DI 220 VAC */
    ECS700_MODULE_DO711       = 0x4001, /**< 16-ch DO Relay */
    ECS700_MODULE_DO721       = 0x4002, /**< 16-ch DO SSR */
    ECS700_MODULE_PI721       = 0x5001  /**< 8-ch Pulse Input */
} ecs700_io_module_type_t;

/**
 * @brief Individual I/O channel configuration and status
 *
 * Each physical I/O channel on a module is represented by this
 * structure. It maps the raw electrical signal to the DCS
 * process point database.
 */
typedef struct {
    uint8_t   channel_index;              /**< Channel number on module (0-based) */
    ecs700_signal_type_t signal_type;     /**< Electrical signal type */
    ecs700_eu_range_t eu_range;           /**< Engineering unit scaling */
    bool      enabled;                    /**< Channel enabled */
    ecs700_io_quality_t quality;          /**< Current signal quality */
    double    raw_value_ma;              /**< Raw value in mA/V */
    uint16_t  raw_adc_counts;            /**< Raw ADC reading (0-65535) */
    double    eu_value;                  /**< Converted engineering unit value */
    double    filtered_eu_value;         /**< Filtered engineering unit value */
    uint64_t  last_read_time;            /**< Last read timestamp μs */
    double    filter_tc;                 /**< Low-pass filter time constant (s) */
    bool      open_wire_detected;        /**< 4-20 mA open circuit detected */
    bool      overrange;                 /**< Signal exceeds range */
    bool      underrange;                /**< Signal below range */
    uint8_t   fault_count;               /**< Consecutive fault detections */
    bool      burn_out_enabled;          /**< TC burnout detection enabled */
    double    cjc_temperature;           /**< Cold junction compensation (TC) */
    char      tag[ECS700_TAG_LEN_MAX];   /**< Mapped process tag */
} ecs700_io_channel_t;

/**
 * @brief I/O module configuration and diagnostic data
 *
 * Represents a single I/O module on the SBUS. Each module
 * contains 8, 16, or 32 channels depending on type.
 */
typedef struct {
    uint16_t  module_id;                  /**< SBUS address (1-128) */
    ecs700_io_module_type_t module_type;  /**< Hardware module type */
    ecs700_io_module_state_t state;       /**< Current operational state */
    uint8_t   firmware_version[4];        /**< Firmware version X.Y.Z.B */
    char      module_name[48];            /**< User-assigned module name */
    uint16_t  control_station_id;         /**< Parent control station */
    uint8_t   num_channels;               /**< Total physical channels */
    uint8_t   num_enabled_channels;       /**< Channels in use */
    ecs700_io_channel_t channels[ECS700_IO_MAX_CHANNELS_PER_MODULE];
    uint64_t  last_scan_time;             /**< Last SBUS scan timestamp */
    uint32_t  scan_errors;               /**< Cumulative communication errors */
    uint32_t  scan_count;                /**< Total scan cycles performed */
    double    module_temperature_c;       /**< Module internal temperature */
    double    supply_voltage_v;           /**< Module supply voltage */
    bool      redundancy_active;          /**< Redundant module installed */
    uint16_t  redundant_module_id;        /**< Partner redundant module */
    uint64_t  total_uptime_ms;            /**< Module total uptime */
    bool      hot_swap_supported;         /**< Hot swap capable */
} ecs700_io_module_t;

/* ============================================================================
 * L2: Core Concepts — I/O Channel Operations
 * ============================================================================
 */

/**
 * @brief Initialize an I/O channel with signal type and scaling
 *
 * Configures the fundamental measurement path: signal type →
 * ADC range → engineering units. Applies default filter
 * time constant.
 *
 * Knowledge Point: I/O Configuration — each measurement channel
 * must be configured with its electrical characteristics before
 * it can produce valid process values. Misconfiguration is a
 * common source of control problems.
 *
 * @param channel     I/O channel to configure
 * @param ch_index    Channel number on module
 * @param signal_type Electrical signal type
 * @param eu_lo       Engineering unit minimum
 * @param eu_hi       Engineering unit maximum
 * @param eu_label    Engineering unit label
 * @param tag         Process tag name
 */
void ecs700_io_channel_init(ecs700_io_channel_t *channel,
                             uint8_t ch_index,
                             ecs700_signal_type_t signal_type,
                             double eu_lo, double eu_hi,
                             const char *eu_label, const char *tag);

/**
 * @brief Process raw ADC reading into engineering units with quality assessment
 *
 * The complete measurement processing chain:
 *   1. Range check (open wire, overrange, underrange)
 *   2. Raw ADC → Engineering units via linear scaling
 *   3. First-order low-pass filter
 *   4. Quality assessment
 *
 * Knowledge Point: Measurement Processing Chain — the multi-step
 * transformation from raw electrical signal to validated process
 * value. Each step adds latency and potential error.
 *
 * @param channel       I/O channel
 * @param raw_adc_counts Raw ADC reading (0-65535 for 16-bit)
 * @param time_us       Current timestamp
 */
void ecs700_io_process_input(ecs700_io_channel_t *channel,
                              uint16_t raw_adc_counts,
                              uint64_t time_us);

/**
 * @brief Set analog output channel to engineering units value
 *
 * Converts EU → DAC counts and applies output limits.
 * The actual DCS output is written by the control station
 * during the output scan phase.
 *
 * @param channel  Output channel
 * @param eu_value Desired output in engineering units
 * @return DAC count value written (0-65535), or 0 on fault
 */
uint16_t ecs700_io_set_output(ecs700_io_channel_t *channel, double eu_value);

/**
 * @brief Check for open-wire condition on 4-20 mA input
 *
 * Open-wire detection: current < 3.6 mA indicates broken wire
 * or transmitter failure. This is the standard NAMUR NE43
 * fault detection method.
 *
 * Knowledge Point: NAMUR NE43 — the standard for 4-20 mA
 * fault detection. Current < 3.6 mA = fault (open wire);
 * current > 20.5 mA = fault (short). This enables automatic
 * fault detection without additional wiring.
 *
 * @param current_ma Measured loop current in mA
 * @return true if open wire detected
 */
bool ecs700_io_detect_open_wire(double current_ma);

/**
 * @brief Detect over-range condition
 *
 * @param current_ma Measured loop current in mA
 * @return true if over-range (>20.8 mA)
 */
bool ecs700_io_detect_overrange(double current_ma);

/* ============================================================================
 * L2: Core Concepts — I/O Module Operations
 * ============================================================================
 */

/**
 * @brief Initialize an I/O module
 *
 * Configures module address, type, and creates all channels.
 *
 * @param module       I/O module to initialize
 * @param module_id    SBUS address
 * @param module_type  Hardware module type
 * @param cs_id        Parent control station ID
 * @param name         Module name
 */
void ecs700_io_module_init(ecs700_io_module_t *module,
                            uint16_t module_id,
                            ecs700_io_module_type_t module_type,
                            uint16_t cs_id, const char *name);

/**
 * @brief Scan all enabled channels on a module
 *
 * Simulates one SBUS scan cycle: reads all configured channels,
 * processes ADC values, updates quality, and flags faults.
 *
 * Knowledge Point: I/O Scan Cycle — the periodic reading of all
 * field inputs. ECS-700 scans modules sequentially on SBUS, with
 * typical per-module scan time of 2 ms. Total scan must fit within
 * the control station scan period.
 *
 * @param module       I/O module
 * @param raw_values   Array of raw ADC values (indexed by channel)
 * @param num_values   Number of values provided
 * @param time_us      Current timestamp
 * @return Number of channels successfully scanned
 */
int ecs700_io_module_scan(ecs700_io_module_t *module,
                           const uint16_t *raw_values,
                           uint8_t num_values, uint64_t time_us);

/**
 * @brief Get the number of faulted channels on a module
 *
 * @param module  I/O module
 * @return Number of channels in fault state
 */
uint8_t ecs700_io_module_fault_count(const ecs700_io_module_t *module);

/**
 * @brief Estimate SBUS bus cycle time for a given configuration
 *
 * Cycle time = sum of per-module scan times + protocol overhead.
 * Must be less than the CS scan period to avoid scan overruns.
 *
 * @param num_modules    Number of modules on bus
 * @param scan_us_per_module Per-module scan time (typical 2000 μs)
 * @return Estimated bus cycle time in microseconds
 */
double ecs700_sbus_cycle_time_estimate(uint8_t num_modules,
                                        double scan_us_per_module);

/* ============================================================================
 * L3: Engineering Structures — Signal Processing
 * ============================================================================
 */

/**
 * @brief Apply CJC (Cold Junction Compensation) for thermocouple
 *
 * Thermocouples measure temperature difference between hot junction
 * (process) and cold junction (terminal block). The cold junction
 * temperature must be measured (typically with an RTD on the TC
 * input module) and added to the TC reading.
 *
 * T_actual = T_measured + T_cold_junction
 *
 * Knowledge Point: CJC Compensation — essential for accurate TC
 * measurement. Without CJC, TC readings are offset by the terminal
 * block temperature, which can vary 20-60°C in industrial cabinets.
 *
 * @param tc_voltage_mv    Measured thermocouple voltage in mV
 * @param cjc_temp_c       Cold junction temperature in °C
 * @param tc_type          TC type (0=K, 1=J, 2=T, 3=E, 4=N, 5=R, 6=S, 7=B)
 * @return Compensated temperature in °C
 */
double ecs700_io_cjc_compensate(double tc_voltage_mv, double cjc_temp_c,
                                 int tc_type);

/**
 * @brief Linearize square-root for differential pressure flow
 *
 * Flow measurement using orifice plates / DP transmitters:
 *   Q = k * sqrt(ΔP)
 *
 * DCS typically performs square-root extraction in software
 * rather than in the transmitter, for better low-flow cut-off
 * control and digital accuracy.
 *
 * Knowledge Point: Square-Root Extraction — converts DP to flow.
 * Low-flow cut-off prevents noise amplification at low flow:
 * if ΔP < cut_off, flow = 0.
 *
 * @param dp_percent      Differential pressure as % of span (0-100)
 * @param cut_off_percent Low-flow cut-off threshold (typical 0.5-2%)
 * @return Flow as % of span (0-100)
 */
double ecs700_io_sqrt_extract(double dp_percent, double cut_off_percent);

/**
 * @brief Convert raw RTD resistance to temperature (Pt100)
 *
 * Pt100 resistance-to-temperature conversion using
 * IEC 60751 Callendar-Van Dusen equation:
 *
 * For -200°C ≤ t < 0°C:
 *   R_t = R₀[1 + At + Bt² + C(t - 100)t³]
 * For 0°C ≤ t ≤ 850°C:
 *   R_t = R₀(1 + At + Bt²)
 *
 * This function implements the inverse (R → t) using
 * the quadratic formula for t ≥ 0°C and iteration for t < 0°C.
 *
 * Knowledge Point: RTD Linearization — the polynomial conversion
 * from resistance to temperature. IEC 60751 defines the standard
 * coefficients for Pt100 (α = 0.00385055).
 *
 * @param resistance_ohm Measured resistance in Ohms
 * @return Temperature in °C
 */
double ecs700_io_rtd_to_temp(double resistance_ohm);

/**
 * @brief Simulate NAMUR NE43-compliant transmitter output
 *
 * Converts engineering unit value to 4-20 mA output with
 * NAMUR fault signaling:
 *   - 3.8 mA ≤ normal ≤ 20.5 mA
 *   - ≤ 3.6 mA = fault low (burnout down)
 *   - ≥ 21.0 mA = fault high (burnout up)
 *
 * @param eu_value   Engineering unit value
 * @param eu_lo      Engineering unit zero
 * @param eu_hi      Engineering unit span
 * @return Current in mA
 */
double ecs700_io_eu_to_transmitter_ma(double eu_value,
                                       double eu_lo, double eu_hi);

/* ============================================================================
 * L4: Engineering Laws — Accuracy and Calibration
 * ============================================================================
 */

/**
 * @brief Compute measurement accuracy as % of span
 *
 * Accuracy(% of span) = (|measured - reference| / span) * 100
 *
 * ECS-700 AI module specification: ±0.1% of span @ 25°C.
 * Over full temperature range (-20 to +70°C): ±0.2% of span.
 *
 * Knowledge Point: Measurement Accuracy — the fundamental specification
 * for industrial I/O. Accuracy determines control quality limits.
 * Per IEC 61298-1: accuracy = max(error/span) across all test points.
 *
 * @param measured   Measured EU value
 * @param reference  Reference (true) EU value
 * @param span       Engineering unit span
 * @return Accuracy as % of span
 */
double ecs700_io_accuracy_pct_span(double measured, double reference,
                                    double span);

/**
 * @brief Compute signal-to-noise ratio for an analog input
 *
 * SNR (dB) = 10 * log10(P_signal / P_noise)
 * For DC signal: SNR = 20 * log10(V_signal / V_noise_rms)
 *
 * Required SNR for 0.1% accuracy 4-20 mA: ≥ 60 dB
 *
 * @param signal_amplitude Signal amplitude (EU or mA)
 * @param noise_rms        RMS noise amplitude
 * @return SNR in dB
 */
double ecs700_io_compute_snr(double signal_amplitude, double noise_rms);

/**
 * @brief Calculate effective number of bits (ENOB) for ADC
 *
 * ENOB = (SNR_dB - 1.76) / 6.02
 *
 * A 16-bit ADC with 85 dB SNR yields ENOB ≈ 13.8 bits.
 * The difference from theoretical is due to quantization noise,
 * thermal noise, and clock jitter.
 *
 * Knowledge Point: ENOB — the true resolution of an ADC after
 * accounting for all noise sources. Critical for understanding
 * the practical limits of measurement resolution.
 *
 * @param snr_db Signal-to-noise ratio in dB
 * @return Effective number of bits
 */
double ecs700_io_enob(double snr_db);

#ifdef __cplusplus
}
#endif

#endif /* ECS700_IO_SUBSYSTEM_H */
