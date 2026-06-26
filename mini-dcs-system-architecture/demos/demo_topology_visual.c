/**
 * @file demo_topology_visual.c
 * @brief Visual demonstration of DCS network topology analysis.
 *
 * Displays a comparative analysis of different network topologies
 * for a given DCS system, including:
 *   - Network diameter
 *   - Redundancy support
 *   - Worst-case latency
 *   - Recommended use cases
 */

#include "dcs_architecture.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *topo_names[] = {"Bus", "Star", "Ring", "Dual Ring",
                                 "Mesh", "Tree", "Dual Star"};
    const int num_topos = 7;

    uint32_t system_sizes[] = {5, 10, 20, 50, 100};
    const int num_sizes = 5;

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  DCS Network Topology Analysis — Comparative Study      ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Table 1: Topology Properties */
    printf("TABLE 1: Topology Properties\n");
    printf("%-12s %-10s %-12s %-18s %s\n",
           "Topology", "Diameter", "Redundancy", "Use Case", "Cable Cost");
    printf("%-12s %-10s %-12s %-18s %s\n",
           "────────", "────────", "──────────", "──────────────", "──────────");

    const char *use_cases[] = {
        "Legacy migration", "Small systems (<5 nodes)",
        "Medium (5-20 nodes)", "Critical processes",
        "Highest availability", "Hierarchical plants",
        "Server farm"
    };
    const char *costs[] = {
        "Low", "Medium", "Medium", "High",
        "Very High", "Medium-High", "High"
    };

    for (int t = 0; t < num_topos; t++) {
        dcs_network_topology_t topo = (dcs_network_topology_t)t;
        uint32_t diam = dcs_network_diameter(topo, 10);
        int redun = dcs_verify_topology_redundancy(topo, 1);

        printf("%-12s  %-10u  %-12s  %-18s  %s\n",
               topo_names[t], diam,
               redun ? "YES" : "NO",
               use_cases[t], costs[t]);
    }

    /* Table 2: Latency vs. System Size (64-byte frames, 100 Mbps) */
    printf("\nTABLE 2: Worst-Case Latency (µs) — 64B frame, 100 Mbps\n");
    printf("%-12s", "Topology");
    for (int s = 0; s < num_sizes; s++) {
        printf("  N=%-5u", system_sizes[s]);
    }
    printf("\n");
    printf("%-12s", "────────");
    for (int s = 0; s < num_sizes; s++) {
        printf("  ───────");
    }
    printf("\n");

    for (int t = 0; t < num_topos; t++) {
        dcs_network_topology_t topo = (dcs_network_topology_t)t;
        printf("%-12s", topo_names[t]);
        for (int s = 0; s < num_sizes; s++) {
            double lat = dcs_worst_case_latency(topo, system_sizes[s],
                                                 100.0, 64);
            printf("  %7.1f", lat);
        }
        printf("\n");
    }

    /* Table 3: Recommended topology by system size */
    printf("\nTABLE 3: Topology Recommendation by System Size\n");
    printf("%-20s %-12s %s\n", "System Size", "Topology", "Rationale");
    printf("%-20s %-12s %s\n", "────────────", "────────", "─────────");

    for (int s = 0; s < num_sizes; s++) {
        dcs_system_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.num_controller_nodes = system_sizes[s];
        cfg.total_ai_points = system_sizes[s] * 100;
        cfg.total_ao_points = system_sizes[s] * 50;
        cfg.total_di_points = system_sizes[s] * 150;
        cfg.total_do_points = system_sizes[s] * 100;
        cfg.controller_scan_ms = 250.0;
        cfg.target_availability_pct = 99.9;

        uint32_t rec_ctrl, rec_ops;
        dcs_network_topology_t rec_topo;
        dcs_recommend_system_sizing(&cfg, &rec_ctrl, &rec_ops, &rec_topo);

        const char *rationale;
        switch (rec_topo) {
            case DCS_TOPOLOGY_STAR:
                rationale = "Small system — cost-optimized";
                break;
            case DCS_TOPOLOGY_RING:
                rationale = "Medium — good balance";
                break;
            case DCS_TOPOLOGY_DUAL_RING:
                rationale = "Large — enhanced redundancy";
                break;
            case DCS_TOPOLOGY_MESH:
                rationale = "Very large — maximum reliability";
                break;
            default:
                rationale = "Standard deployment";
                break;
        }

        printf("N=%-18u %-12s %s\n",
               system_sizes[s], topo_names[(int)rec_topo], rationale);
    }

    /* Summary */
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Analysis Complete — See docs/ for detailed topology    ║\n");
    printf("║  design guide and IEC 62439 redundancy standards.       ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    return 0;
}
