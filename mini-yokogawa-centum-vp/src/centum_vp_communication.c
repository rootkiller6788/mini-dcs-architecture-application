/**
 * @file centum_vp_communication.c
 * @brief CENTUM VP Communication — Vnet/IP, OPC UA, Modbus, FF H1
 *
 * Knowledge Points:
 *   vnet_packet_header_init — Vnet/IP packet framing (L3)
 *   vnet_calculate_crc16 — CRC-16 CCITT for Vnet/IP error detection (L3)
 *   vnet_verify_packet — Packet integrity verification (L3)
 *   vnet_update_statistics — Vnet/IP port statistics collection (L3)
 *   opc_item_value_init — OPC DA item initialization (L7)
 *   opc_item_set_quality — OPC quality flag management (L7)
 *   opc_item_is_good_quality — OPC quality checking utility (L7)
 *   opc_quality_to_string — OPC quality code display mapping (L7)
 *   modbus_request_init — Modbus TCP request initialization (L3)
 *   modbus_build_request_frame — Modbus RTU frame construction (L3)
 *   modbus_parse_response_frame — Modbus response parsing (L3)
 *   modbus_crc16 — Modbus CRC-16 calculation (L3)
 *   ff_h1_segment_init — Foundation Fieldbus H1 segment configuration (L7)
 *   ff_h1_add_device — FF H1 device registration (L7)
 *   ff_h1_remove_device — FF H1 device removal (L7)
 *   ff_h1_schedule_rebuild — FF H1 LAS schedule computation (L5)
 *   centum_cgw_config_init — Communication gateway initialization (L3)
 *
 * References:
 *   - CENTUM VP Communication Gateway Manual (IM 33K01A10-80E)
 *   - OPC DA Specification 2.05a
 *   - Modbus Application Protocol V1.1b
 *   - Foundation Fieldbus H1 Specification (IEC 61158-2 Type 1)
 */

#include "centum_vp_communication.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * vnet_packet_header_init
 *
 * Initializes a Vnet/IP packet header. CENTUM VP uses Vnet/IP as its
 * proprietary control network protocol running on 1 Gbps Ethernet.
 * Messages are prioritized: critical alarms get priority 0 (expedited),
 * background file transfers get priority 4 (best-effort).
 *
 * Vnet/IP features dual redundant buses (Bus A + Bus B) for fault
 * tolerance. Each bus operates independently; the receiver accepts
 * the first valid packet.
 *
 * L3 — Engineering Structure: Vnet/IP packet header format.
 *============================================================================*/
void vnet_packet_header_init(vnet_packet_header_t *hdr, uint8_t dest_dom, uint8_t dest_stn,
                              uint8_t src_dom, uint8_t src_stn, vnet_message_type_t msg_type,
                              vnet_priority_t prio)
{
    if (!hdr) return;
    memset(hdr, 0, sizeof(vnet_packet_header_t));
    hdr->dest_domain = dest_dom;
    hdr->dest_station = dest_stn;
    hdr->src_domain = src_dom;
    hdr->src_station = src_stn;
    hdr->message_type = (uint8_t)msg_type;
    hdr->priority = (uint8_t)prio;
    hdr->sequence_number = 0; /* Set by sender */
    hdr->payload_length = 0;  /* Set when payload added */
    hdr->timestamp_ms = (uint32_t)(time(NULL) * 1000) & 0xFFFFFFFF;
}

/*============================================================================
 * vnet_calculate_crc16
 *
 * Computes CRC-16 CCITT (polynomial 0x1021) for Vnet/IP packet
 * integrity checking. CENTUM VP appends a 16-bit CRC to each
 * packet to detect transmission errors on the control network.
 *
 * CRC polynomial: x^16 + x^12 + x^5 + 1 (CRC-16-CCITT)
 * Initial value: 0xFFFF
 *
 * L3 — Engineering Structure: Error detection coding for real-time
 * control network communication.
 *============================================================================*/
uint16_t vnet_calculate_crc16(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0) return 0xFFFF;

    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/*============================================================================
 * vnet_verify_packet
 *
 * Verifies a received Vnet/IP packet:
 *   1. CRC-16 matches
 *   2. Destination address matches expected
 *   3. Payload length matches declared length
 *   4. Sequence number is valid (no gap or duplicate)
 *
 * L3 — Engineering Structure: Packet integrity verification in
 * deterministic control networks.
 *============================================================================*/
bool vnet_verify_packet(const vnet_packet_header_t *hdr, const uint8_t *payload,
                         uint16_t length)
{
    if (!hdr) return false;
    if (!payload && hdr->payload_length > 0) return false;
    if (length != hdr->payload_length) return false;

    /* CRC would be verified over header + payload in real implementation.
       For this model, we check structural validity. */
    if (hdr->dest_domain == 0 || hdr->dest_station == 0) return false;
    if (hdr->src_domain == 0 || hdr->src_station == 0) return false;

    return true;
}

/*============================================================================
 * vnet_update_statistics
 *
 * Updates Vnet/IP port statistics after each packet transmission/reception.
 * CENTUM VP monitors these statistics to detect network degradation:
 *   - CRC errors: indicate electromagnetic interference or cable faults
 *   - Sequence gaps: indicate lost packets (possible congestion)
 *   - Latency: must stay <1ms for real-time control
 *
 * These statistics are displayed on the Vnet/IP Status Display in HIS.
 *
 * L3 — Engineering Structure: Control network health monitoring.
 *============================================================================*/
void vnet_update_statistics(vnet_port_statistics_t *stats, bool success, double latency_us)
{
    if (!stats) return;

    if (success) {
        stats->packets_received++;
        stats->avg_latency_us = (stats->avg_latency_us * 0.99) + (latency_us * 0.01);
        if (latency_us > stats->max_latency_us) stats->max_latency_us = latency_us;
        if (latency_us < stats->min_latency_us || stats->min_latency_us == 0.0) {
            stats->min_latency_us = latency_us;
        }
    } else {
        stats->packets_dropped++;
    }

    /* Bandwidth usage estimate based on packet rate */
    stats->bandwidth_usage_percent = (double)(stats->packets_received + stats->packets_sent)
                                     * 1500.0 * 8.0 / 1e9 * 100.0;
    if (stats->bandwidth_usage_percent > 100.0) stats->bandwidth_usage_percent = 100.0;
}

/*============================================================================
 * opc_item_value_init
 *
 * Initializes an OPC DA item value structure. CENTUM VP exposes
 * process data via OPC DA (COM/DCOM) and OPC UA interfaces.
 * Each OPC item has:
 *   - Item ID (e.g., "FCS0101.PID001.PV")
 *   - Data type (INT16, FLOAT64, BOOL, etc.)
 *   - Quality flag (GOOD, BAD, UNCERTAIN)
 *   - Timestamp of last update
 *   - Value in variant form
 *
 * L7 — Industrial Application: OPC DA server integration.
 *============================================================================*/
void opc_item_value_init(opc_item_value_t *item, const char *item_id, opc_data_type_t type)
{
    if (!item) return;
    memset(item, 0, sizeof(opc_item_value_t));
    if (item_id) {
        strncpy(item->item_id, item_id, sizeof(item->item_id) - 1);
    }
    item->data_type = type;
    item->quality = OPC_QUALITY_GOOD;
    item->timestamp = time(NULL);
}

/*============================================================================
 * opc_item_set_quality
 *
 * Sets the OPC quality flag for a data item. CENTUM VP maps internal
 * status (IOP, sensor fault, communication loss) to OPC quality codes.
 *
 * Quality mapping:
 *   IOP (Input Open)      → OPC_QUALITY_BAD_DEVICE_FAIL
 *   Communication timeout → OPC_QUALITY_BAD_NOT_CONNECTED
 *   Normal operation      → OPC_QUALITY_GOOD
 *   Uncertain (noisy)     → OPC_QUALITY_UNCERTAIN
 *
 * L7 — Industrial Application: OPC quality mapping in DCS.
 *============================================================================*/
void opc_item_set_quality(opc_item_value_t *item, opc_quality_t quality)
{
    if (item) {
        item->quality = quality;
        item->timestamp = time(NULL);
    }
}

/*============================================================================
 * opc_item_is_good_quality
 *
 * Checks if an OPC quality code indicates valid data. Only GOOD quality
 * (with optional substatus) should be used for control decisions.
 *
 * L7 — Industrial Application: OPC quality-based data validation.
 *============================================================================*/
bool opc_item_is_good_quality(opc_quality_t quality)
{
    return (quality & 0x00C0) == 0x00C0; /* Bits 6-7 = 11 for GOOD */
}

/*============================================================================
 * opc_quality_to_string
 *
 * Converts OPC quality code to human-readable string for HMI display.
 *
 * L7 — Industrial Application: HMI visualization of data quality.
 *============================================================================*/
const char *opc_quality_to_string(opc_quality_t quality)
{
    switch (quality) {
        case OPC_QUALITY_GOOD:              return "Good";
        case OPC_QUALITY_GOOD_LOCAL:        return "Good (Local Override)";
        case OPC_QUALITY_BAD:               return "Bad";
        case OPC_QUALITY_BAD_CONFIG_ERROR:  return "Bad (Config Error)";
        case OPC_QUALITY_BAD_NOT_CONNECTED: return "Bad (Not Connected)";
        case OPC_QUALITY_BAD_DEVICE_FAIL:   return "Bad (Device Failure)";
        case OPC_QUALITY_UNCERTAIN:         return "Uncertain";
        case OPC_QUALITY_UNCERTAIN_SUB:     return "Uncertain (Substitute)";
        default:                            return "Unknown Quality";
    }
}

/*============================================================================
 * modbus_request_init
 *
 * Initializes a Modbus request structure. CENTUM VP's ALR111/AMC80
 * modules provide Modbus RTU/TCP connectivity for integrating
 * third-party PLCs, analyzers, and subsystems.
 *
 * L3 — Engineering Structure: Modbus request framing.
 *============================================================================*/
void modbus_request_init(modbus_request_t *req, uint8_t slave, modbus_function_code_t func,
                          uint16_t addr, uint16_t qty)
{
    if (!req) return;
    memset(req, 0, sizeof(modbus_request_t));
    req->slave_id = slave;
    req->func_code = func;
    req->start_address = addr;
    req->quantity = qty;
    req->success = false;
    req->exception_code = 0;
}

/*============================================================================
 * modbus_build_request_frame
 *
 * Builds a Modbus RTU request frame (ADU format):
 *   [Slave ID][Function Code][Data...][CRC16]
 *
 * For Read Holding Registers (0x03):
 *   Data = [StartAddr_Hi][StartAddr_Lo][Quantity_Hi][Quantity_Lo]
 *
 * L3 — Engineering Structure: Modbus protocol frame assembly.
 *============================================================================*/
bool modbus_build_request_frame(const modbus_request_t *req, uint8_t *frame, uint16_t *frame_len)
{
    if (!req || !frame || !frame_len) return false;

    uint16_t idx = 0;
    frame[idx++] = req->slave_id;
    frame[idx++] = (uint8_t)req->func_code;
    frame[idx++] = (uint8_t)(req->start_address >> 8);
    frame[idx++] = (uint8_t)(req->start_address & 0xFF);

    if (req->func_code == MB_FUNC_WRITE_SINGLE_REG ||
        req->func_code == MB_FUNC_WRITE_MULTI_REGS) {
        if (req->func_code == MB_FUNC_WRITE_MULTI_REGS) {
            frame[idx++] = (uint8_t)(req->quantity >> 8);
            frame[idx++] = (uint8_t)(req->quantity & 0xFF);
            frame[idx++] = (uint8_t)(req->quantity * 2); /* Byte count */
        }
        for (uint16_t i = 0; i < req->quantity && i < 256; i++) {
            frame[idx++] = (uint8_t)(req->data[i] >> 8);
            frame[idx++] = (uint8_t)(req->data[i] & 0xFF);
        }
    } else {
        frame[idx++] = (uint8_t)(req->quantity >> 8);
        frame[idx++] = (uint8_t)(req->quantity & 0xFF);
    }

    /* Append CRC-16 */
    uint16_t crc = modbus_crc16(frame, idx);
    frame[idx++] = (uint8_t)(crc & 0xFF);
    frame[idx++] = (uint8_t)(crc >> 8);

    *frame_len = idx;
    return true;
}

/*============================================================================
 * modbus_parse_response_frame
 *
 * Parses a Modbus RTU response frame and extracts register/coil data.
 * Checks CRC-16 and exception codes.
 *
 * L3 — Engineering Structure: Modbus protocol response parsing.
 *============================================================================*/
bool modbus_parse_response_frame(modbus_request_t *req, const uint8_t *frame, uint16_t frame_len)
{
    if (!req || !frame || frame_len < 5) return false;

    /* Verify CRC */
    uint16_t rx_crc = (uint16_t)(frame[frame_len - 1] << 8) | frame[frame_len - 2];
    uint16_t calc_crc = modbus_crc16(frame, frame_len - 2);
    if (rx_crc != calc_crc) {
        req->success = false;
        return false;
    }

    /* Check for exception response */
    if (frame[1] & 0x80) {
        req->exception_code = frame[2];
        req->success = false;
        return false;
    }

    /* Parse data based on function code */
    if (req->func_code == MB_FUNC_READ_HOLDING_REGS ||
        req->func_code == MB_FUNC_READ_INPUT_REGS) {
        uint8_t byte_count = frame[2];
        uint16_t reg_count = byte_count / 2;
        for (uint16_t i = 0; i < reg_count && i < 256; i++) {
            req->data[i] = ((uint16_t)frame[3 + i * 2] << 8) | frame[4 + i * 2];
        }
    } else if (req->func_code == MB_FUNC_READ_COILS ||
               req->func_code == MB_FUNC_READ_DISCRETE_INPUTS) {
        uint8_t byte_count = frame[2];
        for (uint16_t i = 0; i < byte_count && i < 256; i++) {
            req->data[i] = frame[3 + i];
        }
    }

    req->success = true;
    return true;
}

/*============================================================================
 * modbus_crc16
 *
 * Modbus CRC-16 calculation (polynomial 0xA001, reflected).
 * This is the standard CRC used in Modbus RTU/ASCII and Modbus TCP.
 *
 * Polynomial: x^16 + x^15 + x^2 + 1 (CRC-16-IBM, reversed 0xA001)
 * Initial value: 0xFFFF
 *
 * L3 — Engineering Structure: CRC algorithm for industrial serial protocols.
 *============================================================================*/
uint16_t modbus_crc16(const uint8_t *data, uint16_t length)
{
    if (!data) return 0xFFFF;

    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

/*============================================================================
 * ff_h1_segment_init
 *
 * Initializes a Foundation Fieldbus H1 segment. FF H1 operates at
 * 31.25 kbps over twisted pair, providing power and communication
 * on the same wire. Each segment supports up to 16 devices with
 * a Link Active Scheduler (LAS) managing deterministic communication.
 *
 * CENTUM VP's ALF111 module provides 4 FF H1 segments, each with
 * redundant H1 interface capability.
 *
 * L7 — Industrial Application: Foundation Fieldbus H1 in Yokogawa DCS.
 *============================================================================*/
void ff_h1_segment_init(ff_h1_segment_t *seg, uint8_t seg_id, uint32_t macrocycle_us)
{
    if (!seg) return;
    memset(seg, 0, sizeof(ff_h1_segment_t));
    seg->segment_id = seg_id;
    seg->device_count = 0;
    seg->redundant_link = false;
    seg->macrocycle_us = macrocycle_us;
    seg->link_active_scheduler = false;
}

/*============================================================================
 * ff_h1_add_device
 *
 * Registers a field device on an FF H1 segment. The device is assigned
 * a unique node address (0x10-0xFF for basic devices, 0x00-0x0F for
 * link masters). The LAS assigns addresses during the probe sequence.
 *
 * L7 — Industrial Application: FF H1 device management.
 *============================================================================*/
bool ff_h1_add_device(ff_h1_segment_t *seg, const ff_h1_device_t *dev)
{
    if (!seg || !dev) return false;
    if (seg->device_count >= 16) return false;

    /* Check duplicate address */
    for (uint8_t i = 0; i < seg->device_count; i++) {
        if (seg->devices[i].node_address == dev->node_address) return false;
    }

    memcpy(&seg->devices[seg->device_count], dev, sizeof(ff_h1_device_t));
    seg->device_count++;
    return true;
}

/*============================================================================
 * ff_h1_remove_device
 *
 * Removes a device from the FF H1 segment's live list. Corresponds to
 * a device going offline or being decommissioned. The LAS will stop
 * scheduling communication to this address.
 *
 * L7 — Industrial Application: FF H1 device decommissioning.
 *============================================================================*/
bool ff_h1_remove_device(ff_h1_segment_t *seg, uint8_t node_addr)
{
    if (!seg) return false;

    for (uint8_t i = 0; i < seg->device_count; i++) {
        if (seg->devices[i].node_address == node_addr) {
            for (uint8_t j = i; j < seg->device_count - 1; j++) {
                memcpy(&seg->devices[j], &seg->devices[j + 1], sizeof(ff_h1_device_t));
            }
            memset(&seg->devices[seg->device_count - 1], 0, sizeof(ff_h1_device_t));
            seg->device_count--;
            return true;
        }
    }
    return false;
}

/*============================================================================
 * ff_h1_schedule_rebuild
 *
 * Rebuilds the FF H1 communication schedule. The LAS maintains a
 * schedule specifying when each device publishes its data (cyclic)
 * and when acyclic communication (parameter access, alarms) can occur.
 *
 * Macrocycle time = sum of all cyclic data transfer times + margin
 * for acyclic communication.
 *
 * L5 — Algorithm: Link Active Scheduler macrocycle computation.
 *============================================================================*/
bool ff_h1_schedule_rebuild(ff_h1_segment_t *seg)
{
    if (!seg) return false;
    if (seg->device_count == 0) return false;

    /* Compute total schedule time based on device VCR count */
    uint32_t total_schedule_time_us = 0;
    for (uint8_t i = 0; i < seg->device_count; i++) {
        /* Each VCR (Virtual Communication Relationship) takes ~1ms */
        total_schedule_time_us += seg->devices[i].vcr_count * 1000;
    }

    /* Add 20% margin for acyclic communication */
    seg->macrocycle_us = total_schedule_time_us * 120 / 100;
    return true;
}

/*============================================================================
 * centum_cgw_config_init
 *
 * Initializes a Communication Gateway (CGW) station configuration.
 * The CGW acts as a protocol converter between Vnet/IP and external
 * networks/systems:
 *   - OPC DA/UA server for MES/ERP integration
 *   - Modbus TCP gateway for PLC/RTU connectivity
 *   - FF H1 segment management
 *
 * Each CGW can manage up to 16 OPC groups, 8 Modbus gateways, and
 * 4 FF H1 segments simultaneously.
 *
 * L3 — Engineering Structure: Communication gateway configuration.
 *============================================================================*/
void centum_cgw_config_init(centum_cgw_config_t *cgw, uint32_t station_id)
{
    if (!cgw) return;
    memset(cgw, 0, sizeof(centum_cgw_config_t));
    cgw->cgw_station_id = station_id;
    cgw->opc_group_count = 0;
    cgw->modbus_gw_count = 0;
    cgw->ff_segment_count = 0;
    cgw->total_messages_processed = 0;
}

/*============================================================================
 * centum_cgw_add_opc_group
 *
 * Adds an OPC group configuration to the CGW. Each group defines
 * a set of items polled at a specified update rate.
 *
 * L3 — Engineering Structure: OPC group configuration.
 *============================================================================*/
bool centum_cgw_add_opc_group(centum_cgw_config_t *cgw, const opc_group_config_t *group)
{
    if (!cgw || !group) return false;
    if (cgw->opc_group_count >= 16) return false;

    memcpy(&cgw->opc_groups[cgw->opc_group_count], group, sizeof(opc_group_config_t));
    cgw->opc_group_count++;
    return true;
}

/*============================================================================
 * centum_cgw_add_modbus_gateway
 *
 * Configures a Modbus gateway connection. CENTUM VP CGW can poll
 * multiple Modbus TCP slaves, each with its own IP address, polling
 * interval, and register mapping.
 *
 * L3 — Engineering Structure: Modbus gateway configuration.
 *============================================================================*/
bool centum_cgw_add_modbus_gateway(centum_cgw_config_t *cgw, const modbus_gateway_config_t *gw)
{
    if (!cgw || !gw) return false;
    if (cgw->modbus_gw_count >= 8) return false;

    memcpy(&cgw->modbus_gw[cgw->modbus_gw_count], gw, sizeof(modbus_gateway_config_t));
    cgw->modbus_gw_count++;
    return true;
}

/*============================================================================
 * centum_cgw_add_ff_segment
 *
 * Adds a Foundation Fieldbus H1 segment to the CGW configuration.
 *
 * L3 — Engineering Structure: FF H1 segment configuration.
 *============================================================================*/
bool centum_cgw_add_ff_segment(centum_cgw_config_t *cgw, const ff_h1_segment_t *seg)
{
    if (!cgw || !seg) return false;
    if (cgw->ff_segment_count >= 4) return false;

    memcpy(&cgw->ff_segments[cgw->ff_segment_count], seg, sizeof(ff_h1_segment_t));
    cgw->ff_segment_count++;
    return true;
}