/**
 * @file example_dcs_system_sizing.c
 * @brief L6 Canonical Problem: DCS system sizing for a chemical plant.
 *
 * This example demonstrates how to use the DCS architecture module
 * to size a DCS system for a typical chemical plant with:
 *   - 3 process areas (Reaction, Separation, Utilities)
 *   - 3500 total I/O points
 *   - 50 PID loops
 *   - 99.9% availability target
 *
 * The example walks through:
 *   1. Defining system requirements
 *   2. Calculating controller count
 *   3. Estimating network bandwidth
 *   4. Choosing network topology
 *   5. Estimating system availability
 *   6. Generating the sizing recommendation
 */

#include "dcs_architecture.h"
#include "dcs_redundancy.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  DCS System Sizing — Chemical Plant FEED Study  ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Step 1: Define plant requirements */
    dcs_system_config_t plant;
    memset(&plant, 0, sizeof(plant));

    snprintf(plant.system_name, 64, "Chemical Plant DCS");
    snprintf(plant.vendor, 32, "Honeywell");
    snprintf(plant.model, 32, "Experion PKS");

    /* Process requirements */
    plant.num_process_areas = 3;
    plant.total_ai_points = 800;   /* Temperature, pressure, flow, level */
    plant.total_ao_points = 300;   /* Control valves, VFDs */
    plant.total_di_points = 1200;  /* Limit switches, motor status */
    plant.total_do_points = 700;   /* Pumps, solenoids, agitators */
    plant.total_hart_devices = 500;
    plant.total_fieldbus_devices = 50;

    uint32_t total_io = plant.total_ai_points + plant.total_ao_points
                      + plant.total_di_points + plant.total_do_points;

    printf("Plant Requirements:\n");
    printf("  Process Areas:       %u\n", plant.num_process_areas);
    printf("  Total I/O Points:    %u\n", total_io);
    printf("    Analog Input:      %u\n", plant.total_ai_points);
    printf("    Analog Output:     %u\n", plant.total_ao_points);
    printf("    Digital Input:     %u\n", plant.total_di_points);
    printf("    Digital Output:    %u\n", plant.total_do_points);
    printf("  HART Devices:        %u\n", plant.total_hart_devices);
    printf("  FF Devices:          %u\n\n", plant.total_fieldbus_devices);

    /* Operational parameters */
    plant.controller_scan_ms = 250.0;
    plant.target_availability_pct = 99.9;
    plant.backbone_speed_mbps = 1000.0;  /* 1 Gbps backbone */
    plant.backbone_redundant = 1;

    printf("Operational Requirements:\n");
    printf("  Controller Scan:     %.0f ms\n", plant.controller_scan_ms);
    printf("  Target Availability: %.1f%%\n", plant.target_availability_pct);
    printf("  Backbone Speed:      %.0f Mbps\n\n", plant.backbone_speed_mbps);

    /* Step 2: Calculate controller count */
    uint32_t controllers = dcs_calculate_controller_count(total_io,
                                                           plant.controller_scan_ms);
    plant.num_controller_nodes = controllers;

    printf("Step 2 — Controller Sizing:\n");
    printf("  Controllers Required: %u (includes redundancy)\n", controllers);
    printf("  I/O per Controller:   ~%u\n\n", total_io / controllers);

    /* Step 3: Calculate bandwidth requirement */
    double bandwidth = dcs_calculate_bandwidth_requirement(&plant);
    double network_load = dcs_calculate_network_load(&plant, bandwidth);

    printf("Step 3 — Network Sizing:\n");
    printf("  Estimated Traffic:   %.2f Mbps\n", bandwidth);
    printf("  Network Load:        %.1f%% (limit: 60%%)\n\n", network_load);

    /* Step 4: Choose topology */
    uint32_t rec_ops;
    uint32_t rec_ctrl;
    dcs_network_topology_t rec_topo;
    dcs_recommend_system_sizing(&plant, &rec_ctrl, &rec_ops, &rec_topo);

    const char *topo_names[] = {"Bus", "Star", "Ring", "Dual Ring",
                                 "Mesh", "Tree", "Dual Star"};

    printf("Step 4 — Topology Recommendation:\n");
    printf("  Recommended:         %s\n", topo_names[(int)rec_topo]);
    printf("  Redundancy Compatible: %s\n",
           dcs_verify_topology_redundancy(rec_topo, 1) ? "YES" : "NO");
    printf("  Network Diameter:    %u hops\n",
           dcs_network_diameter(rec_topo, controllers + rec_ops + 2));
    printf("  Worst-Case Latency:  %.1f µs (64B frame)\n",
           dcs_worst_case_latency(rec_topo, controllers + rec_ops + 2,
                                   1000.0, 64));
    printf("  Operator Stations:   %u\n\n", rec_ops);

    /* Step 5: Estimate availability */
    plant.controller_redundancy = 1;
    plant.network_redundancy = 1;
    plant.server_redundancy = 1;
    plant.num_operator_stations = rec_ops;

    double availability = dcs_estimate_availability(&plant);
    double downtime_hrs_per_year = (1.0 - availability) * 8760.0;

    printf("Step 5 — Availability Analysis:\n");
    printf("  System Availability:  %.6f%%\n", availability * 100.0);
    printf("  Expected Downtime:    %.2f hours/year\n", downtime_hrs_per_year);

    if (availability * 100.0 >= plant.target_availability_pct) {
        printf("  Status:               MEETS %.1f%% target\n\n",
               plant.target_availability_pct);
    } else {
        printf("  Status:               BELOW target — consider:\n");
        printf("    - Add network redundancy (dual ring → mesh)\n");
        printf("    - Increase server redundancy\n");
        printf("    - Use hot standby controllers\n\n");
    }

    /* Step 6: Verify architecture */
    dcs_arch_verification_t verification;
    int ver_ok = dcs_verify_architecture(&plant, &verification);

    printf("Step 6 — Architecture Verification:\n");
    printf("  Topology:             %s\n",
           verification.topology_valid ? "VALID" : "INVALID");
    printf("  Hierarchy:            %s\n",
           verification.hierarchy_complete ? "COMPLETE" : "INCOMPLETE");
    printf("  Redundancy:           %s\n",
           verification.redundancy_adequate ? "ADEQUATE" : "INADEQUATE");
    printf("  Network Capacity:     %s\n",
           verification.network_capacity_ok ? "SUFFICIENT" : "OVERLOADED");
    printf("  Overall:              %s (%u violations)\n\n",
           ver_ok ? "PASS" : "FAIL", verification.violations);

    /* Step 7: Redundancy analysis */
    dcs_redundant_pair_t rp;
    dcs_redundancy_init(&rp, 1, 2,
                         DCS_REDUNDANCY_MODE_HOT_STANDBY,
                         DCS_REDUNDANCY_1OO2);

    double sw_time = dcs_calculate_switchover_time(
        DCS_REDUNDANCY_MODE_HOT_STANDBY, plant.controller_scan_ms);

    printf("Step 7 — Redundancy Performance:\n");
    printf("  Switchover Time (hot):  %.1f ms\n", sw_time);
    printf("  Switchover Time (warm): %.1f ms\n",
           dcs_calculate_switchover_time(DCS_REDUNDANCY_MODE_WARM_STANDBY,
                                          plant.controller_scan_ms));
    printf("  Switchover Time (cold): %.0f ms\n",
           dcs_calculate_switchover_time(DCS_REDUNDANCY_MODE_COLD_STANDBY,
                                          plant.controller_scan_ms));

    /* Step 8: Controller loading */
    double load = dcs_analyze_controller_loading(
        50, 800/controllers, 300/controllers,
        1200/controllers, 700/controllers, plant.controller_scan_ms);

    double max_loops = controllers * 50.0;  /* ~50 loops per controller */

    printf("\nStep 8 — Controller Loading:\n");
    printf("  Estimated CPU Load:   %.1f%%\n", load);
    printf("  Max PID Loops:        ~%.0f (at 250ms scan)\n", max_loops);
    printf("  Status:               %s\n\n",
           load < 70.0 ? "ACCEPTABLE (< 70%)" : "OVERLOAD — add controllers");

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  Sizing Complete — Ready for Detailed Design    ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    return 0;
}
