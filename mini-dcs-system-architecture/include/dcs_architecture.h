/**
 * @file dcs_architecture.h
 * @brief DCS system architecture analysis and verification.
 *
 * Knowledge Level: L2 Core Concepts + L3 Engineering Structures
 *
 * References:
 *   - ISA-95 Part 1: Enterprise-Control System Integration Models
 *   - ISA-95 Part 3: Activity Models of Manufacturing Operations
 *   - Honeywell Experion PKS System Architecture (EP-DCS-SYS-ARCH, 2020)
 *   - Yokogawa CENTUM VP System Overview (TI 33Y01B10-01E)
 *   - Emerson DeltaV Distributed Control System Product Data Sheet
 *
 * Covers: ISA-95 level mapping, architecture verification, system sizing,
 * network topology analysis, and communication backbone design.
 */

#ifndef DCS_ARCHITECTURE_H
#define DCS_ARCHITECTURE_H

#include "dcs_types.h"

/*===========================================================================
 * L2: System Architecture Configuration
 *===========================================================================*/

/**
 * @brief DCS domain configuration.
 *
 * A DCS domain is a logical grouping of nodes that share a common
 * configuration database, time source, and security domain.
 * Large plants may have multiple domains.
 */
typedef struct {
    uint32_t            domain_id;
    char                domain_name[32];
    dcs_network_topology_t topology;
    uint32_t            num_nodes;
    uint32_t            max_nodes;
    int                 is_critical_domain;  /* Safety-critical domain */
    double              network_load_pct;
} dcs_domain_t;

/**
 * @brief Node connectivity record.
 *
 * Describes a communication link between two DCS nodes.
 */
typedef struct {
    uint32_t         link_id;
    uint32_t         node_a_id;
    uint32_t         node_b_id;
    dcs_link_type_t  link_type;
    double           bandwidth_mbps;
    double           latency_ms;
    int              redundant;
    int              is_active;
} dcs_node_link_t;

/**
 * @brief Architecture verification result.
 */
typedef struct {
    int     topology_valid;
    int     hierarchy_complete;       /* All ISA-95 levels present */
    int     redundancy_adequate;      /* Critical paths redundant */
    int     network_capacity_ok;      /* Bandwidth sufficient */
    int     time_sync_configured;
    int     security_zones_defined;
    uint32_t violations;
    char    violation_list[10][128];  /* Up to 10 violation descriptions */
} dcs_arch_verification_t;

/*===========================================================================
 * L2: Core Functions — ISA-95 Level Mapping
 *===========================================================================*/

/**
 * @brief Map a DCS node type to its ISA-95 hierarchy level.
 *
 * ISA-95 defines 5 levels (0-4). This function determines which level
 * a given DCS node type operates at. Essential for architecture layering.
 *
 * @param node_type  The DCS node type to classify.
 * @return           The ISA-95 hierarchy level (0-4).
 */
dcs_hierarchy_level_t dcs_map_node_to_isa95_level(dcs_node_type_t node_type);

/**
 * @brief Check if a given ISA-95 level is adequately represented in the system.
 *
 * A system must have all ISA-95 levels populated for complete architecture.
 *
 * @param config  The DCS system configuration.
 * @param level   The ISA-95 level to check.
 * @return        1 if the level has at least one configured node, 0 otherwise.
 */
int dcs_check_isa95_level_present(const dcs_system_config_t *config,
                                   dcs_hierarchy_level_t level);

/*===========================================================================
 * L2: Core Functions — Architecture Verification
 *===========================================================================*/

/**
 * @brief Verify a DCS system architecture against best practices.
 *
 * Checks:
 *   1. All ISA-95 levels are represented (Levels 0-3 minimum).
 *   2. Controller redundancy is enabled for critical loops.
 *   3. Network topology supports redundancy (ring/mesh, not bus).
 *   4. Maximum network load does not exceed 60% of bandwidth.
 *   5. Time synchronization source is configured.
 *   6. Operator stations are replicated (no single point of failure).
 *   7. Safety controllers are isolated from basic process control.
 *
 * @param config        The DCS system configuration to verify.
 * @param result        Output: detailed verification result with violations.
 * @return              1 if all checks pass, 0 if violations exist.
 */
int dcs_verify_architecture(const dcs_system_config_t *config,
                             dcs_arch_verification_t *result);

/*===========================================================================
 * L3: System Sizing Functions
 *===========================================================================*/

/**
 * @brief Calculate the minimum number of controller nodes for a given I/O count.
 *
 * Formula: N_ctrl = ceil(total_IO / IO_per_controller)
 * where IO_per_controller considers:
 *   - Max I/O channels per controller (typical: 2000-5000)
 *   - Controller loading target (typically 60-70%)
 *   - Scan period requirement
 *
 * @param total_io_points   Total number of I/O points (all types).
 * @param scan_period_ms    Required scan period in milliseconds.
 * @return                  Minimum number of controllers needed.
 */
uint32_t dcs_calculate_controller_count(uint32_t total_io_points,
                                         double scan_period_ms);

/**
 * @brief Calculate network bandwidth requirement for the DCS backbone.
 *
 * Accounts for:
 *   - Cyclic I/O data (controller-to-server)
 *   - Operator station refresh (server-to-client)
 *   - Alarm/event traffic
 *   - Engineering downloads
 *   - Time synchronization
 *   - OPC data access
 *
 * @param config   The DCS system configuration.
 * @return         Required bandwidth in Mbps.
 */
double dcs_calculate_bandwidth_requirement(const dcs_system_config_t *config);

/**
 * @brief Calculate expected network load as a percentage of capacity.
 *
 * Network load = total_traffic / backbone_speed
 * Best practice: keep load < 50% for normal operation,
 * < 70% including burst traffic.
 *
 * @param config         The DCS system configuration.
 * @param total_traffic  Total estimated traffic in Mbps.
 * @return               Network load as a percentage (0-100).
 */
double dcs_calculate_network_load(const dcs_system_config_t *config,
                                   double total_traffic);

/**
 * @brief Estimate system availability based on redundancy configuration.
 *
 * Uses series-parallel reliability block diagram (RBD) analysis.
 * A_sys = A_controller^N_ctrl * A_network * A_server * A_ops...
 * For redundant components: A_pair = 1 - (1 - A_single)^2
 *
 * @param config   The DCS system configuration.
 * @return         System availability as a fraction (0.0 to 1.0).
 */
double dcs_estimate_availability(const dcs_system_config_t *config);

/*===========================================================================
 * L3: Topology Analysis Functions
 *===========================================================================*/

/**
 * @brief Verify that the network topology supports the required redundancy level.
 *
 * Ring and dual-ring topologies provide self-healing capability.
 * Bus topology has a single point of failure.
 * Mesh topology provides highest redundancy but highest cost.
 *
 * @param topology    The network topology.
 * @param redundant   Whether redundancy is required.
 * @return            1 if topology is compatible with redundancy requirement, 0 otherwise.
 */
int dcs_verify_topology_redundancy(dcs_network_topology_t topology,
                                    int redundant);

/**
 * @brief Calculate the network diameter (maximum hop count) for a topology.
 *
 * Important for deterministic communication latency analysis.
 * Bus: n-1, Star: 2, Ring: n/2, Mesh: 1 (single-hop between any nodes).
 *
 * @param topology   The network topology.
 * @param num_nodes  Number of nodes in the topology.
 * @return           Maximum hop count between any two nodes.
 */
uint32_t dcs_network_diameter(dcs_network_topology_t topology,
                               uint32_t num_nodes);

/**
 * @brief Analyze worst-case latency for a given topology.
 *
 * Latency = (frame_size_bits / bandwidth_bps) * num_hops + switch_delay * (num_hops - 1)
 *
 * @param topology       The network topology.
 * @param num_nodes      Number of nodes.
 * @param bandwidth_mbps Bandwidth in Mbps.
 * @param frame_size_bytes Frame size in bytes.
 * @return               Worst-case end-to-end latency in microseconds.
 */
double dcs_worst_case_latency(dcs_network_topology_t topology,
                               uint32_t num_nodes,
                               double bandwidth_mbps,
                               uint32_t frame_size_bytes);

/*===========================================================================
 * L3: Controller Loading Analysis
 *===========================================================================*/

/**
 * @brief Analyze controller CPU loading based on configured loops and I/O.
 *
 * CPU load (%) = (N_PID * T_PID + N_AI * T_AI + N_AO * T_AO +
 *                 N_DI * T_DI + N_DO * T_DO + N_HART * T_HART) / scan_period
 *
 * where T_* are execution times per function block type.
 * Best practice: keep loading < 70% for normal operation.
 *
 * @param num_pid_loops     Number of PID loops.
 * @param num_ai_points     Number of analog input points.
 * @param num_ao_points     Number of analog output points.
 * @param num_di_points     Number of digital input points.
 * @param num_do_points     Number of digital output points.
 * @param scan_period_ms    Controller scan period in milliseconds.
 * @return                  Estimated CPU loading as percentage.
 */
double dcs_analyze_controller_loading(uint32_t num_pid_loops,
                                       uint32_t num_ai_points,
                                       uint32_t num_ao_points,
                                       uint32_t num_di_points,
                                       uint32_t num_do_points,
                                       double scan_period_ms);

/*===========================================================================
 * L6: Classic Problem — DCS System Sizing
 *===========================================================================*/

/**
 * @brief Complete DCS system sizing recommendation.
 *
 * Given plant requirements (I/O count, process areas, operators),
 * compute the recommended DCS architecture including:
 *   - Number and type of controllers
 *   - I/O subsystem layout
 *   - Network topology recommendation
 *   - Server specifications
 *   - Operator station count
 *
 * @param config           Input: plant requirements.
 * @param recommended_ctrl Output: recommended controller count.
 * @param recommended_ops  Output: recommended operator station count.
 * @param recommended_topo Output: recommended network topology.
 * @return                 1 on success, 0 if requirements cannot be met.
 */
int dcs_recommend_system_sizing(const dcs_system_config_t *config,
                                 uint32_t *recommended_ctrl,
                                 uint32_t *recommended_ops,
                                 dcs_network_topology_t *recommended_topo);

#endif /* DCS_ARCHITECTURE_H */
