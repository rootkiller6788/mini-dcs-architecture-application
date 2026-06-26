/**
 * @file dcs_architecture.c
 * @brief DCS system architecture implementation.
 *
 * Covers ISA-95 level mapping, architecture verification, system sizing,
 * network topology analysis, and controller loading analysis.
 *
 * Knowledge Levels: L2, L3, L6
 */

#include "dcs_architecture.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*===========================================================================
 * L2: ISA-95 Level Mapping
 *===========================================================================*/

dcs_hierarchy_level_t dcs_map_node_to_isa95_level(dcs_node_type_t node_type)
{
    switch (node_type) {
        case DCS_NODE_IO_SUBSYSTEM:
            return DCS_LEVEL_0_FIELD;

        case DCS_NODE_CONTROLLER:
        case DCS_NODE_SAFETY_CONTROLLER:
        case DCS_NODE_COMM_GATEWAY:
            return DCS_LEVEL_1_CONTROL;

        case DCS_NODE_OPERATOR_STATION:
        case DCS_NODE_ALARM_SERVER:
        case DCS_NODE_APPLICATION_STATION:
        case DCS_NODE_ENGINEERING_STATION:
            return DCS_LEVEL_2_SUPERVISORY;

        case DCS_NODE_HISTORIAN:
        case DCS_NODE_BATCH_SERVER:
        case DCS_NODE_ASSET_MANAGER:
            return DCS_LEVEL_3_PLANT_MES;

        case DCS_NODE_DOMAIN_CONTROLLER:
            return DCS_LEVEL_4_ENTERPRISE;

        default:
            return DCS_LEVEL_1_CONTROL;
    }
}

int dcs_check_isa95_level_present(const dcs_system_config_t *config,
                                   dcs_hierarchy_level_t level)
{
    if (config == NULL) return 0;

    switch (level) {
        case DCS_LEVEL_0_FIELD:
            return config->num_io_subsystems > 0;
        case DCS_LEVEL_1_CONTROL:
            return config->num_controller_nodes > 0;
        case DCS_LEVEL_2_SUPERVISORY:
            return config->num_operator_stations > 0
                || config->num_engineering_stations > 0;
        case DCS_LEVEL_3_PLANT_MES:
            /* Application stations serve as Level 3 bridge */
            return config->num_application_stations > 0;
        case DCS_LEVEL_4_ENTERPRISE:
            /* Always present via domain controller or gateway */
            return 1;
        default:
            return 0;
    }
}

/*===========================================================================
 * L2: Architecture Verification
 *===========================================================================*/

int dcs_verify_architecture(const dcs_system_config_t *config,
                             dcs_arch_verification_t *result)
{
    if (config == NULL || result == NULL) return 0;

    memset(result, 0, sizeof(dcs_arch_verification_t));
    result->topology_valid = 1;
    result->hierarchy_complete = 1;
    result->redundancy_adequate = 1;
    result->network_capacity_ok = 1;
    result->time_sync_configured = 1;
    result->security_zones_defined = 1;
    result->violations = 0;

    /* Check 1: ISA-95 hierarchy completeness (Levels 0-2 minimum) */
    if (!dcs_check_isa95_level_present(config, DCS_LEVEL_0_FIELD)) {
        result->hierarchy_complete = 0;
        snprintf(result->violation_list[result->violations],
                 128, "Level 0 (Field) not present: no I/O subsystems");
        result->violations++;
    }
    if (!dcs_check_isa95_level_present(config, DCS_LEVEL_1_CONTROL)) {
        result->hierarchy_complete = 0;
        snprintf(result->violation_list[result->violations],
                 128, "Level 1 (Control) not present: no controllers");
        result->violations++;
    }
    if (result->violations < 10 && !dcs_check_isa95_level_present(config, DCS_LEVEL_2_SUPERVISORY)) {
        result->hierarchy_complete = 0;
        snprintf(result->violation_list[result->violations],
                 128, "Level 2 (Supervisory) not present: no operator stations");
        result->violations++;
    }

    /* Check 2: Controller redundancy for systems with > 2 controllers */
    if (config->num_controller_nodes > 2 && !config->controller_redundancy) {
        result->redundancy_adequate = 0;
        if (result->violations < 10) {
            snprintf(result->violation_list[result->violations],
                     128, "Controller redundancy not enabled for system with %u controllers",
                     config->num_controller_nodes);
            result->violations++;
        }
    }

    /* Check 3: Network topology redundancy support */
    if (config->backbone_redundant) {
        if (config->backbone_topology == DCS_TOPOLOGY_BUS) {
            result->redundancy_adequate = 0;
            if (result->violations < 10) {
                snprintf(result->violation_list[result->violations],
                         128, "Bus topology cannot support redundancy");
                result->violations++;
            }
        }
    } else if (config->num_controller_nodes > 3) {
        if (result->violations < 10) {
            snprintf(result->violation_list[result->violations],
                     128, "Network redundancy recommended for systems with %u controllers",
                     config->num_controller_nodes);
            result->violations++;
        }
    }

    /* Check 4: Network load threshold (60% max) */
    double estimated_traffic = dcs_calculate_bandwidth_requirement(config);
    double network_load = dcs_calculate_network_load(config, estimated_traffic);
    if (network_load > 60.0) {
        result->network_capacity_ok = 0;
        if (result->violations < 10) {
            snprintf(result->violation_list[result->violations],
                     128, "Network load %.1f%% exceeds 60%% threshold", network_load);
            result->violations++;
        }
    }

    /* Check 5: Operator station count (at least 1 for Level 2) */
    if (config->num_operator_stations == 0 && config->num_controller_nodes > 0) {
        if (result->violations < 10) {
            snprintf(result->violation_list[result->violations],
                     128, "No operator stations configured for process monitoring");
            result->violations++;
        }
    }

    /* Check 6: Server redundancy for critical systems */
    if (config->target_availability_pct > 99.9 && !config->server_redundancy) {
        if (result->violations < 10) {
            snprintf(result->violation_list[result->violations],
                     128, "Server redundancy needed for %.2f%% availability target",
                     config->target_availability_pct);
            result->violations++;
        }
    }

    return (result->violations == 0) ? 1 : 0;
}

/*===========================================================================
 * L3: System Sizing
 *===========================================================================*/

uint32_t dcs_calculate_controller_count(uint32_t total_io_points,
                                         double scan_period_ms)
{
    if (total_io_points == 0) return 0;
    if (scan_period_ms <= 0.0) scan_period_ms = 250.0;

    /*
     * Controller sizing:
     *   - Fast scan (≤ 100ms): 500 I/O per controller
     *   - Normal scan (100-500ms): 1500 I/O per controller
     *   - Slow scan (> 500ms): 3000 I/O per controller
     *
     * Apply 70% loading safety factor.
     */
    double io_per_controller;
    if (scan_period_ms <= 100.0) {
        io_per_controller = 500.0;
    } else if (scan_period_ms <= 500.0) {
        io_per_controller = 1500.0;
    } else {
        io_per_controller = 3000.0;
    }

    /* Apply loading factor */
    io_per_controller *= 0.70;

    uint32_t count = (uint32_t)ceil((double)total_io_points / io_per_controller);

    /* Minimum 2 controllers for redundancy */
    if (count < 2 && total_io_points > 0) count = 2;

    return count;
}

double dcs_calculate_bandwidth_requirement(const dcs_system_config_t *config)
{
    if (config == NULL) return 0.0;

    double traffic_mbps = 0.0;

    /*
     * Traffic estimation model:
     *
     * 1. Cyclic I/O data (dominant traffic):
     *    Each I/O point: ~100 bytes per scan (tag + quality + timestamp + value)
     *    Update rate = 1000 / scan_period_ms per second
     *    Traffic = total_IO * 100 bytes * update_rate * 8 bits/byte / 1e6
     */
    uint32_t total_io = config->total_ai_points + config->total_ao_points
                      + config->total_di_points + config->total_do_points;
    double update_rate = 1000.0 / config->controller_scan_ms;
    traffic_mbps += (double)total_io * 100.0 * update_rate * 8.0 / 1e6;

    /*
     * 2. Operator station refresh (server to client):
     *    ~500 bytes per displayed tag, ~200 tags per display, 1 Hz refresh
     *    per operator station
     */
    double ops_traffic = config->num_operator_stations * 500.0 * 200.0 * 1.0 * 8.0 / 1e6;
    traffic_mbps += ops_traffic;

    /*
     * 3. Alarm/event traffic: ~5% of I/O traffic
     */
    traffic_mbps *= 1.05;

    /*
     * 4. Engineering data + OPC + time sync overhead: fixed 2 Mbps
     */
    traffic_mbps += 2.0;

    /*
     * 5. Safety margin: 20%
     */
    traffic_mbps *= 1.20;

    return traffic_mbps;
}

double dcs_calculate_network_load(const dcs_system_config_t *config,
                                   double total_traffic)
{
    if (config == NULL || config->backbone_speed_mbps <= 0.0) return 100.0;

    return (total_traffic / config->backbone_speed_mbps) * 100.0;
}

double dcs_estimate_availability(const dcs_system_config_t *config)
{
    if (config == NULL) return 0.0;

    /*
     * Availability estimation using Reliability Block Diagram (RBD).
     *
     * Baseline single-component availabilities:
     *   Controller:    99.95% (MTBF 175,000 hrs, MTTR 8 hrs)
     *   Network switch: 99.99%
     *   Server:        99.9%
     *   Operator station: 99.5%
     */
    double a_ctrl_single  = 0.9995;
    double a_network      = 0.9999;
    double a_server       = 0.999;
    double a_ops          = 0.995;

    /* Controller subsystem availability (series or with redundancy) */
    double a_ctrl;
    if (config->controller_redundancy) {
        /* 1oo2: A_pair = 1 - (1 - A)^2 */
        double a_ctrl_pair = 1.0 - (1.0 - a_ctrl_single) * (1.0 - a_ctrl_single);
        /* Worst case: redundant pair fails if both fail */
        a_ctrl = a_ctrl_pair;
    } else {
        /* Series of N controllers */
        a_ctrl = pow(a_ctrl_single, (double)config->num_controller_nodes);
    }

    /* Network subsystem */
    double a_net;
    if (config->network_redundancy) {
        double a_net_pair = 1.0 - (1.0 - a_network) * (1.0 - a_network);
        a_net = a_net_pair;
    } else {
        a_net = a_network;
    }

    /* Server subsystem */
    double a_srv;
    if (config->server_redundancy) {
        a_srv = 1.0 - (1.0 - a_server) * (1.0 - a_server);
    } else {
        a_srv = a_server;
    }

    /* Operator stations (parallel: at least 1 must work) */
    if (config->num_operator_stations > 0) {
        double a_ops_parallel = 1.0 - pow(1.0 - a_ops, (double)config->num_operator_stations);
        /* System availability = product of subsystems */
        return a_ctrl * a_net * a_srv * a_ops_parallel;
    } else {
        return a_ctrl * a_net * a_srv;
    }
}

/*===========================================================================
 * L3: Topology Analysis
 *===========================================================================*/

int dcs_verify_topology_redundancy(dcs_network_topology_t topology,
                                    int redundant)
{
    if (!redundant) return 1; /* No redundancy requirement, any topology works */

    switch (topology) {
        case DCS_TOPOLOGY_RING:
        case DCS_TOPOLOGY_DUAL_RING:
        case DCS_TOPOLOGY_MESH:
        case DCS_TOPOLOGY_DUAL_STAR:
            return 1; /* These topologies support redundancy */

        case DCS_TOPOLOGY_BUS:
        case DCS_TOPOLOGY_STAR:
        case DCS_TOPOLOGY_TREE:
            return 0; /* Single point of failure exists */

        default:
            return 0;
    }
}

uint32_t dcs_network_diameter(dcs_network_topology_t topology,
                               uint32_t num_nodes)
{
    if (num_nodes <= 1) return 0;

    switch (topology) {
        case DCS_TOPOLOGY_BUS:
            return num_nodes - 1;

        case DCS_TOPOLOGY_STAR:
        case DCS_TOPOLOGY_DUAL_STAR:
            return 2; /* Any node → switch → any other node */

        case DCS_TOPOLOGY_RING:
            /* Worst case: floor(n/2) hops */
            return num_nodes / 2;

        case DCS_TOPOLOGY_DUAL_RING:
            return num_nodes / 2; /* Same as single ring but more resilient */

        case DCS_TOPOLOGY_MESH:
            return 1; /* Direct single-hop between any two nodes */

        case DCS_TOPOLOGY_TREE:
            /* Worst case: log2(n) * 2 hops (down then up the tree) */
            return 2 * ((uint32_t)(log((double)num_nodes) / log(2.0) + 0.5));

        default:
            return num_nodes - 1;
    }
}

double dcs_worst_case_latency(dcs_network_topology_t topology,
                               uint32_t num_nodes,
                               double bandwidth_mbps,
                               uint32_t frame_size_bytes)
{
    if (bandwidth_mbps <= 0.0 || frame_size_bytes == 0) return 0.0;

    uint32_t hops = dcs_network_diameter(topology, num_nodes);

    /*
     * Transmission time per hop:
     *   T_tx = (frame_size_bytes * 8) / (bandwidth_mbps * 1e6) seconds
     *   = frame_size_bytes * 8 / bandwidth_mbps µs
     */
    double tx_time_us = (double)(frame_size_bytes * 8) / bandwidth_mbps;

    /*
     * Switch processing delay: typical 5-10 µs per switch hop
     */
    double switch_delay_us = 5.0;

    /*
     * Total latency = hops * tx_time + (hops - 1) * switch_delay
     */
    double total_us = (double)hops * tx_time_us;
    if (hops > 1) {
        total_us += (double)(hops - 1) * switch_delay_us;
    }

    return total_us;
}

/*===========================================================================
 * L3: Controller Loading Analysis
 *===========================================================================*/

double dcs_analyze_controller_loading(uint32_t num_pid_loops,
                                       uint32_t num_ai_points,
                                       uint32_t num_ao_points,
                                       uint32_t num_di_points,
                                       uint32_t num_do_points,
                                       double scan_period_ms)
{
    if (scan_period_ms <= 0.0) return 100.0;

    /*
     * Execution time estimates per function block type:
     * (Values based on typical DCS controller benchmarks)
     *
     * PID loop:    150 µs
     * AI channel:   50 µs
     * AO channel:   40 µs
     * DI channel:   10 µs
     * DO channel:   10 µs
     * HART device:  60 µs
     */
    double t_pid = 150.0;  /* µs */
    double t_ai  = 50.0;
    double t_ao  = 40.0;
    double t_di  = 10.0;
    double t_do  = 10.0;

    double total_exec_us = (double)num_pid_loops * t_pid
                         + (double)num_ai_points * t_ai
                         + (double)num_ao_points * t_ao
                         + (double)num_di_points * t_di
                         + (double)num_do_points * t_do;

    /*
     * Controller loading = total_exec_us / (scan_period_ms * 1000) * 100%
     *
     * Include communication overhead: 15% of execution time
     */
    double comm_overhead = total_exec_us * 0.15;
    total_exec_us += comm_overhead;

    double load_pct = total_exec_us / (scan_period_ms * 1000.0) * 100.0;

    /* Clamp to realistic range */
    if (load_pct > 100.0) load_pct = 100.0;
    if (load_pct < 0.0) load_pct = 0.0;

    return load_pct;
}

/*===========================================================================
 * L6: Classic Problem — DCS System Sizing
 *===========================================================================*/

int dcs_recommend_system_sizing(const dcs_system_config_t *config,
                                 uint32_t *recommended_ctrl,
                                 uint32_t *recommended_ops,
                                 dcs_network_topology_t *recommended_topo)
{
    if (config == NULL || recommended_ctrl == NULL
        || recommended_ops == NULL || recommended_topo == NULL) {
        return 0;
    }

    /* Calculate total I/O */
    uint32_t total_io = config->total_ai_points + config->total_ao_points
                      + config->total_di_points + config->total_do_points;

    /* Controller count based on I/O and scan period */
    *recommended_ctrl = dcs_calculate_controller_count(total_io,
                                                        config->controller_scan_ms);

    /* Operator stations: 1 per 500 loops, minimum 2 */
    uint32_t ops = config->num_controller_nodes / 2;
    if (ops < 2) ops = 2;
    *recommended_ops = ops;

    /* Network topology recommendation */
    if (config->num_controller_nodes > 20) {
        /* Large system: mesh for highest reliability */
        *recommended_topo = DCS_TOPOLOGY_MESH;
    } else if (config->num_controller_nodes > 10) {
        /* Medium-large: dual ring */
        *recommended_topo = DCS_TOPOLOGY_DUAL_RING;
    } else if (config->num_controller_nodes > 3) {
        /* Medium: ring topology */
        *recommended_topo = DCS_TOPOLOGY_RING;
    } else {
        /* Small system: star is sufficient */
        *recommended_topo = DCS_TOPOLOGY_STAR;
    }

    /* Override based on availability target */
    if (config->target_availability_pct > 99.99) {
        *recommended_topo = DCS_TOPOLOGY_MESH;
    } else if (config->target_availability_pct > 99.9) {
        if (*recommended_topo < DCS_TOPOLOGY_DUAL_RING) {
            *recommended_topo = DCS_TOPOLOGY_DUAL_RING;
        }
    }

    return 1;
}
