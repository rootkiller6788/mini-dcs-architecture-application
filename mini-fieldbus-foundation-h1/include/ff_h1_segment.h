/**
 * ff_h1_segment.h ? Foundation Fieldbus H1 Segment Engineering
 *
 * Implements H1 segment design calculations: DC power budget analysis,
 * voltage drop computation, cable length limits, spur configuration,
 * intrinsic safety (FISCO/FNICO) models, and segment commissioning checks.
 *
 * Course Mapping:
 *   MIT 2.171   ? Physical system design, power distribution
 *   RWTH Aachen ? Feldbus-Engineering, explosionsschutz
 *   ISA/IEC     ? IEC 60079-11 (IS), IEC 60079-27 (FISCO), IEC 61158-2 Annex A
 *   Cambridge   ? Process control infrastructure design
 *
 * Knowledge Levels: L1 (Definitions), L6 (Canonical Problems), L7 (Industrial Apps)
 */

#ifndef FF_H1_SEGMENT_H
#define FF_H1_SEGMENT_H

#include <stdint.h>
#include <stddef.h>
#include "ff_h1_physical.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Power Supply / Power Conditioner Definitions
 *
 * The H1 power supply provides DC voltage to the bus through a power
 * conditioner (impedance network) that prevents the power supply's low
 * output impedance from shorting the communication signal.
 *
 * Power Conditioner: a series inductor (or active circuit) that presents
 * high impedance (> 3 k?) at 31.25 kHz (the H1 bitrate) while allowing
 * DC current to flow. Without it, the power supply would absorb the
 * Manchester-encoded signal.
 * ============================================================================ */

/** Power conditioner: minimum impedance at 31.25 kHz (IEC 61158-2) */
#define FF_PS_MIN_IMPEDANCE_OHM    3000.0

/** Typical power supply output voltage */
#define FF_PS_OUTPUT_VOLTAGE       24.0

/** Maximum power supply output current (typical segment supply) */
#define FF_PS_MAX_CURRENT_MA       500.0

/** Typical device quiescent current draw */
#define FF_DEVICE_QUIESCENT_MA     20.0

/** Device inrush current factor (? quiescent during startup) */
#define FF_DEVICE_INRUSH_FACTOR    1.5


/* ============================================================================
 * L1: FISCO / FNICO ? Explosion Protection Models for Fieldbus
 *
 * FISCO (Fieldbus Intrinsically Safe Concept): IEC 60079-27.
 *   Allows multiple IS devices on a single IS barrier, using simplified
 *   entity parameter matching, provided all devices and the power supply
 *   are FISCO-certified.
 *
 * FNICO (Fieldbus Non-Incendive Concept): for Zone 2 / Division 2.
 *   Similar to FISCO but with relaxed energy limits.
 *
 * Entity parameters:
 *   Ui/Ii/Pi  ? maximum input voltage/current/power device can accept
 *   Uo/Io/Po  ? maximum output voltage/current/power the source can deliver
 *   Ci/Li      ? internal capacitance/inductance of device
 *   Co/Lo      ? maximum allowed external capacitance/inductance for source
 *
 * FISCO rules (simplified):
 *   - Up to 32 devices, max 1000m trunk, max 60m spurs
 *   - One active source only (power supply through IS barrier)
 *   - Terminators at both ends (may be integrated in PS and last device)
 * ============================================================================ */

/** Intrinsic safety type */
typedef enum {
    FF_IS_TYPE_NONE   = 0,  /**< Non-IS (general purpose) */
    FF_IS_TYPE_FISCO  = 1,  /**< FISCO (Zone 0/1, Division 1) */
    FF_IS_TYPE_FNICO  = 2,  /**< FNICO (Zone 2, Division 2) */
    FF_IS_TYPE_ENTITY = 3   /**< Traditional entity-parameter IS */
} ff_is_type_t;

/** Entity parameters for IS calculation */
typedef struct {
    ff_is_type_t type;
    double       ui_v;     /**< Max input voltage (V) */
    double       ii_ma;    /**< Max input current (mA) */
    double       pi_w;     /**< Max input power (W) */
    double       uo_v;     /**< Max output voltage (V) */
    double       io_ma;    /**< Max output current (mA) */
    double       po_w;     /**< Max output power (W) */
    double       ci_nf;    /**< Internal capacitance (nF) */
    double       li_uh;    /**< Internal inductance (?H) */
} ff_entity_params_t;

/**
 * FISCO entity parameter verification: check that a device (Ui/Ii/Pi/Ci/Li)
 * is compatible with a source (Uo/Io/Po/Co/Lo).
 *
 * FISCO check:
 *   Ui >= Uo, Ii >= Io, Pi >= Po
 *   Ci <= Co, Li <= Lo
 *
 * @param device  device entity parameters
 * @param source  source/power supply entity parameters
 * @return 1 if compatible, 0 otherwise
 *
 * Reference: IEC 60079-27 Section 5.2 "FISCO requirements"
 */
int ff_fisco_verify_compatibility(const ff_entity_params_t *device,
                                   const ff_entity_params_t *source);


/* ============================================================================
 * L6: H1 Segment Design ? DC Power Budget
 *
 * A segment designer must ensure every device receives at least 9V at its
 * terminals, accounting for cable resistance voltage drop.
 *
 *   V_device = V_supply - I_total * R_cable - V_conditioner_drop
 *
 * where:
 *   I_total = sum of all device quiescent currents + safety margin
 *   R_cable = (cable_length * cable_resistance_per_meter) / num_conductors
 *
 * This is the single most important calculation for H1 segment design.
 * An under-powered segment causes intermittent device failures and is
 * one of the most common commissioning problems.
 * ============================================================================ */

/** Power supply / conditioner model */
typedef struct {
    double       output_voltage_v;       /**< PS output voltage */
    double       max_current_ma;          /**< Maximum output current */
    double       conditioner_impedance_ohm; /**< Impedance at 31.25 kHz */
    double       conditioner_drop_v;      /**< DC voltage drop across conditioner */
    ff_is_type_t is_type;                 /**< IS type (if applicable) */
    ff_entity_params_t is_params;         /**< IS entity parameters */
} ff_ps_spec_t;

/** H1 segment configuration for power budget analysis */
typedef struct {
    ff_ps_spec_t    power_supply;
    ff_cable_type_t trunk_cable_type;
    double          trunk_length_m;
    int             num_devices;
    double          device_current_ma[32]; /**< Per-device quiescent current */
    ff_cable_type_t spur_cable_type[32];  /**< Per-device spur cable type */
    double          spur_length_m[32];     /**< Per-device spur length */
    double          temperature_c;         /**< Ambient temperature (affects cable resistance) */
} ff_segment_config_t;

/** Segment power budget analysis result */
typedef struct {
    double total_current_ma;           /**< Total segment current draw */
    double trunk_voltage_drop_v;       /**< Voltage drop on trunk (both conductors) */
    double min_device_voltage_v;       /**< Lowest voltage at any device terminal */
    int    worst_device_index;         /**< Index of device with lowest voltage (0-based) */
    double power_supply_utilization;   /**< Fraction of PS current capacity used */
    int    is_viable;                  /**< 1 if all devices get >= 9V */
    double margin_ma;                  /**< Remaining current capacity */
    double margin_v;                   /**< Worst-case voltage margin above 9V */
} ff_power_budget_result_t;

/**
 * Perform complete DC power budget analysis for an H1 segment.
 *
 * This is the canonical engineering calculation for H1 segment design.
 * Computes:
 *   1. Total current draw at supply point
 *   2. Voltage drop along trunk (both directions = 2 ? length ? R/km ? I)
 *   3. Voltage at each device terminal (trunk voltage - spur drop)
 *   4. Viability check: all devices >= 9V
 *
 * @param config  segment configuration
 * @param result  output analysis result
 * @return 0 on success, -1 on invalid config
 *
 * Reference: IEC 61158-2 Annex A "Segment design guidelines"
 * Complexity: O(n) in number of devices
 */
int ff_segment_power_budget(const ff_segment_config_t *config,
                             ff_power_budget_result_t *result);


/* ============================================================================
 * L6: Cable Voltage Drop Calculation
 *
 * Ohm's Law applied to bus-powered fieldbus:
 *
 *   V_drop = I ? R_loop
 *   R_loop = 2 ? length ? R_per_unit_length / (1 + ? ? (T - 20?C))
 *
 * where ? ? 0.00393/?C for copper (temperature coefficient of resistance).
 *
 * The factor of 2 accounts for both conductors (supply + return).
 * ============================================================================ */

/**
 * Compute loop resistance of a cable segment.
 *
 * @param length_m     cable length in meters
 * @param cable        cable type
 * @param temp_c       ambient temperature in Celsius
 * @return loop resistance in ohms
 */
double ff_cable_loop_resistance(double length_m, ff_cable_type_t cable,
                                 double temp_c);

/**
 * Compute voltage drop over a cable segment at given current.
 *
 * @param length_m     one-way length in meters
 * @param cable        cable type
 * @param current_ma   current draw in mA
 * @param temp_c       ambient temperature in Celsius
 * @return voltage drop in volts
 */
double ff_cable_voltage_drop(double length_m, ff_cable_type_t cable,
                              double current_ma, double temp_c);

/**
 * Compute the maximum allowable trunk length given:
 *   - Supply voltage, min device voltage, total segment current
 *   - Cable type and temperature
 *   - Allowable voltage drop budget
 *
 * This is the inverse of the power budget: given constraints, how long
 * can the trunk be?
 *
 * @return maximum trunk length in meters, or -1 if constraints infeasible
 */
double ff_max_trunk_length(double supply_v, double min_device_v,
                            double total_current_ma, ff_cable_type_t cable,
                            double temp_c);


/* ============================================================================
 * L6: Spur Topology Analysis
 *
 * Spurs connect individual devices to the trunk via junction boxes.
 * Spur length limits depend on the number of devices (IEC 61158-2):
 *
 *   Devices     Max Spur Length
 *   1?12        120 m
 *   13?14        90 m
 *   15?18        60 m
 *   19?24        30 m
 *   25?32         1 m
 *
 * These limits prevent signal reflections from degrading communication.
 * ============================================================================ */

/**
 * Validate all spur lengths against the limit for the given device count.
 *
 * @param num_devices     total devices on segment
 * @param spur_lengths_m  array of spur lengths
 * @return 1 if all spurs within limits, 0 if any violation
 */
int ff_segment_validate_spurs(int num_devices, const double spur_lengths_m[]);

/**
 * Compute worst-case signal round-trip time on the segment, including
 * trunk propagation delay and worst-case spur delay.
 *
 * @return round-trip time in microseconds
 */
double ff_segment_round_trip_time(const ff_segment_config_t *config);


/* ============================================================================
 * L6: Segment Health Diagnostics
 *
 * Real-time monitoring of segment electrical health:
 *   - Signal level (should be 0.75?1.0 V peak-to-peak)
 *   - DC voltage (should be > 9V at each device)
 *   - Noise level (should be < 75 mV peak-to-peak, per IEC 61158-2)
 *   - Retransmission rate (should be < 1%)
 *   - Frame error rate (should be < 0.01%)
 *
 * Reference: Relcom/Pepperl+Fuchs Fieldbus Diagnostic Guidelines
 * ============================================================================ */

typedef enum {
    FF_SEGMENT_HEALTH_GOOD        = 0,  /**< All parameters within spec */
    FF_SEGMENT_HEALTH_WARNING     = 1,  /**< Marginal: one parameter borderline */
    FF_SEGMENT_HEALTH_DEGRADED    = 2,  /**< Degraded: intermittent errors expected */
    FF_SEGMENT_HEALTH_CRITICAL    = 3   /**< Critical: segment failing */
} ff_segment_health_t;

/** Segment diagnostic measurements */
typedef struct {
    double signal_level_pp_v;     /**< Peak-to-peak signal amplitude */
    double dc_voltage_v;          /**< DC bus voltage at measurement point */
    double noise_pp_mv;           /**< Peak-to-peak noise */
    double retransmission_rate;   /**< DL retransmission ratio */
    double frame_error_rate;      /**< CRC-16 failure ratio */
    uint32_t devices_detected;    /**< Number of devices in Live List */
    uint32_t devices_expected;    /**< Expected device count */
    double power_supply_current_ma; /**< Actual PS current draw */
} ff_segment_diagnostics_t;

/**
 * Evaluate segment health from diagnostic measurements.
 *
 * @return segment health classification
 */
ff_segment_health_t ff_segment_health_evaluate(const ff_segment_diagnostics_t *diag);

/**
 * Generate a human-readable summary of segment diagnostics.
 * Output buffer must be at least 256 bytes.
 */
void ff_segment_diag_summary(const ff_segment_diagnostics_t *diag,
                              char *buf, size_t buf_size);


/* ============================================================================
 * L6: Segment Commissioning Checklist
 *
 * Before a segment goes operational, all items must pass:
 *   1. Terminators: exactly 2, one at each trunk end
 *   2. Grounding: shield grounded at exactly one point (usually PS)
 *   3. Polarity: all devices wired with correct polarity (+/-)
 *   4. Power budget: all devices >= 9V under worst case
 *   5. Signal level: 0.75?1.0 Vpp at furthest device
 *   6. Live List: all expected devices present and operational
 *   7. LAS: exactly 1 LAS, at least 1 backup Link Master
 *   8. Time sync: all devices synchronized
 *
 * Reference: Fieldbus Foundation AG-181 "System Engineering Guidelines"
 * ============================================================================ */

typedef struct {
    int terminators_ok;
    int grounding_ok;
    int polarity_ok;
    int power_budget_ok;
    int signal_level_ok;
    int live_list_ok;
    int las_ok;
    int time_sync_ok;
} ff_commissioning_checklist_t;

/**
 * Count the number of passing items in the commissioning checklist.
 *
 * @return number of OK items (0-8)
 */
int ff_commissioning_pass_count(const ff_commissioning_checklist_t *checklist);

/**
 * Check if segment is ready for operation (all 8 items must pass).
 */
int ff_commissioning_ready(const ff_commissioning_checklist_t *checklist);


#ifdef __cplusplus
}
#endif

#endif /* FF_H1_SEGMENT_H */