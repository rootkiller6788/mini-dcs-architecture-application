/**
 * @file    ecs700_io_subsystem.c
 * @brief   SUPCON ECS-700 I/O Subsystem Implementation
 *
 * Implements I/O channel configuration, signal processing chain
 * (ADC → EU → filtering → quality), NAMUR NE43 fault detection,
 * thermocouple CJC compensation, RTD linearization, square-root
 * extraction, and measurement accuracy calculations.
 *
 * Knowledge Coverage:
 *   L1: I/O channel/模块配置，信号类型
 *   L2: 扫描周期处理，信号质量评估，NAMUR NE43
 *   L3: CJC热电偶补偿，开方提取，RTD线性化
 *   L4: IEC 60751 (RTD), NAMUR NE43, 精度/SNR/ENOB计算
 *
 * References:
 *   - IEC 60751: Industrial platinum resistance thermometers
 *   - NAMUR NE43: Standardization of signal levels for failure information
 *   - IEC 61298-1: Process measurement and control devices
 *   - SUPCON ECS-700 I/O Module Hardware Manual
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#include "ecs700_io_subsystem.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L1/L2: I/O Channel Configuration and Processing
 * ============================================================================
 */

void ecs700_io_channel_init(ecs700_io_channel_t *channel,
                             uint8_t ch_index,
                             ecs700_signal_type_t signal_type,
                             double eu_lo, double eu_hi,
                             const char *eu_label, const char *tag)
{
    if (channel == NULL) {
        return;
    }

    memset(channel, 0, sizeof(*channel));

    channel->channel_index = ch_index;
    channel->signal_type = signal_type;
    channel->enabled = true;
    channel->quality = ECS700_IO_QUALITY_BAD;  /* Not yet read */
    channel->filter_tc = ECS700_DEFAULT_FILTER_TC;

    /* Configure engineering unit range */
    channel->eu_range.eu_lo = eu_lo;
    channel->eu_range.eu_hi = eu_hi;
    channel->eu_range.raw_lo = 0.0;
    channel->eu_range.raw_hi = 65535.0;  /* 16-bit ADC full scale */
    channel->eu_range.decimal_places = 2;

    if (eu_label != NULL) {
        strncpy(channel->eu_range.eu_label, eu_label,
                sizeof(channel->eu_range.eu_label) - 1);
        channel->eu_range.eu_label[sizeof(channel->eu_range.eu_label) - 1] = '\0';
    }

    if (tag != NULL) {
        strncpy(channel->tag, tag, ECS700_TAG_LEN_MAX - 1);
        channel->tag[ECS700_TAG_LEN_MAX - 1] = '\0';
    }

    /* Configure raw range based on signal type */
    switch (signal_type) {
        case ECS700_SIGNAL_AI_4_20MA:
        case ECS700_SIGNAL_AI_0_10V:
            channel->eu_range.raw_lo = 0.0;
            channel->eu_range.raw_hi = 65535.0;
            break;
        case ECS700_SIGNAL_AI_TC:
        case ECS700_SIGNAL_AI_RTD:
            /* Temperature sensors use ADC internal ranges */
            channel->eu_range.raw_lo = -32768.0;
            channel->eu_range.raw_hi = 32767.0;
            break;
        default:
            channel->eu_range.raw_lo = 0.0;
            channel->eu_range.raw_hi = 65535.0;
            break;
    }
}

void ecs700_io_process_input(ecs700_io_channel_t *channel,
                              uint16_t raw_adc_counts,
                              uint64_t time_us)
{
    /**
     * I/O Input Processing Chain:
     *
     *   Raw ADC counts (0-65535 for 16-bit)
     *     ↓
     *   Range check (open wire, overrange, underrange)
     *     ↓
     *   Linear scaling → Engineering units
     *     ↓
     *   First-order low-pass filter
     *     ↓
     *   Quality assessment → Good/Uncertain/Bad
     *     ↓
     *   Update channel state
     */

    if (channel == NULL || !channel->enabled) {
        return;
    }

    /* Step 1: Store raw reading */
    channel->raw_adc_counts = raw_adc_counts;
    channel->last_read_time = time_us;

    /* Step 2: Convert ADC to approximate mA for current inputs
     * (for fault detection purposes) */
    double raw_ma = 4.0 + (double)raw_adc_counts / 65535.0 * 16.0;

    switch (channel->signal_type) {
        case ECS700_SIGNAL_AI_4_20MA:
        case ECS700_SIGNAL_AO_4_20MA:
            channel->raw_value_ma = raw_ma;

            /* NAMUR NE43 fault detection */
            if (ecs700_io_detect_open_wire(raw_ma)) {
                channel->open_wire_detected = true;
                channel->quality = ECS700_IO_QUALITY_BAD;
                channel->fault_count++;
            } else if (ecs700_io_detect_overrange(raw_ma)) {
                channel->overrange = true;
                channel->quality = ECS700_IO_QUALITY_UNCERTAIN;
            } else {
                channel->open_wire_detected = false;
                channel->overrange = false;
                channel->underrange = false;
                channel->fault_count = 0;
            }
            break;

        case ECS700_SIGNAL_AI_0_10V:
        case ECS700_SIGNAL_AO_0_10V:
            channel->raw_value_ma = (double)raw_adc_counts / 65535.0 * 10.0;
            break;

        default:
            channel->raw_value_ma = 0.0;
            break;
    }

    /* Step 3: Scale raw ADC to engineering units */
    /* Use stored eu_range for conversion */
    double eu_raw = channel->eu_range.eu_lo
                  + (double)raw_adc_counts * (channel->eu_range.eu_hi - channel->eu_range.eu_lo)
                  / 65535.0;
    channel->eu_value = eu_raw;

    /* Step 4: Apply low-pass filter */
    double sample_time_s;
    if (channel->last_read_time > 0 && time_us > channel->last_read_time) {
        sample_time_s = (double)(time_us - channel->last_read_time) / 1000000.0;
    } else {
        sample_time_s = 0.2;  /* Default 200 ms sample time */
    }

    /* Apply exponential filter */
    double alpha = sample_time_s / (sample_time_s + channel->filter_tc);
    if (alpha < 0.0) {
        alpha = 0.0;
    } else if (alpha > 1.0) {
        alpha = 1.0;
    }

    channel->filtered_eu_value = alpha * eu_raw
                               + (1.0 - alpha) * channel->filtered_eu_value;

    /* Step 5: Quality assessment */
    if (channel->fault_count == 0 && !channel->open_wire_detected) {
        channel->quality = ECS700_IO_QUALITY_GOOD;
    } else if (channel->fault_count <= 3) {
        channel->quality = ECS700_IO_QUALITY_UNCERTAIN;
    } else {
        channel->quality = ECS700_IO_QUALITY_BAD;
    }
}

uint16_t ecs700_io_set_output(ecs700_io_channel_t *channel, double eu_value)
{
    /**
     * Analog Output Processing:
     *
     *   Engineering unit value (e.g., 0-100% valve position)
     *     ↓
     *   Clamp to EU range
     *     ↓
     *   Linear scaling → DAC counts (0-65535)
     *     ↓
     *   Output to physical channel (with readback verification)
     */

    if (channel == NULL || !channel->enabled) {
        return 0;
    }

    /* Clamp to engineering unit range */
    if (eu_value < channel->eu_range.eu_lo) {
        eu_value = channel->eu_range.eu_lo;
    } else if (eu_value > channel->eu_range.eu_hi) {
        eu_value = channel->eu_range.eu_hi;
    }

    /* Scale EU to DAC counts */
    double dac_counts;
    if (channel->eu_range.eu_hi > channel->eu_range.eu_lo) {
        dac_counts = (eu_value - channel->eu_range.eu_lo)
                   * 65535.0
                   / (channel->eu_range.eu_hi - channel->eu_range.eu_lo);
    } else {
        dac_counts = 0.0;
    }

    /* Clamp to DAC range */
    if (dac_counts < 0.0) {
        dac_counts = 0.0;
    } else if (dac_counts > 65535.0) {
        dac_counts = 65535.0;
    }

    uint16_t dac_val = (uint16_t)(dac_counts + 0.5);
    channel->raw_adc_counts = dac_val;

    return dac_val;
}

bool ecs700_io_detect_open_wire(double current_ma)
{
    /**
     * NAMUR NE43 Open Wire Detection:
     *
     * For 4-20 mA transmitters:
     *   - 4.0 mA = 0% of measurement range
     *   - 20.0 mA = 100% of measurement range
     *
     * Fault signaling:
     *   - I < 3.6 mA  → Fault (open circuit / transmitter failure low)
     *   - I > 21.0 mA → Fault (short circuit / transmitter failure high)
     *   - 3.8-20.5 mA → Normal operating range
     *
     * Open wire detection threshold: < 3.6 mA
     * This is a hardware-independent fault detection mechanism:
     * even if the ADC reads perfectly, a broken wire will result
     * in 0 mA (or very low leakage current).
     */
    return (current_ma < ECS700_AI_OPEN_WIRE_MA);
}

bool ecs700_io_detect_overrange(double current_ma)
{
    /**
     * Over-range Detection:
     *
     * Current > 20.8 mA indicates the process variable has exceeded
     * the transmitter's calibrated range. This is a "soft" fault
     * (measurement may still be valid but accuracy is degraded).
     *
     * The 20.8 mA threshold provides 5% over-range capability
     * (20 mA + 5% = 21 mA, threshold set at 20.8 mA for margin).
     */
    return (current_ma > ECS700_AI_OVERRANGE_MA);
}

/* ============================================================================
 * L2: I/O Module Operations
 * ============================================================================
 */

void ecs700_io_module_init(ecs700_io_module_t *module,
                            uint16_t module_id,
                            ecs700_io_module_type_t module_type,
                            uint16_t cs_id, const char *name)
{
    if (module == NULL) {
        return;
    }

    memset(module, 0, sizeof(*module));

    module->module_id = module_id;
    module->module_type = module_type;
    module->control_station_id = cs_id;
    module->state = ECS700_IO_MODULE_BOOTING;

    if (name != NULL) {
        strncpy(module->module_name, name, sizeof(module->module_name) - 1);
        module->module_name[sizeof(module->module_name) - 1] = '\0';
    }

    /* Determine channel count based on module type */
    switch (module_type) {
        case ECS700_MODULE_AI711:
        case ECS700_MODULE_AI713:
        case ECS700_MODULE_AI721:
        case ECS700_MODULE_AO711:
        case ECS700_MODULE_PI721:
            module->num_channels = 8;
            break;
        case ECS700_MODULE_DI711:
        case ECS700_MODULE_DI721:
        case ECS700_MODULE_DO711:
        case ECS700_MODULE_DO721:
            module->num_channels = 16;
            break;
        default:
            module->num_channels = 8;
            break;
    }

    /* Set nominal operating parameters */
    module->module_temperature_c = 35.0;
    module->supply_voltage_v = 24.0;
    module->hot_swap_supported = true;
    module->state = ECS700_IO_MODULE_ONLINE;
}

int ecs700_io_module_scan(ecs700_io_module_t *module,
                           const uint16_t *raw_values,
                           uint8_t num_values, uint64_t time_us)
{
    /**
     * SBUS I/O Module Scan Cycle:
     *
     * Each scan cycle:
     *   1. CS sends scan request to module address on SBUS
     *   2. Module responds with all channel ADC values
     *   3. CS processes each channel through input processing chain
     *   4. Channel quality is assessed
     *   5. Values are published to real-time database
     *
     * Timing budget per module:
     *   - SBUS transaction: 1-2 ms
     *   - ADC conversion: < 0.1 ms (already done by module)
     *   - Processing: < 0.1 ms per channel
     */

    if (module == NULL || raw_values == NULL || num_values == 0) {
        return -1;
    }

    if (module->state != ECS700_IO_MODULE_ONLINE &&
        module->state != ECS700_IO_MODULE_DEGRADED) {
        return -2;  /* Module not ready for scanning */
    }

    int channels_scanned = 0;
    uint8_t fail_count = 0;

    for (uint8_t i = 0; i < num_values && i < module->num_channels; i++) {
        ecs700_io_channel_t *ch = &module->channels[i];

        if (!ch->enabled) {
            continue;  /* Skip disabled channels */
        }

        /* Process the raw ADC reading through the signal chain */
        ecs700_io_process_input(ch, raw_values[i], time_us);
        channels_scanned++;

        if (ch->quality == ECS700_IO_QUALITY_BAD) {
            fail_count++;
        }
    }

    /* Update module statistics */
    module->last_scan_time = time_us;
    module->scan_count++;

    /* Update module state based on channel fault count */
    if (fail_count == 0) {
        module->state = ECS700_IO_MODULE_ONLINE;
    } else if (fail_count < module->num_enabled_channels) {
        module->state = ECS700_IO_MODULE_DEGRADED;
    } else if (fail_count >= module->num_enabled_channels) {
        module->state = ECS700_IO_MODULE_FAULT;
        module->scan_errors++;
    }

    return channels_scanned;
}

uint8_t ecs700_io_module_fault_count(const ecs700_io_module_t *module)
{
    if (module == NULL) {
        return 0;
    }

    uint8_t faults = 0;

    for (uint8_t i = 0; i < module->num_channels; i++) {
        if (module->channels[i].enabled &&
            module->channels[i].quality == ECS700_IO_QUALITY_BAD) {
            faults++;
        }
    }

    return faults;
}

double ecs700_sbus_cycle_time_estimate(uint8_t num_modules,
                                        double scan_us_per_module)
{
    /**
     * SBUS bus cycle time estimation:
     *
     * Total cycle = modules * (scan_time_per_module + protocol_overhead)
     *
     * Protocol overhead:
     *   - Address polling: ~200 μs per module
     *   - Data transfer: ~100 μs per 8 channels
     *   - Turnaround: ~50 μs
     *   - Total overhead: ~350 μs per module
     *
     * At 1 Mbps RS-485 with 32 modules:
     *   Cycle ≈ 32 * (2000 + 350) = 75,200 μs ≈ 75 ms
     *
     * This must fit within the CS scan period (200 ms typical),
     * leaving 125 ms for control logic execution.
     */
    const double protocol_overhead_us = 350.0;

    if (num_modules == 0) {
        return 0.0;
    }

    return (double)num_modules * (scan_us_per_module + protocol_overhead_us);
}

/* ============================================================================
 * L3: Signal Processing — CJC, Square Root, RTD
 * ============================================================================
 */

double ecs700_io_cjc_compensate(double tc_voltage_mv, double cjc_temp_c,
                                 int tc_type)
{
    /**
     * Thermocouple Cold Junction Compensation:
     *
     * Thermocouples measure the temperature DIFFERENCE between
     * the hot junction (measurement point) and the cold junction
     * (where TC wires connect to copper on the I/O module).
     *
     * The measured voltage V_measured = V(T_hot) - V(T_cold)
     *
     * To get the actual temperature:
     *   1. Convert CJC temperature to equivalent voltage: V_cjc = f(T_cold)
     *   2. Add to measured voltage: V_total = V_measured + V_cjc
     *   3. Convert V_total to hot junction temperature: T_hot = f⁻¹(V_total)
     *
     * This function provides simplified linear approximation for
     * common TC types. Full implementation uses NIST ITS-90
     * polynomial coefficients (8th-order for type K).
     *
     * Approximate Seebeck coefficients (μV/°C) at 25°C:
     *   Type K (Chromel-Alumel):  40.6 μV/°C  (most common)
     *   Type J (Iron-Constantan): 51.7 μV/°C
     *   Type T (Copper-Constantan): 40.9 μV/°C
     *   Type E (Chromel-Constantan): 60.9 μV/°C
     *   Type N (Nicrosil-Nisil):  26.2 μV/°C
     *   Type R (Pt13%Rh-Pt):       5.8 μV/°C
     *   Type S (Pt10%Rh-Pt):       5.4 μV/°C
     *   Type B (Pt30%Rh-Pt6%Rh):   0.1 μV/°C (at 25°C, much lower)
     */

    /* Seebeck coefficients in μV/°C (approximate at 25°C) */
    static const double seebeck_coeff[] = {
        40.6,  /* Type K */
        51.7,  /* Type J */
        40.9,  /* Type T */
        60.9,  /* Type E */
        26.2,  /* Type N */
        5.8,   /* Type R */
        5.4,   /* Type S */
        0.2    /* Type B */
    };

    if (tc_type < 0 || tc_type > 7) {
        return cjc_temp_c;  /* Unknown TC type: return CJC temp as best guess */
    }

    double coeff = seebeck_coeff[tc_type];

    if (fabs(coeff) < 1e-9) {
        return 0.0;  /* Effectively no signal */
    }

    /* Convert CJC temperature to equivalent microvolts */
    double v_cjc_uv = coeff * cjc_temp_c;

    /* Add to measured voltage (convert mV to μV) */
    double v_total_uv = tc_voltage_mv * 1000.0 + v_cjc_uv;

    /* Convert back to temperature using linear approximation */
    double temperature_c = v_total_uv / coeff;

    return temperature_c;
}

double ecs700_io_sqrt_extract(double dp_percent, double cut_off_percent)
{
    /**
     * Square-Root Extraction for Flow Measurement:
     *
     * Differential pressure flow measurement (orifice plate, Venturi,
     * pitot tube) uses the Bernoulli principle:
     *
     *   Q = C * sqrt(ΔP / ρ)
     *
     * where Q = volumetric flow rate, C = discharge coefficient,
     * ΔP = differential pressure, ρ = fluid density.
     *
     * When ΔP is expressed as % of transmitter span:
     *   Flow[%] = 100 * sqrt(ΔP[%] / 100)
     *
     * Low-flow cut-off:
     * At low flows (< 5-10% of max), the square root relationship
     * becomes highly nonlinear and noisy (small ΔP errors cause
     * large flow errors). The cut-off forces flow = 0 below a
     * configurable threshold.
     *
     * Typical cut-off settings:
     *   - Orifice plate: 0.5-2% of ΔP span
     *   - Custody transfer: 1% (API MPMS Chapter 21.1)
     *   - General process: 2%
     */

    if (dp_percent <= 0.0) {
        return 0.0;
    }

    /* Apply low-flow cut-off */
    if (dp_percent < cut_off_percent) {
        return 0.0;
    }

    /* Square root with linear region for low flows
     * (prevents infinite gain near zero):
     *   If ΔP < 0.1%: flow = 3.16 * ΔP (linear approximation)
     *   Otherwise: flow = 10 * sqrt(ΔP)
     */
    if (dp_percent < 0.1) {
        /* Linear region: smooth transition
         * At ΔP=0.1%: sqrt(0.1) = 0.316 → flow = 3.16%
         * Gain = 3.16/0.1 = 31.6 */
        return 31.62 * dp_percent;
    }

    /* Normal square root extraction */
    return 10.0 * sqrt(dp_percent);
}

double ecs700_io_rtd_to_temp(double resistance_ohm)
{
    /**
     * Pt100 RTD to Temperature Conversion (IEC 60751):
     *
     * Callendar-Van Dusen Equation:
     *
     * For -200°C ≤ t < 0°C:
     *   R_t = R₀[1 + A*t + B*t² + C*(t - 100)*t³]
     *
     * For 0°C ≤ t ≤ 850°C:
     *   R_t = R₀(1 + A*t + B*t²)
     *
     * IEC 60751 Standard Coefficients (α = 0.00385055):
     *   R₀ = 100.0 Ω  (at 0°C)
     *   A  = 3.9083 × 10⁻³ °C⁻¹
     *   B  = -5.775 × 10⁻⁷ °C⁻²
     *   C  = -4.183 × 10⁻¹² °C⁻⁴
     *
     * For t ≥ 0°C: solve quadratic R_t = R₀(1 + At + Bt²)
     *   B*t² + A*t + (1 - R_t/R₀) = 0
     *   t = [-A + sqrt(A² - 4B(1 - R_t/R₀))] / (2B)
     *
     * For t < 0°C: the C term makes it a 4th-order polynomial.
     * We use iterative Newton-Raphson for this case.
     */

    const double R0 = 100.0;
    const double A  = 3.9083e-3;
    const double B  = -5.775e-7;
    const double C  = -4.183e-12;

    /* Check for valid resistance range:
     * Pt100 at -200°C ≈ 18.52 Ω
     * Pt100 at 850°C  ≈ 390.48 Ω */
    if (resistance_ohm < 18.0 || resistance_ohm > 400.0) {
        return 0.0;  /* Out of range */
    }

    /* First try the quadratic solution for t ≥ 0°C */
    double ratio = resistance_ohm / R0;
    double discriminant = A * A - 4.0 * B * (1.0 - ratio);

    if (discriminant >= 0.0) {
        /* Quadratic solution */
        double t_quad = (-A + sqrt(discriminant)) / (2.0 * B);

        /* Check if solution is in valid range for quadratic (t ≥ 0°C) */
        if (t_quad >= -1.0 && t_quad <= 850.0) {
            /* Verify: recompute resistance at this temperature */
            double r_check = R0 * (1.0 + A * t_quad + B * t_quad * t_quad);
            if (resistance_ohm > 100.0) {
                return t_quad;  /* Valid for t ≥ 0°C */
            }
            if (fabs(r_check - resistance_ohm) < 0.1) {
                return t_quad;
            }
        }
    }

    /* For t < 0°C: Newton-Raphson iteration
     *
     * f(t) = R₀[1 + At + Bt² + C(t-100)t³] - R = 0
     * f'(t) = R₀[A + 2Bt + C(4t³ - 300t²)]
     *
     * Start from t = -100°C (typical mid-range for sub-zero) */
    double t = -100.0;

    for (int iter = 0; iter < 50; iter++) {
        double t100 = t - 100.0;
        double r_computed = R0 * (1.0 + A * t + B * t * t + C * t100 * t * t * t);
        double residual = r_computed - resistance_ohm;

        if (fabs(residual) < 0.001) {
            return t;  /* Converged */
        }

        /* Derivative */
        double dR_dt = R0 * (A + 2.0 * B * t + C * (4.0 * t * t * t - 300.0 * t * t));

        if (fabs(dR_dt) < 1e-12) {
            break;  /* Avoid division by zero */
        }

        /* Newton step */
        double delta = residual / dR_dt;
        t = t - delta;

        /* Clamp to valid temperature range */
        if (t < -200.0) {
            t = -200.0;
        } else if (t > 850.0) {
            t = 850.0;
        }

        if (fabs(delta) < 0.0001) {
            return t;
        }
    }

    /* Fallback: linear approximation R ≈ R₀(1 + A*t) */
    return (resistance_ohm / R0 - 1.0) / A;
}

double ecs700_io_eu_to_transmitter_ma(double eu_value,
                                       double eu_lo, double eu_hi)
{
    /**
     * Convert EU value to NAMUR-compliant 4-20 mA output.
     *
     * Standard mapping:
     *   4.0 mA = 0%   (EU = eu_lo)
     *   20.0 mA = 100% (EU = eu_hi)
     *
     * Linear interpolation:
     *   mA = 4 + 16 * (EU - eu_lo) / (eu_hi - eu_lo)
     *
     * NAMUR NE43 over/underrange:
     *   Normal:  3.8 – 20.5 mA
     *   Fault low:  ≤ 3.6 mA  (configurable burnout direction)
     *   Fault high: ≥ 21.0 mA
     */

    if (eu_hi <= eu_lo) {
        return 4.0;  /* Degenerate range: return 0% */
    }

    double fraction = (eu_value - eu_lo) / (eu_hi - eu_lo);

    /* Clamp to [0, 1] for normal range, but allow slight over/under */
    if (fraction < -0.025) {
        /* Below -2.5%: fault low (burnout down) */
        return 3.6;
    } else if (fraction > 1.025) {
        /* Above 102.5%: fault high (burnout up) */
        return 21.0;
    }

    /* Normal range: 4 + 16 * fraction */
    return 4.0 + 16.0 * fraction;
}

/* ============================================================================
 * L4: Engineering Laws — Accuracy and Signal Quality
 * ============================================================================
 */

double ecs700_io_accuracy_pct_span(double measured, double reference,
                                    double span)
{
    /**
     * Measurement accuracy as % of span:
     *
     *   Accuracy = |measured - reference| / span * 100%
     *
     * This is the standard way to express industrial instrument
     * accuracy (per IEC 61298-1).
     *
     * Example:
     *   Temperature transmitter, span = 0-200°C
     *   Reference = 100.0°C, Measured = 100.3°C
     *   Accuracy = |0.3| / 200 * 100 = 0.15% of span
     *
     * ECS-700 AI module specification:
     *   ±0.1% of span at 25°C ambient
     *   ±0.2% of span over full temperature range
     */

    if (span <= 0.0) {
        return 0.0;
    }

    double error = fabs(measured - reference);
    return (error / span) * 100.0;
}

double ecs700_io_compute_snr(double signal_amplitude, double noise_rms)
{
    /**
     * Signal-to-Noise Ratio:
     *
     *   SNR_dB = 20 * log10(V_signal / V_noise_rms)   [for voltage/current]
     *
     * Required SNR for various accuracy levels:
     *   ±1%:    SNR ≥ 40 dB
     *   ±0.1%:  SNR ≥ 60 dB
     *   ±0.01%: SNR ≥ 80 dB
     *
     * For a 16-bit ADC with ideal characteristics:
     *   Quantization noise RMS = LSB / sqrt(12)
     *   = (20V/65536) / 3.464 = 88 μV
     *
     *   SNR_quantization = 20 * log10(10V / 88μV) = 101 dB
     *
     * In practice, thermal noise and EMI reduce this to 80-90 dB.
     */

    if (noise_rms <= 0.0) {
        return 200.0;  /* Infinite SNR (ideal) */
    }
    if (signal_amplitude <= 0.0) {
        return -200.0;  /* No signal */
    }

    double ratio = signal_amplitude / noise_rms;
    double snr_db = 20.0 * log10(ratio);

    return snr_db;
}

double ecs700_io_enob(double snr_db)
{
    /**
     * Effective Number of Bits (ENOB):
     *
     *   ENOB = (SNR_dB - 1.76) / 6.02
     *
     * This formula comes from the theoretical SNR of an ideal N-bit ADC:
     *   SNR_ideal = 6.02*N + 1.76 dB
     *
     * Solving for N gives ENOB.
     *
     * Example:
     *   16-bit ADC with SNR = 85 dB:
     *   ENOB = (85 - 1.76) / 6.02 = 13.8 bits
     *
     * This means the ADC effectively has about 14 bits of resolution
     * after accounting for noise. The "lost" 2 bits represent the
     * effective resolution penalty from real-world noise sources.
     *
     * Industrial implications:
     *   - ENOB < 12: Not suitable for precision measurement
     *   - ENOB 12-14: Good for general process control
     *   - ENOB > 14: Excellent, suitable for custody transfer
     */

    if (snr_db < 1.76) {
        return 0.0;  /* SNR too low for even 1 bit */
    }

    double enob = (snr_db - 1.76) / 6.02;

    /* Clamp: ENOB cannot exceed physical ADC resolution */
    if (enob > 16.0) {
        enob = 16.0;  /* Max 16-bit ADC */
    }

    return enob;
}
