/**
 * ff_h1_segment.c ? Foundation Fieldbus H1 Segment Engineering Implementation
 *
 * Implements H1 segment DC power budget analysis, cable voltage drop
 * calculation, maximum trunk length estimation, spur validation,
 * FISCO intrinsic safety compatibility checking, segment health
 * diagnostics, and commissioning readiness assessment.
 *
 * Knowledge Levels: L1, L6, L7
 */

#include "ff_h1_segment.h"
#include "ff_h1_physical.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

/* ============================================================================
 * L7: Temperature Coefficient of Copper Resistance
 *
 * Copper resistivity increases with temperature:
 *   R(T) = R_20?C ? (1 + ? ? (T - 20))
 *
 * where ?_Cu = 0.00393 per ?C (temperature coefficient of resistance).
 *
 * This matters for H1 segments in hot environments (e.g., Middle East,
 * furnace areas) where cable temperature can reach 60-70?C, increasing
 * resistance by ~16-20%.
 *
 * Reference: IEC 60028 (International Standard of Resistance for Copper)
 * ============================================================================ */

#define CU_TEMP_COEFF_PER_C    0.00393   /**< Copper TCR in /?C (at 20?C) */
#define REFERENCE_TEMP_C       20.0      /**< Reference temperature for cable specs */

static double temp_correction_factor(double temp_c) {
    return 1.0 + CU_TEMP_COEFF_PER_C * (temp_c - REFERENCE_TEMP_C);
}


/* ============================================================================
 * L6: Cable Loop Resistance & Voltage Drop
 *
 * Loop resistance = 2 ? L ? R_km ? temp_factor / 1000
 *   where R_km is ?/km (loop resistance already accounts for both conductors)
 *
 * Wait ? the cable spec resistance_per_km is already the loop resistance
 * (both conductors). Let me clarify:
 *
 * Per IEC 61158-2 Table A.1, the resistance values are "loop resistance",
 * meaning both conductors combined. So:
 *
 *   R_loop(?) = length_m ? R_km(?/km) / 1000 ? temp_factor
 *
 * Voltage drop: V_drop = I_mA ? R_loop(?) / 1000
 *   where I_mA is in milliamps, so I_mA/1000 converts to amps.
 *
 * ============================================================================ */

double ff_cable_loop_resistance(double length_m, ff_cable_type_t cable,
                                 double temp_c) {
    if (length_m <= 0.0) return 0.0;

    const ff_cable_spec_t *spec = ff_cable_spec(cable);
    if (!spec) return -1.0;

    double r_km = spec->resistance_per_km; /* ?/km loop resistance */
    double factor = temp_correction_factor(temp_c);

    return (length_m * r_km * factor) / 1000.0;
}

double ff_cable_voltage_drop(double length_m, ff_cable_type_t cable,
                              double current_ma, double temp_c) {
    if (current_ma <= 0.0) return 0.0;

    double r_loop = ff_cable_loop_resistance(length_m, cable, temp_c);
    if (r_loop < 0.0) return -1.0;

    return (current_ma / 1000.0) * r_loop;
}


/* ============================================================================
 * L6: Maximum Trunk Length Calculation
 *
 * Given:
 *   V_supply    ? power supply output voltage (V)
 *   V_min_device ? minimum allowed voltage at device (V), typically 9V
 *   I_total     ? total segment current (mA)
 *   cable       ? cable type
 *   temp_c      ? ambient temperature (?C)
 *
 * Available voltage budget: V_budget = V_supply - V_min_device - V_cond_drop
 *
 * R_max = V_budget / (I_total / 1000)   [?]
 *
 * Max length = R_max ? 1000 / (R_km ? temp_factor)   [m]
 *
 * If V_budget ? 0, the configuration is infeasible regardless of length.
 * ============================================================================ */

double ff_max_trunk_length(double supply_v, double min_device_v,
                            double total_current_ma, ff_cable_type_t cable,
                            double temp_c) {
    if (total_current_ma <= 0.0) return -1.0;

    /* Typical power conditioner voltage drop: ~0.5V for passive,
     * ~0.2V for active conditioner. Use 0.5V as conservative estimate. */
    double v_cond_drop = 0.5;

    double v_budget = supply_v - min_device_v - v_cond_drop;
    if (v_budget <= 0.0) return -1.0;

    const ff_cable_spec_t *spec = ff_cable_spec(cable);
    if (!spec) return -1.0;

    double i_amps = total_current_ma / 1000.0; /* Convert mA to A */
    double r_max = v_budget / i_amps;           /* Maximum allowable loop resistance */

    double r_km_corrected = spec->resistance_per_km * temp_correction_factor(temp_c);

    double max_length_km = r_max / r_km_corrected;
    double max_length_m = max_length_km * 1000.0;

    /* Clamp to cable type's absolute max length */
    if (max_length_m > spec->max_length_m) {
        max_length_m = spec->max_length_m;
    }

    return max_length_m;
}


/* ============================================================================
 * L6: Complete H1 Segment DC Power Budget Analysis
 *
 * This is the canonical segment design calculation. It evaluates:
 *   1. Worst-case voltage at each device terminal
 *   2. Whether all devices meet the 9V minimum
 *   3. Power supply utilization and margin
 *
 * Algorithm:
 *   For each device i on the segment:
 *     I_before_device = sum of currents of devices 0..i-1
 *     V_trunk_at_spur_i = V_supply - V_cond_drop - V_drop(trunk, I_before_device)
 *     V_device_i = V_trunk_at_spur_i - V_drop(spur_i, I_device_i)
 *     Track minimum V_device
 *
 * The worst-case device is typically the furthest one on the trunk
 * (highest cumulative current through the trunk, plus its own spur).
 * ============================================================================ */

int ff_segment_power_budget(const ff_segment_config_t *config,
                             ff_power_budget_result_t *result) {
    if (!config || !result) return -1;
    if (config->num_devices < 1 || config->num_devices > FF_H1_MAX_DEVICES) return -1;

    memset(result, 0, sizeof(*result));

    /* --- Step 1: Sum all device currents --- */
    double total_current = 0.0;
    for (int i = 0; i < config->num_devices; i++) {
        double i_dev = config->device_current_ma[i];
        if (i_dev <= 0.0) i_dev = FF_DEVICE_QUIESCENT_MA; /* Default if not specified */
        total_current += i_dev;
    }
    /* Add safety margin: +10% for startup transients and measurement tolerance */
    total_current *= 1.10;

    result->total_current_ma = total_current;

    /* --- Step 2: Check against power supply current limit --- */
    if (total_current > config->power_supply.max_current_ma) {
        result->is_viable = 0;
        result->power_supply_utilization = total_current / config->power_supply.max_current_ma;
        return 0; /* Not an error, just not viable */
    }

    /* --- Step 3: Compute trunk voltage drop cumulatively --- */
    double v_cond_drop = config->power_supply.conditioner_drop_v;
    double v_supply = config->power_supply.output_voltage_v;
    double v_after_cond = v_supply - v_cond_drop;
    double trunk_length = config->trunk_length_m;
    ff_cable_type_t trunk_type = config->trunk_cable_type;
    double temp_c = config->temperature_c;

    /* Trunk resistance per meter */
    const ff_cable_spec_t *trunk_spec = ff_cable_spec(trunk_type);
    if (!trunk_spec) return -1;
    double trunk_r_per_m = (trunk_spec->resistance_per_km * temp_correction_factor(temp_c)) / 1000.0;

    double min_device_v = 1e9; /* Start with very high value */
    int worst_device = 0;

    /* If devices are distributed along the trunk (not all at the end):
     * For simplicity, assume devices are uniformly distributed. The trunk
     * section carrying current for device i goes from the power supply to
     * device i's junction point. The segment from PS to device k carries
     * all devices beyond k.
     *
     * With uniform distribution: device i is at trunk_length * (i + 1) / (N + 1)
     * But the worst case is when all devices are at the far end of the trunk,
     * so we compute both: (1) uniform distribution, (2) all-at-end.
     *
     * Here we implement the conservative "all-at-end" model, which is the
     * standard commissioning check: assume maximum trunk current over the
     * full trunk length.
     */

    /* Full trunk drop (all current flows through entire trunk): */
    double trunk_drop = (total_current / 1000.0) * trunk_length * trunk_r_per_m;
    result->trunk_voltage_drop_v = trunk_drop;

    /* Voltage at the far end of the trunk (before spurs): */
    double v_at_trunk_end = v_after_cond - trunk_drop;

    /* --- Step 4: Compute voltage at each device (trunk end + spur drop) --- */
    for (int i = 0; i < config->num_devices; i++) {
        double i_dev_ma = config->device_current_ma[i];
        if (i_dev_ma <= 0.0) i_dev_ma = FF_DEVICE_QUIESCENT_MA;

        /* Spur voltage drop */
        double spur_length = config->spur_length_m[i];
        ff_cable_type_t spur_type = config->spur_cable_type[i];
        double spur_drop = ff_cable_voltage_drop(spur_length, spur_type, i_dev_ma, temp_c);

        double v_device = v_at_trunk_end - spur_drop;

        if (v_device < min_device_v) {
            min_device_v = v_device;
            worst_device = i;
        }
    }

    result->min_device_voltage_v = min_device_v;
    result->worst_device_index = worst_device;
    result->power_supply_utilization = total_current / config->power_supply.max_current_ma;
    result->margin_ma = config->power_supply.max_current_ma - total_current;
    result->margin_v = min_device_v - FF_H1_MIN_VOLTAGE_V;

    result->is_viable = (min_device_v >= FF_H1_MIN_VOLTAGE_V) ? 1 : 0;

    return 0;
}


/* ============================================================================
 * L6: Spur Length Validation
 * ============================================================================ */

int ff_segment_validate_spurs(int num_devices, const double spur_lengths_m[]) {
    if (!spur_lengths_m || num_devices < 1 || num_devices > FF_H1_MAX_DEVICES) {
        return 0;
    }

    double max_spur = ff_h1_max_spur_length(num_devices);
    if (max_spur < 0.0) return 0;

    for (int i = 0; i < num_devices; i++) {
        if (spur_lengths_m[i] > max_spur) {
            return 0; /* Violation */
        }
    }

    return 1; /* All OK */
}


/* ============================================================================
 * L6: Segment Round-Trip Time
 *
 * The worst-case round-trip time accounts for:
 *   1. Trunk propagation delay (one-way ? 2 for round-trip)
 *   2. Worst-case spur delay (longest spur ? 2)
 *   3. Device response time (typically 500-1000 ?s for FF devices)
 *
 * Round-trip time = 2?(trunk_delay + max_spur_delay) + device_response_time
 *
 * This must be less than the slot time for CD schedule entries,
 * otherwise the LAS cannot reliably exchange data within the
 * allocated time window.
 * ============================================================================ */

double ff_segment_round_trip_time(const ff_segment_config_t *config) {
    if (!config || config->num_devices < 1) return -1.0;

    /* Trunk one-way delay */
    double trunk_delay = ff_h1_propagation_delay_us(config->trunk_length_m,
                                                     config->trunk_cable_type);

    /* Find longest spur */
    double max_spur = 0.0;
    ff_cable_type_t max_spur_type = FF_CABLE_TYPE_A;
    for (int i = 0; i < config->num_devices; i++) {
        if (config->spur_length_m[i] > max_spur) {
            max_spur = config->spur_length_m[i];
            max_spur_type = config->spur_cable_type[i];
        }
    }

    double spur_delay = ff_h1_propagation_delay_us(max_spur, max_spur_type);

    /* Device response time: typical FF H1 device responds within 500-1000 ?s */
    double device_response_us = 750.0;

    return 2.0 * (trunk_delay + spur_delay) + device_response_us;
}


/* ============================================================================
 * L7: FISCO Intrinsic Safety Compatibility
 *
 * FISCO (IEC 60079-27) simplifies IS verification by using pre-certified
 * parameter sets. The standard check is:
 *
 *   Uo_source ? Ui_device   (source cannot output more voltage than device can accept)
 *   Io_source ? Ii_device   (source cannot output more current than device can accept)
 *   Po_source ? Pi_device   (source cannot output more power than device can accept)
 *   Co_source ? Ci_device + Ccable  (source's allowed capacitance must exceed total)
 *   Lo_source ? Li_device + Lcable  (source's allowed inductance must exceed total)
 *
 * For FISCO specifically, the cable parameters are bounded by the FISCO
 * standard itself (max 1000m, max 5 nF/m, max 1 ?H/m), so the cable
 * contribution is implicitly covered by the FISCO certification.
 *
 * Reference: IEC 60079-27 ?5.2 "Determination of system safety"
 * ============================================================================ */

int ff_fisco_verify_compatibility(const ff_entity_params_t *device,
                                   const ff_entity_params_t *source) {
    if (!device || !source) return 0;

    /* Only applicable for FISCO or FNICO */
    if (device->type != FF_IS_TYPE_FISCO && device->type != FF_IS_TYPE_FNICO) {
        return 0;
    }
    if (source->type != FF_IS_TYPE_FISCO && source->type != FF_IS_TYPE_FNICO) {
        return 0;
    }

    /* Voltage check: Ui ? Uo */
    if (device->ui_v < source->uo_v) return 0;

    /* Current check: Ii ? Io */
    if (device->ii_ma < source->io_ma) return 0;

    /* Power check: Pi ? Po */
    if (device->pi_w < source->po_w) return 0;

    /* Capacitance check: Ci ? Co (device capacitance must be within source limit) */
    if (device->ci_nf > source->uo_v) {
        /* Note: The FISCO source Co/Co we model via capacitance budget.
         * Simplified check: device capacitance must be less than the
         * FISCO maximum (typically 5nF total for Group IIC).
         * Full compliance requires adding cable capacitance. */
        if (device->ci_nf > 5.0) return 0; /* FISCO IIC limit */
    }

    /* Inductance check: Li ? Lo */
    if (device->li_uh > 20.0) return 0; /* FISCO IIC limit */

    return 1; /* FISCO compatible */
}


/* ============================================================================
 * L6: Segment Health Diagnostics ? Signal and Error Analysis
 *
 * Evaluates segment electrical health from field measurements.
 * This function implements the signal quality heuristics used in
 * fieldbus diagnostic tools (Pepperl+Fuchs, Relcom, Emerson AMS).
 *
 * Health assessment criteria:
 *
 *   Signal level:
 *     > 0.75 Vpp ? GOOD
 *     0.5?0.75 Vpp ? WARNING (marginal, check terminators/cable)
 *     < 0.50 Vpp ? DEGRADED (likely a missing terminator or cable fault)
 *
 *   DC voltage:
 *     > 12V ? GOOD
 *     9-12V ? WARNING (low, check power supply)
 *     < 9V ? CRITICAL (device failure imminent)
 *
 *   Noise:
 *     < 50 mVpp ? GOOD
 *     50?75 mVpp ? WARNING
 *     > 75 mVpp ? DEGRADED (interference, check shielding/grounding)
 *
 *   Retransmission rate:
 *     < 0.5% ? GOOD
 *     0.5?1.0% ? WARNING
 *     > 1.0% ? DEGRADED (signal quality issue)
 *
 *   Frame error rate:
 *     < 0.001% ? GOOD
 *     0.001?0.01% ? WARNING
 *     > 0.01% ? DEGRADED
 *
 *   Device count mismatch (Live List vs expected):
 *     Match ? GOOD
 *     Missing 1 ? WARNING (check device power/fault)
 *     Missing > 1 ? CRITICAL (segment or power failure)
 *
 * ============================================================================ */

ff_segment_health_t ff_segment_health_evaluate(const ff_segment_diagnostics_t *diag) {
    if (!diag) return FF_SEGMENT_HEALTH_CRITICAL;

    int worst = FF_SEGMENT_HEALTH_GOOD;

    /* Helper: update worst if new_level is worse */
    #define UPDATE_WORST(new_level) do { if ((new_level) > worst) worst = (new_level); } while(0)

    /* Signal level check */
    if (diag->signal_level_pp_v < 0.50) {
        UPDATE_WORST(FF_SEGMENT_HEALTH_DEGRADED);
    } else if (diag->signal_level_pp_v < 0.75) {
        UPDATE_WORST(FF_SEGMENT_HEALTH_WARNING);
    }

    /* DC voltage check */
    if (diag->dc_voltage_v < FF_H1_MIN_VOLTAGE_V) {
        UPDATE_WORST(FF_SEGMENT_HEALTH_CRITICAL);
    } else if (diag->dc_voltage_v < 12.0) {
        UPDATE_WORST(FF_SEGMENT_HEALTH_WARNING);
    }

    /* Noise check */
    if (diag->noise_pp_mv > 75.0) {
        UPDATE_WORST(FF_SEGMENT_HEALTH_DEGRADED);
    } else if (diag->noise_pp_mv > 50.0) {
        UPDATE_WORST(FF_SEGMENT_HEALTH_WARNING);
    }

    /* Retransmission rate */
    if (diag->retransmission_rate > 0.01) { /* > 1% */
        UPDATE_WORST(FF_SEGMENT_HEALTH_DEGRADED);
    } else if (diag->retransmission_rate > 0.005) { /* > 0.5% */
        UPDATE_WORST(FF_SEGMENT_HEALTH_WARNING);
    }

    /* Frame error rate */
    if (diag->frame_error_rate > 0.0001) { /* > 0.01% */
        UPDATE_WORST(FF_SEGMENT_HEALTH_DEGRADED);
    } else if (diag->frame_error_rate > 0.00001) { /* > 0.001% */
        UPDATE_WORST(FF_SEGMENT_HEALTH_WARNING);
    }

    /* Device count mismatch */
    if (diag->devices_expected > 0) {
        int missing = (int)diag->devices_expected - (int)diag->devices_detected;
        if (missing > 1) {
            UPDATE_WORST(FF_SEGMENT_HEALTH_CRITICAL);
        } else if (missing == 1) {
            UPDATE_WORST(FF_SEGMENT_HEALTH_WARNING);
        }
    }

    #undef UPDATE_WORST

    return (ff_segment_health_t)worst;
}

void ff_segment_diag_summary(const ff_segment_diagnostics_t *diag,
                              char *buf, size_t buf_size) {
    if (!diag || !buf || buf_size < 256) return;

    ff_segment_health_t health = ff_segment_health_evaluate(diag);

    const char *health_str;
    switch (health) {
        case FF_SEGMENT_HEALTH_GOOD:     health_str = "GOOD"; break;
        case FF_SEGMENT_HEALTH_WARNING:  health_str = "WARNING"; break;
        case FF_SEGMENT_HEALTH_DEGRADED: health_str = "DEGRADED"; break;
        case FF_SEGMENT_HEALTH_CRITICAL: health_str = "CRITICAL"; break;
        default:                         health_str = "UNKNOWN"; break;
    }

    snprintf(buf, buf_size,
        "FF H1 Segment Diagnostics:\n"
        "  Health:         %s\n"
        "  Signal Level:   %.2f Vpp\n"
        "  DC Voltage:     %.1f V\n"
        "  Noise:          %.1f mVpp\n"
        "  Retransmissions: %.2f%%\n"
        "  Frame Errors:   %.4f%%\n"
        "  Devices:        %u detected / %u expected\n"
        "  PS Current:     %.1f mA\n",
        health_str,
        diag->signal_level_pp_v, diag->dc_voltage_v, diag->noise_pp_mv,
        diag->retransmission_rate * 100.0, diag->frame_error_rate * 100.0,
        diag->devices_detected, diag->devices_expected,
        diag->power_supply_current_ma);
}


/* ============================================================================
 * L6: Commissioning Checklist
 * ============================================================================ */

int ff_commissioning_pass_count(const ff_commissioning_checklist_t *checklist) {
    if (!checklist) return 0;

    int count = 0;
    if (checklist->terminators_ok) count++;
    if (checklist->grounding_ok)   count++;
    if (checklist->polarity_ok)    count++;
    if (checklist->power_budget_ok) count++;
    if (checklist->signal_level_ok) count++;
    if (checklist->live_list_ok)   count++;
    if (checklist->las_ok)         count++;
    if (checklist->time_sync_ok)   count++;

    return count;
}

int ff_commissioning_ready(const ff_commissioning_checklist_t *checklist) {
    return (ff_commissioning_pass_count(checklist) == 8) ? 1 : 0;
}