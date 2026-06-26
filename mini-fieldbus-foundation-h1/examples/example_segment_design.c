/**
 * example_segment_design.c ? H1 Segment Design: Power Budget for a Flow Control Loop
 *
 * Demonstrates the canonical H1 segment design calculation for a real-world
 * process automation scenario: a flow control loop with 6 devices on
 * a 300m trunk with Type A cable at 40?C ambient (Middle East installation).
 *
 * Knowledge: L6 (Canonical Problem ? H1 Segment Design)
 */

#include "ff_h1_segment.h"
#include "ff_h1_physical.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void) {
    printf("???????????????????????????????????????????????\n");
    printf("  H1 Segment Design: Flow Control Loop          \n");
    printf("  Plant: Gas Processing Facility, Abu Dhabi     \n");
    printf("  Ambient: 40?C, Type A cable, 300m trunk       \n");
    printf("???????????????????????????????????????????????\n\n");

    /* Configure the segment */
    ff_segment_config_t config;
    memset(&config, 0, sizeof(config));

    /* Power supply: Pepperl+Fuchs F2?? 24V / 500mA */
    config.power_supply.output_voltage_v = 24.0;
    config.power_supply.max_current_ma = 500.0;
    config.power_supply.conditioner_drop_v = 0.5;
    config.power_supply.is_type = FF_IS_TYPE_NONE;

    config.trunk_cable_type = FF_CABLE_TYPE_A;
    config.trunk_length_m = 300.0;
    config.temperature_c = 40.0;  /* High ambient */

    /* 6 devices on this segment:
     *   1. FT-1001: Rosemount 3051S FC flow transmitter (20mA)
     *   2. PT-1002: Rosemount 3051S pressure transmitter (20mA)
     *   3. FV-1001: Fisher DVC6200 valve positioner (25mA)
     *   4. TT-1003: Rosemount 3144P temperature transmitter (20mA)
     *   5. AT-1001: Rosemount X-STREAM gas analyzer (35mA ? heavier load)
     *   6. Spare capacity (assumed 20mA)
     */
    config.num_devices = 6;

    /* Device 1: Flow Transmitter ? 20mA, 50m spur */
    config.device_current_ma[0] = 20.0;
    config.spur_cable_type[0] = FF_CABLE_TYPE_A;
    config.spur_length_m[0] = 50.0;

    /* Device 2: Pressure Transmitter ? 20mA, 30m spur */
    config.device_current_ma[1] = 20.0;
    config.spur_cable_type[1] = FF_CABLE_TYPE_A;
    config.spur_length_m[1] = 30.0;

    /* Device 3: Valve Positioner ? 25mA, 80m spur (near the valve) */
    config.device_current_ma[2] = 25.0;
    config.spur_cable_type[2] = FF_CABLE_TYPE_A;
    config.spur_length_m[2] = 80.0;

    /* Device 4: Temperature Transmitter ? 20mA, 10m spur */
    config.device_current_ma[3] = 20.0;
    config.spur_cable_type[3] = FF_CABLE_TYPE_A;
    config.spur_length_m[3] = 10.0;

    /* Device 5: Gas Analyzer ? 35mA, 120m spur (worst case) */
    config.device_current_ma[4] = 35.0;
    config.spur_cable_type[4] = FF_CABLE_TYPE_A;
    config.spur_length_m[4] = 120.0;

    /* Device 6: Spare ? 20mA, 30m spur */
    config.device_current_ma[5] = 20.0;
    config.spur_cable_type[5] = FF_CABLE_TYPE_A;
    config.spur_length_m[5] = 30.0;

    /* Run power budget */
    ff_power_budget_result_t result;
    ff_segment_power_budget(&config, &result);

    /* Display results */
    printf("??? Segment Configuration ???\n");
    printf("  Trunk: %.0f m Type A at %.0f?C\n", config.trunk_length_m, config.temperature_c);
    printf("  Devices: %d\n", config.num_devices);
    printf("  Supply: %.1f V / %.0f mA max\n",
           config.power_supply.output_voltage_v,
           config.power_supply.max_current_ma);

    printf("\n??? Cable Parameters ???\n");
    const ff_cable_spec_t *cable = ff_cable_spec(FF_CABLE_TYPE_A);
    printf("  %s\n", cable->description);
    printf("  Loop resistance: %.1f ?/km at 20?C\n", cable->resistance_per_km);
    printf("  Temperature correction factor at %.0f?C: %.3f\n",
           config.temperature_c,
           1.0 + 0.00393 * (config.temperature_c - 20.0));

    printf("\n??? Power Budget Results ???\n");
    printf("  Total segment current: %.1f mA (incl. 10%% margin)\n", result.total_current_ma);
    printf("  Power supply utilization: %.1f%%\n",
           result.power_supply_utilization * 100.0);
    printf("  Trunk voltage drop: %.3f V\n", result.trunk_voltage_drop_v);
    printf("  Minimum device voltage: %.3f V (Device #%d)\n",
           result.min_device_voltage_v, result.worst_device_index + 1);
    printf("  Current margin: %.1f mA\n", result.margin_ma);
    printf("  Voltage margin (above 9V): %.3f V\n", result.margin_v);

    printf("\n??? Design Verdict ???\n");
    if (result.is_viable) {
        printf("  ? SEGMENT IS VIABLE\n");
        printf("  All devices receive ? 9V under worst-case conditions.\n");
    } else {
        printf("  ? SEGMENT IS NOT VIABLE\n");
        printf("  Corrective actions:\n");
        if (result.min_device_voltage_v < 9.0) {
            printf("    - Reduce trunk length or use heavier cable\n");
            printf("    - Add a second power supply at the far end\n");
            printf("    - Reduce spur length for worst-case device\n");
        }
        if (result.power_supply_utilization > 1.0) {
            printf("    - Upgrade power supply to higher current rating\n");
            printf("    - Move high-power device to separate segment\n");
        }
    }

    /* Spur validation */
    double spurs[6];
    for (int i = 0; i < 6; i++) spurs[i] = config.spur_length_m[i];
    int spurs_ok = ff_segment_validate_spurs(6, spurs);
    printf("\n??? Spur Validation ???\n");
    for (int i = 0; i < 6; i++) {
        double max_spur = ff_h1_max_spur_length(6);
        printf("  Device %d: %.0f m (limit: %.0f m) %s\n",
               i + 1, spurs[i], max_spur,
               spurs[i] <= max_spur ? "?" : "? VIOLATION");
    }
    printf("  All spurs valid: %s\n", spurs_ok ? "YES" : "NO");

    /* Round-trip time */
    double rtt = ff_segment_round_trip_time(&config);
    printf("\n??? Timing ???\n");
    printf("  Worst-case round-trip time: %.1f ?s\n", rtt);
    printf("  Max CD slot time: 1500 ?s\n");
    printf("  Slot budget: %s\n", rtt < 1500 ? "? SUFFICIENT" : "? EXCEEDED");

    printf("\n???????????????????????????????????????????????\n");

    return result.is_viable ? 0 : 1;
}