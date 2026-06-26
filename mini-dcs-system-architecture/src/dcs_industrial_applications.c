/**
 * @file dcs_industrial_applications.c
 * @brief DCS industrial application models — vendor-specific architectures.
 *
 * Knowledge Level: L7 Industrial Applications
 *
 * References:
 *   - Honeywell Experion PKS System Architecture (EP03-300-200)
 *   - Yokogawa CENTUM VP Integrated Production Control System (TI 33Y01B10-01E)
 *   - Emerson DeltaV Distributed Control System Product Data Sheet
 *   - Siemens SIMATIC PCS 7 Process Control System
 *   - ABB Ability System 800xA Architecture Overview
 *   - Supcon ECS-700 DCS Architecture (Chinese GB/T standards)
 *
 * Covers vendor-specific DCS architecture models, sizing rules,
 * and migration path analysis.
 */

#include "dcs_types.h"
#include "dcs_architecture.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/*===========================================================================
 * L7: Honeywell Experion PKS Architecture Model
 *===========================================================================*/

/**
 * @brief Honeywell Experion PKS node configuration.
 *
 * Experion PKS uses a hierarchical architecture:
 *   Level 0: Field devices (HART, FF, Profibus)
 *   Level 1: C300 Controller + Series-8 I/O
 *   Level 2: Experion Server (ESVT) + FTE (Fault Tolerant Ethernet)
 *   Level 3: Experion eServer, Historian, PHD
 *
 * FTE (Fault Tolerant Ethernet):
 *   - Dual parallel Ethernet networks (Yellow and Green trees)
 *   - Single network failure: < 1 second recovery
 *   - Both networks carry traffic simultaneously
 */
typedef struct {
    char     system_id[32];
    uint32_t num_c300_controllers;
    uint32_t num_series8_io_racks;
    uint32_t num_esvt_servers;
    uint32_t num_estations;         /* Operator stations */
    double   fte_bandwidth_mbps;     /* 100 or 1000 Mbps */
    int      fte_redundant;          /* Always 1 for production */
    int      experion_release;       /* R500, R501, R510, R520 */
} dcs_honeywell_experion_t;

/**
 * @brief Calculate maximum I/O capacity for Honeywell Experion PKS.
 *
 * C300 Controller limits:
 *   - Max 64 I/O modules (Series-8)
 *   - Max 800 I/O points per controller (mixed AI/AO/DI/DO)
 *   - Max 200 PID loops per controller
 *   - Scan period: 50 ms, 100 ms, 200 ms, 500 ms, 1000 ms
 *
 * FTE network limits:
 *   - Max 40 nodes per FTE community
 *   - Max 200 nodes with FTE bridges
 *   - 100 Mbps or 1 Gbps
 *
 * @param config  Honeywell configuration.
 * @return        Maximum I/O capacity.
 */
uint32_t dcs_honeywell_max_io_capacity(const dcs_honeywell_experion_t *config)
{
    if (config == NULL) return 0;

    /* C300 controller: 800 I/O per controller */
    uint32_t io_per_ctrl = 800;

    /* Series-8 I/O: 16 channels per module (average) */
    uint32_t io_per_rack = 64 * 16; /* 64 modules × 16 channels */

    /* System capacity is limited by controllers */
    uint32_t controller_io = config->num_c300_controllers * io_per_ctrl;
    uint32_t rack_io = config->num_series8_io_racks * io_per_rack;

    /* Return the limiting factor */
    return (controller_io < rack_io) ? controller_io : rack_io;
}

/**
 * @brief Recommend Experion PKS sizing for a given I/O count.
 *
 * @param total_io       Total I/O points.
 * @param ctrl_count     Output: recommended C300 controllers.
 * @param rack_count     Output: recommended Series-8 I/O racks.
 * @param server_count   Output: recommended Experion servers.
 * @return               1 on success.
 */
int dcs_honeywell_recommend_sizing(uint32_t total_io,
                                    uint32_t *ctrl_count,
                                    uint32_t *rack_count,
                                    uint32_t *server_count)
{
    if (ctrl_count == NULL || rack_count == NULL
        || server_count == NULL) return 0;

    /* Controllers: 800 I/O per C300, 70% loading, redundant pair */
    uint32_t io_per_ctrl = (uint32_t)(800.0 * 0.70);
    *ctrl_count = (total_io + io_per_ctrl - 1) / io_per_ctrl;

    /* Always redundant (pair = 2 controllers) */
    if (*ctrl_count < 2) *ctrl_count = 2;

    /* I/O racks: 1024 channels per rack, 80% utilization */
    uint32_t io_per_rack = (uint32_t)(1024.0 * 0.80);
    *rack_count = (total_io + io_per_rack - 1) / io_per_rack;

    /* Servers: 1 server pair per 10 controllers */
    *server_count = (*ctrl_count + 9) / 10;
    if (*server_count < 2) *server_count = 2; /* Redundant pair */

    return 1;
}

/*===========================================================================
 * L7: Yokogawa CENTUM VP Architecture Model
 *===========================================================================*/

/**
 * @brief Yokogawa CENTUM VP node configuration.
 *
 * CENTUM VP architecture:
 *   Level 0: Field devices (HART, FF, ISA100 Wireless)
 *   Level 1: FCS (Field Control Station) with Vnet/IP
 *   Level 2: HIS (Human Interface Station) + Vnet/IP
 *   Level 3: Exaquantum Historian, Exapilot APC
 *
 * Vnet/IP (real-time control bus):
 *   - 1 Gbps dual-redundant Ethernet
 *   - Up to 256 stations per domain
 *   - Deterministic communication (TDMA-like scheduling)
 */
typedef struct {
    char     system_id[32];
    uint32_t num_fcs_stations;      /* Field control stations */
    uint32_t num_his_stations;      /* Human interface stations */
    uint32_t num_nodes_per_vnet;    /* Vnet/IP domain nodes */
    double   vnet_bandwidth_gbps;   /* 1 Gbps standard */
    int      vnet_dual;             /* Dual-redundant = 1 (always) */
    int      centum_generation;     /* CS 3000, CS VP, CENTUM VP */
} dcs_yokogawa_centum_t;

/**
 * @brief Calculate bus bandwidth usage for CENTUM VP Vnet/IP.
 *
 * Vnet/IP communication cycle:
 *   1. Cyclic data transfer (process data): fixed time slots
 *   2. Message transfer (alarms, events): dynamic allocation
 *   3. File transfer (engineering data): remaining bandwidth
 *
 * Maximum cyclic data:
 *   100,000 tags × 32 bytes × 2 (redundant) per second
 *   = 100,000 × 32 × 2 × 8 = 51.2 Mbps per direction
 *
 * @param config     Yokogawa configuration.
 * @param num_tags   Number of real-time tags.
 * @param bandwidth_used Output: bandwidth used in Mbps.
 * @return           1 if within Vnet/IP capacity, 0 if overloaded.
 */
int dcs_yokogawa_vnet_bandwidth_check(const dcs_yokogawa_centum_t *config,
                                       uint32_t num_tags,
                                       double *bandwidth_used)
{
    if (config == NULL) return 0;

    /* Cyclic data: 32 bytes per tag, 1 Hz update, bi-directional */
    double bytes_per_tag = 32.0;
    double updates_per_sec = 1.0;
    double factor_redundant = 2.0;

    double bps = (double)num_tags * bytes_per_tag * updates_per_sec
                * factor_redundant * 8.0;

    double mbps = bps / 1e6;

    if (bandwidth_used != NULL) *bandwidth_used = mbps;

    /* Vnet/IP is 1 Gbps full duplex
     * Deterministic traffic must stay < 40% for bounded latency */
    double available_mbps = 1000.0 * 0.40;

    return (mbps <= available_mbps) ? 1 : 0;
}

/**
 * @brief Calculate CENTUM VP FCS controller scan utilization.
 *
 * FCS executes in fixed scan cycles:
 *   - High-speed scan: 50 ms (critical loops)
 *   - Medium-speed scan: 200 ms (regulatory control)
 *   - Low-speed scan: 1000 ms (monitoring, calculations)
 *
 * @param fast_loops    Number of 50ms loops.
 * @param med_loops     Number of 200ms loops.
 * @param slow_loops    Number of 1000ms loops.
 * @param utilization   Output: FCS utilization percentage.
 * @return              1 on success.
 */
int dcs_yokogawa_fcs_utilization(uint32_t fast_loops,
                                  uint32_t med_loops,
                                  uint32_t slow_loops,
                                  double *utilization)
{
    if (utilization == NULL) return 0;

    /*
     * FCS execution times:
     *   PID loop:    100 µs
     *   AI channel:   30 µs
     *   Sequence:    200 µs
     *
     * Scan period: 50ms = 50000µs (high-speed), 1000ms medium, 2000ms slow
     */
    double t_per_loop = 100.0; /* µs per PID loop execution */

    /* Each loop executes once per its scan period */
    double exec_per_50ms = (double)fast_loops * t_per_loop;
    double exec_per_200ms = (double)med_loops * t_per_loop / 4.0;
    double exec_per_1000ms = (double)slow_loops * t_per_loop / 20.0;

    double total_exec_per_50ms = exec_per_50ms
                                + exec_per_200ms
                                + exec_per_1000ms;

    /* Overhead: 20% of execution time */
    double total_with_overhead = total_exec_per_50ms * 1.20;

    double util = total_with_overhead / 50000.0 * 100.0;

    if (util > 100.0) util = 100.0;
    if (util < 0.0) util = 0.0;

    *utilization = util;

    /* Acceptable: ≤ 80% for CENTUM VP */
    return (util <= 80.0) ? 1 : 0;
}

/*===========================================================================
 * L7: Emerson DeltaV Architecture Model
 *===========================================================================*/

/**
 * @brief Emerson DeltaV node configuration.
 *
 * DeltaV architecture:
 *   Level 0: Field devices (HART, FF, WirelessHART)
 *   Level 1: DeltaV Controller (M-series, S-series, PK Controller)
 *   Level 2: DeltaV Workstation + DeltaV Application Station
 *   Level 3: DeltaV Historian (PI), AMS Device Manager
 *
 * DeltaV controllers use CHARMs (CHARacterization Modules)
 * for electronic marshalling — software-configurable I/O.
 */
typedef struct {
    char     system_id[32];
    uint32_t num_controllers;
    uint32_t num_charms;           /* Electronic marshalling */
    uint32_t num_workstations;
    uint32_t max_dst_per_controller; /* Device Signal Tags */
    double   controller_scan_ms;     /* 100ms default */
    int      charms_redundant;
    int      deltav_version;         /* v13, v14, v15 */
} dcs_emerson_deltav_t;

/**
 * @brief Calculate CHARM I/O capacity for DeltaV.
 *
 * CHARM I/O card (CIOC):
 *   - Up to 96 CHARMs per CIOC
 *   - Each CHARM = 1 I/O channel
 *   - Up to 8 CIOCs per controller (768 CHARMs total)
 *   - Mixed signal types: AI, AO, DI, DO, RTD, TC, Pulse
 *
 * @param config  DeltaV configuration.
 * @return        Maximum CHARM I/O count.
 */
uint32_t dcs_deltav_charm_capacity(const dcs_emerson_deltav_t *config)
{
    if (config == NULL) return 0;

    uint32_t charms_per_cioc = 96;
    uint32_t cioc_per_ctrl = 8;

    return config->num_controllers * cioc_per_ctrl * charms_per_cioc;
}

/**
 * @brief Calculate DST (Device Signal Tag) licensing requirement.
 *
 * DeltaV licenses DSTs (Device Signal Tags) — each physical I/O
 * or software tag consumes 1 DST.
 *
 * DST tiers:
 *   - 50 DST (small skid)
 *   - 100, 250, 500, 1000, 5000+ DST
 *
 * Recommendation: select the next tier above calculated requirement
 * with 25% growth margin.
 *
 * @param io_count     Total I/O count (all types).
 * @param sw_tags      Software tags (PID, CALC, etc.).
 * @param dst_tier     Output: recommended DST tier.
 * @return             1 on success.
 */
int dcs_deltav_dst_tier(uint32_t io_count,
                         uint32_t sw_tags,
                         uint32_t *dst_tier)
{
    if (dst_tier == NULL) return 0;

    uint32_t total_tags = io_count + sw_tags;

    /* Add 25% growth margin */
    total_tags = (uint32_t)((double)total_tags * 1.25);

    /* Select tier */
    const uint32_t tiers[] = {50, 100, 250, 500, 1000, 2500, 5000, 10000};
    int num_tiers = 8;

    for (int i = 0; i < num_tiers; i++) {
        if (total_tags <= tiers[i]) {
            *dst_tier = tiers[i];
            return 1;
        }
    }

    /* Beyond 10000: custom sizing */
    *dst_tier = ((total_tags + 4999) / 5000) * 5000;
    return 1;
}

/*===========================================================================
 * L7: Vendor Comparison — System Sizing
 *===========================================================================*/

/**
 * @brief Compare DCS vendor sizing for a given I/O count.
 *
 * Provides a normalized comparison across vendors to support
 * Front-End Engineering Design (FEED) decisions.
 *
 * @param total_io           Total I/O points.
 * @param honeywell_ctrl     Output: Honeywell C300 count.
 * @param yokogawa_fcs       Output: Yokogawa FCS count.
 * @param emerson_ctrl       Output: Emerson DeltaV controller count.
 * @return                   1 on success.
 */
int dcs_compare_vendor_sizing(uint32_t total_io,
                               uint32_t *honeywell_ctrl,
                               uint32_t *yokogawa_fcs,
                               uint32_t *emerson_ctrl)
{
    if (honeywell_ctrl == NULL || yokogawa_fcs == NULL
        || emerson_ctrl == NULL) return 0;

    /* Honeywell: 800 I/O per C300, 70% loading */
    *honeywell_ctrl = (uint32_t)ceil((double)total_io / (800.0 * 0.70));

    /* Yokogawa: 1200 I/O per FCS, 70% loading */
    *yokogawa_fcs = (uint32_t)ceil((double)total_io / (1200.0 * 0.70));

    /* Emerson: 768 CHARMs per controller, 80% utilization */
    *emerson_ctrl = (uint32_t)ceil((double)total_io / (768.0 * 0.80));

    return 1;
}

/**
 * @brief Estimate DCS system cost based on vendor and I/O count.
 *
 * Rough order of magnitude (ROM) cost estimate for FEED studies.
 * Costs include hardware, software licenses, engineering, FAT, SAT.
 *
 * Cost model (USD per I/O point, including all subsystems):
 *   - Honeywell Experion PKS: $800-1500 per I/O
 *   - Yokogawa CENTUM VP:    $700-1300 per I/O
 *   - Emerson DeltaV:        $600-1200 per I/O
 *   - Siemens PCS 7:         $700-1400 per I/O
 *   - ABB 800xA:             $800-1600 per I/O
 *
 * Economies of scale: larger systems have lower per-point cost.
 *
 * @param total_io        Total I/O points.
 * @param vendor          Vendor index (0=Honeywell, 1=Yokogawa,
 *                        2=Emerson, 3=Siemens, 4=ABB).
 * @param estimated_cost  Output: estimated system cost in USD.
 * @return                1 on success.
 */
int dcs_estimate_system_cost(uint32_t total_io,
                              int vendor,
                              double *estimated_cost)
{
    if (total_io == 0 || estimated_cost == NULL) return 0;

    /* Base cost per I/O point (USD) for 1000-point system */
    const double base_cost_per_io[] = {1100.0, 950.0, 850.0, 1000.0, 1150.0};

    if (vendor < 0 || vendor > 4) return 0;

    double cost_per_io = base_cost_per_io[vendor];

    /*
     * Economy of scale factor:
     *   < 500 points:    × 1.3
     *   500-1000:        × 1.1
     *   1000-5000:       × 1.0
     *   5000-10000:      × 0.85
     *   > 10000:         × 0.70
     */
    double scale_factor;
    if (total_io < 500) {
        scale_factor = 1.30;
    } else if (total_io < 1000) {
        scale_factor = 1.10;
    } else if (total_io < 5000) {
        scale_factor = 1.00;
    } else if (total_io < 10000) {
        scale_factor = 0.85;
    } else {
        scale_factor = 0.70;
    }

    *estimated_cost = (double)total_io * cost_per_io * scale_factor;

    return 1;
}
