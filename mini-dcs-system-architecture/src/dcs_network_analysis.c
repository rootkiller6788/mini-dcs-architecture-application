/**
 * @file dcs_network_analysis.c
 * @brief DCS communication network design and analysis.
 *
 * Knowledge Levels: L3 Engineering Structures, L5 Algorithms, L6 Canonical Problems
 *
 * Covers bandwidth allocation, token passing analysis, network loading,
 * topology optimization, and real-time communication guarantees.
 *
 * References:
 *   - IEEE 802.3 Ethernet for control systems
 *   - IEC 61784-2 Industrial Ethernet profiles
 *   - Honeywell Fault Tolerant Ethernet (FTE) specification
 *   - Yokogawa Vnet/IP real-time control bus
 */

#include "dcs_architecture.h"
#include "dcs_types.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*===========================================================================
 * L2: Network Bandwidth Allocation Model
 *===========================================================================*/

/**
 * @brief Network traffic categorization for bandwidth allocation.
 */
typedef enum {
    DCS_TRAFFIC_CYCLIC_IO    = 0,  /* Deterministic, fixed bandwidth */
    DCS_TRAFFIC_ALARM_EVENT  = 1,  /* Bursty, priority guaranteed */
    DCS_TRAFFIC_HMI_REFRESH  = 2,  /* Periodic, bandwidth guaranteed */
    DCS_TRAFFIC_ENGINEERING  = 3,  /* Aperiodic, best-effort */
    DCS_TRAFFIC_TIME_SYNC    = 4,  /* Periodic, low bandwidth, critical */
    DCS_TRAFFIC_SAFETY       = 5,  /* Safety-related, highest priority */
    DCS_TRAFFIC_OPC_DATA     = 6   /* Aperiodic, moderate priority */
} dcs_traffic_category_t;

/**
 * @brief Bandwidth allocation for a traffic category.
 */
typedef struct {
    dcs_traffic_category_t category;
    double                  allocated_mbps;
    double                  peak_mbps;
    double                  priority;       /* 0-7, 7=highest (PCP/IEEE 802.1Q) */
    int                     is_guaranteed;  /* Deterministic guarantee */
} dcs_bandwidth_allocation_t;

/**
 * @brief Network segment configuration.
 */
typedef struct {
    uint32_t   segment_id;
    uint32_t   num_nodes;
    double     bandwidth_mbps;
    double     max_latency_ms;
    int        is_redundant;
    double     current_load_pct;
    double     peak_load_pct;
} dcs_network_segment_t;

/*===========================================================================
 * L3: Network Design Functions
 *===========================================================================*/

/**
 * @brief Design bandwidth allocation plan for a DCS network segment.
 *
 * Allocates network bandwidth across traffic categories according
 * to DCS best practices:
 *
 *   Cyclic I/O:   40-50% (deterministic, highest volume)
 *   Alarm/Event:   5-10% (burst headroom)
 *   HMI refresh:  15-20%
 *   Engineering:  10-15%
 *   Time sync:      2-5%
 *   Safety:         5-10% (separate VLAN recommended)
 *   OPC data:      5-10%
 *
 * @param total_bandwidth_mbps  Total segment bandwidth.
 * @param allocations           Output: array of 7 allocations.
 * @return                      1 on success.
 */
int dcs_design_bandwidth_allocation(double total_bandwidth_mbps,
                                     dcs_bandwidth_allocation_t *allocations)
{
    if (total_bandwidth_mbps <= 0.0 || allocations == NULL) return 0;

    /* Default allocation fractions */
    double fractions[] = {0.40, 0.10, 0.15, 0.10, 0.05, 0.10, 0.10};
    double priorities[] = {6, 5, 4, 2, 7, 7, 3};
    int guaranteed[]    = {1, 0, 0, 0, 1, 1, 0};

    dcs_traffic_category_t cats[] = {
        DCS_TRAFFIC_CYCLIC_IO, DCS_TRAFFIC_ALARM_EVENT,
        DCS_TRAFFIC_HMI_REFRESH, DCS_TRAFFIC_ENGINEERING,
        DCS_TRAFFIC_TIME_SYNC, DCS_TRAFFIC_SAFETY,
        DCS_TRAFFIC_OPC_DATA
    };

    for (int i = 0; i < 7; i++) {
        allocations[i].category      = cats[i];
        allocations[i].allocated_mbps = total_bandwidth_mbps * fractions[i];
        allocations[i].peak_mbps     = allocations[i].allocated_mbps * 1.5;
        allocations[i].priority      = priorities[i];
        allocations[i].is_guaranteed  = guaranteed[i];
    }

    return 1;
}

/*===========================================================================
 * L5: Token Passing Time Analysis (for Deterministic Protocols)
 *===========================================================================*/

/**
 * @brief Calculate token rotation time for a token-passing network.
 *
 * Used for legacy DCS networks (e.g., Honeywell TDC 3000 LCN).
 * Modern DCS uses switched Ethernet, but understanding token passing
 * is essential for migration and hybrid architectures.
 *
 * Token rotation time = N * T_hold + (N-1) * T_prop
 *
 * where:
 *   N = number of stations
 *   T_hold = token holding time (time each station transmits)
 *   T_prop = propagation delay between stations
 *
 * @param num_stations      Number of stations on the token ring.
 * @param token_hold_us     Token holding time per station (µs).
 * @param propagation_us    Propagation delay per hop (µs).
 * @return                  Token rotation time in microseconds.
 */
double dcs_calculate_token_rotation(uint32_t num_stations,
                                     double token_hold_us,
                                     double propagation_us)
{
    if (num_stations <= 1) return token_hold_us;

    double rotation = (double)num_stations * token_hold_us
                    + (double)(num_stations - 1) * propagation_us;

    return rotation;
}

/**
 * @brief Calculate maximum data throughput for token-passing network.
 *
 * Throughput = (token_hold_time / token_rotation_time) * link_speed
 *
 * @param num_stations        Number of stations.
 * @param token_hold_us       Token holding time per station (µs).
 * @param propagation_us      Propagation delay per hop (µs).
 * @param link_speed_mbps     Link speed in Mbps.
 * @return                    Maximum throughput in Mbps.
 */
double dcs_token_ring_throughput(uint32_t num_stations,
                                  double token_hold_us,
                                  double propagation_us,
                                  double link_speed_mbps)
{
    double rotation = dcs_calculate_token_rotation(num_stations,
                                                    token_hold_us,
                                                    propagation_us);
    if (rotation <= 0.0) return 0.0;

    /* All stations' hold time combined */
    double total_hold = (double)num_stations * token_hold_us;

    /* Fraction of time available for data */
    double utilization = total_hold / rotation;

    return utilization * link_speed_mbps;
}

/*===========================================================================
 * L5: Switched Ethernet Analysis
 *===========================================================================*/

/**
 * @brief Calculate worst-case queuing delay in a store-and-forward switch.
 *
 * For a switch with N ports at speed B, processing frames of size S:
 *
 * Queue delay = (N-1) * (S * 8 / B) * queue_factor
 *
 * where queue_factor accounts for congestion (typically 1.0-2.0).
 *
 * This is the dominant latency component in modern DCS networks.
 * IEEE 802.1Qav (FQTSS) provides credit-based shaping to bound queuing delay.
 *
 * @param num_ports         Number of switch ports.
 * @param link_speed_mbps   Per-port link speed in Mbps.
 * @param frame_size_bytes  Typical frame size in bytes.
 * @param queue_factor      Congestion factor (1.0 = no congestion).
 * @return                  Worst-case queuing delay in microseconds.
 */
double dcs_switch_queue_delay(uint32_t num_ports,
                               double link_speed_mbps,
                               uint32_t frame_size_bytes,
                               double queue_factor)
{
    if (link_speed_mbps <= 0.0 || frame_size_bytes == 0) return 0.0;

    /* Transmission time per frame */
    double tx_time_us = (double)(frame_size_bytes * 8) / link_speed_mbps;

    /* Worst case: frames from all other ports queued behind ours */
    double queued_frames = (double)(num_ports - 1) * queue_factor;

    return tx_time_us * queued_frames;
}

/**
 * @brief Analyze network convergence time after topology change.
 *
 * In a ring topology with RSTP (Rapid Spanning Tree Protocol, IEEE 802.1w):
 *   Convergence time ≈ 3 * hello_time + max_age/2 + forward_delay
 *
 * Typical values: hello=2s, max_age=20s, forward_delay=15s
 * → convergence ≈ 3*2 + 10 + 15 = ~31 seconds (standard STP)
 *
 * With RSTP (IEEE 802.1w): < 1-2 seconds (rapid transition)
 * With MRP (IEC 62439-2 Media Redundancy Protocol): < 200 ms
 * With PRP (IEC 62439-3 Parallel Redundancy Protocol): 0 ms (no convergence needed)
 *
 * @param protocol  Redundancy protocol: 0=STP, 1=RSTP, 2=MRP, 3=PRP.
 * @return          Estimated convergence time in milliseconds.
 */
double dcs_network_convergence_time(int protocol)
{
    switch (protocol) {
        case 0:  /* STP */
            return 31000.0;
        case 1:  /* RSTP */
            return 1500.0;
        case 2:  /* MRP (IEC 62439-2) */
            return 200.0;
        case 3:  /* PRP (IEC 62439-3) */
            return 0.0;   /* Bumpless, seamless redundancy */
        default:
            return 1500.0;
    }
}

/*===========================================================================
 * L6: Network Segment Sizing
 *===========================================================================*/

/**
 * @brief Recommend network segmentation for a DCS system.
 *
 * A network segment should not exceed:
 *   - 50 nodes for control-level (Level 1) segment
 *   - 100 nodes for supervisory-level (Level 2) segment
 *   - 40% bandwidth utilization for deterministic traffic
 *
 * Segments are separated by routers/firewalls per IEC 62443.
 *
 * @param total_nodes       Total number of DCS nodes.
 * @param max_nodes_per_seg Maximum nodes per segment.
 * @param segments_needed   Output: number of segments required.
 * @param nodes_per_seg     Output: recommended nodes per segment.
 * @return                  1 on success.
 */
int dcs_recommend_segmentation(uint32_t total_nodes,
                                uint32_t max_nodes_per_seg,
                                uint32_t *segments_needed,
                                uint32_t *nodes_per_seg)
{
    if (total_nodes == 0) {
        if (segments_needed != NULL) *segments_needed = 0;
        if (nodes_per_seg != NULL) *nodes_per_seg = 0;
        return 0;
    }

    if (max_nodes_per_seg == 0) max_nodes_per_seg = 50;

    uint32_t segs = (total_nodes + max_nodes_per_seg - 1) / max_nodes_per_seg;
    uint32_t per_seg = total_nodes / segs;

    /* Distribute remainder */
    if (total_nodes % segs != 0) {
        per_seg++; /* Round up for even distribution */
    }

    if (segments_needed != NULL) *segments_needed = segs;
    if (nodes_per_seg != NULL) *nodes_per_seg = per_seg;

    return 1;
}

/**
 * @brief Calculate IEEE 802.1Q VLAN priority mapping for DCS traffic.
 *
 * DCS traffic classes and their IEEE 802.1Q Priority Code Point (PCP):
 *   PCP 7: Network control (time sync PTP, MRP frames)
 *   PCP 6: Safety-related traffic (SIL communications, PROFIsafe)
 *   PCP 5: Voice/video (not typical in DCS)
 *   PCP 4: Controlled load (cyclic I/O data)
 *   PCP 3: Excellent effort (alarms, events, SOE)
 *   PCP 2: Spare
 *   PCP 1: Background (engineering downloads)
 *   PCP 0: Best effort (OPC data, file transfers)
 *
 * @param traffic_category  DCS traffic category.
 * @return                  IEEE 802.1Q PCP value (0-7).
 */
int dcs_map_traffic_to_vlan_priority(dcs_traffic_category_t traffic_category)
{
    switch (traffic_category) {
        case DCS_TRAFFIC_SAFETY:
            return 6;
        case DCS_TRAFFIC_TIME_SYNC:
            return 7;
        case DCS_TRAFFIC_CYCLIC_IO:
            return 4;
        case DCS_TRAFFIC_ALARM_EVENT:
            return 3;
        case DCS_TRAFFIC_HMI_REFRESH:
            return 2;
        case DCS_TRAFFIC_OPC_DATA:
            return 0;
        case DCS_TRAFFIC_ENGINEERING:
            return 1;
        default:
            return 0;
    }
}

/*===========================================================================
 * L6: Real-Time Communication Guarantee
 *===========================================================================*/

/**
 * @brief Verify that cyclic I/O traffic can meet its deadline.
 *
 * For real-time control, cyclic data must arrive within one scan period.
 * This function checks if the worst-case network latency plus jitter
 * is less than the scan period.
 *
 * Condition: latency + jitter < scan_period * deadline_factor
 *
 * where deadline_factor is typically:
 *   0.5 for critical control (data must arrive by mid-scan)
 *   0.8 for regulatory control
 *   1.0 for monitoring (data must arrive by next scan)
 *
 * @param latency_us         Worst-case network latency (µs).
 * @param jitter_us          Network jitter (µs).
 * @param scan_period_ms     Controller scan period (ms).
 * @param deadline_factor    Allowed fraction of scan period (0-1).
 * @return                   1 if deadline can be met, 0 otherwise.
 */
int dcs_verify_realtime_deadline(double latency_us,
                                  double jitter_us,
                                  double scan_period_ms,
                                  double deadline_factor)
{
    if (scan_period_ms <= 0.0) return 0;
    if (deadline_factor <= 0.0 || deadline_factor > 1.0) deadline_factor = 0.8;

    double available_us = scan_period_ms * 1000.0 * deadline_factor;
    double total_delay_us = latency_us + jitter_us;

    return (total_delay_us <= available_us) ? 1 : 0;
}

/**
 * @brief Calculate the maximum scan rate supported by network bandwidth.
 *
 * For cyclic I/O with N tags, each tag transferring S bytes:
 *
 * Max scan rate (Hz) = bandwidth_bps / (N * S * 8 * overhead_factor)
 *
 * @param num_tags            Number of real-time data tags.
 * @param bytes_per_tag       Bytes per tag (value + quality + timestamp).
 * @param bandwidth_mbps      Available bandwidth in Mbps.
 * @param overhead_factor     Protocol overhead (1.2 = 20% overhead).
 * @return                    Maximum scan rate in Hz (scans per second).
 */
double dcs_max_scan_rate_from_bandwidth(uint32_t num_tags,
                                         uint32_t bytes_per_tag,
                                         double bandwidth_mbps,
                                         double overhead_factor)
{
    if (num_tags == 0 || bytes_per_tag == 0 || bandwidth_mbps <= 0.0) return 0.0;
    if (overhead_factor <= 0.0) overhead_factor = 1.0;

    double total_bytes_per_scan = (double)num_tags * (double)bytes_per_tag
                                 * overhead_factor;
    double total_bits_per_scan = total_bytes_per_scan * 8.0;
    double bandwidth_bps = bandwidth_mbps * 1e6;

    double max_rate_hz = bandwidth_bps / total_bits_per_scan;

    return max_rate_hz;
}
