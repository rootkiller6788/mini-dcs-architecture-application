#include "delta_v_communication.h"
#include <string.h>
#include <stdio.h>

void delta_v_acn_packet_init(delta_v_acn_packet_t *pkt, delta_v_acn_msg_type_t type)
{
    if (!pkt) return;
    memset(pkt, 0, sizeof(delta_v_acn_packet_t));
    pkt->delta_v_version = 14;
    pkt->msg_type = type;
    pkt->priority = DELTAV_ACN_PRIORITY_NORMAL;
}

bool delta_v_acn_packet_validate(const delta_v_acn_packet_t *pkt)
{
    if (!pkt) return false;
    if (pkt->source_node_id == 0 || pkt->dest_node_id == 0) return false;
    if (pkt->payload_length > DELTAV_ACN_MAX_PAYLOAD) return false;
    return true;
}

uint32_t delta_v_acn_crc32_calculate(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0) return 0xFFFFFFFF;
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    return crc ^ 0xFFFFFFFF;
}

uint16_t delta_v_modbus_crc16_calculate(const uint8_t *data, uint8_t length)
{
    if (!data || length == 0) return 0xFFFF;
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    return crc;
}

uint16_t delta_v_modbus_crc16_ccitt_calculate(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0) return 0xFFFF;
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
    }
    return crc;
}

bool delta_v_opc_tag_read(delta_v_opc_tag_t *tag)
{
    if (!tag || !tag->scan_active) return false;
    if (tag->quality == DELTAV_OPC_QUALITY_BAD ||
        tag->quality == DELTAV_OPC_QUALITY_BAD_COMM_FAIL ||
        tag->quality == DELTAV_OPC_QUALITY_BAD_OUT_OF_SVC)
        return false;
    return true;
}

bool delta_v_opc_tag_write(delta_v_opc_tag_t *tag, const void *value)
{
    if (!tag || !value || !tag->scan_active) return false;
    return true;
}

void delta_v_opc_server_init(delta_v_opc_server_config_t *opc)
{
    if (!opc) return;
    memset(opc, 0, sizeof(delta_v_opc_server_config_t));
    strncpy(opc->server_name, "DeltaV.OPC.1", sizeof(opc->server_name)-1);
    opc->tcp_port = 135;
    opc->ua_port = 4840;
    opc->max_tags = 50000;
    opc->da_enabled = true;
    opc->ua_enabled = false;
}

void delta_v_modbus_build_request(delta_v_modbus_request_t *req, uint8_t addr,
    delta_v_modbus_func_t func, uint16_t start, uint16_t qty)
{
    if (!req) return;
    memset(req, 0, sizeof(delta_v_modbus_request_t));
    req->device_address = addr;
    req->function_code = func;
    req->start_address = start;
    req->quantity = qty;
    req->data[0] = (uint8_t)(start >> 8);
    req->data[1] = (uint8_t)(start & 0xFF);
    req->data[2] = (uint8_t)(qty >> 8);
    req->data[3] = (uint8_t)(qty & 0xFF);
    req->data_length = 4;
}

bool delta_v_modbus_parse_response(const delta_v_modbus_response_t *resp, uint8_t *out, uint16_t *len)
{
    if (!resp || !out || !len) return false;
    if (resp->error) return false;
    *len = (resp->data_length < 252) ? resp->data_length : 252;
    memcpy(out, resp->data, *len);
    return true;
}

bool delta_v_modbus_device_poll(delta_v_modbus_device_t *dev)
{
    if (!dev || !dev->active) return false;
    dev->successful_polls++;
    return true;
}

void delta_v_ff_h1_segment_init(delta_v_ff_h1_segment_t *seg, uint8_t num)
{
    if (!seg) return;
    memset(seg, 0, sizeof(delta_v_ff_h1_segment_t));
    seg->segment_number = num;
    seg->link_schedule_time_us = 1000000;
}

bool delta_v_ff_h1_add_device(delta_v_ff_h1_segment_t *seg, const delta_v_ff_h1_device_t *dev)
{
    if (!seg || !dev || seg->device_count >= 16) return false;
    seg->devices[seg->device_count] = *dev;
    seg->device_count++;
    return true;
}

uint32_t delta_v_ff_h1_calculate_macrocycle(const delta_v_ff_h1_segment_t *seg)
{
    if (!seg) return 0;
    uint32_t scheduled = 0;
    for (uint16_t i = 0; i < seg->device_count; i++) {
        if (seg->devices[i].link_active)
            scheduled += seg->devices[i].macrocycle_us;
    }
    return scheduled + (scheduled * 20) / 100;
}

bool delta_v_eip_device_connect(delta_v_eip_device_t *dev)
{
    if (!dev) return false;
    dev->connected = true;
    dev->packet_count = 0;
    dev->timeout_count = 0;
    return true;
}

bool delta_v_eip_device_read(delta_v_eip_device_t *dev, uint8_t *data, uint16_t len)
{
    if (!dev || !data || !dev->connected || len > dev->input_size) return false;
    dev->packet_count++;
    return true;
}

const char *delta_v_acn_msg_type_to_string(delta_v_acn_msg_type_t type) {
    static const char *s[] = {"Heartbeat","TimeSync","Download","Alarm","Event","Trend",
        "CtrlReq","CtrlResp","StationStat","LicenseCk","Diagnostic","SecChallenge","CfgSync","BatchCmd","BatchResp"};
    return (type <= DELTAV_ACN_BATCH_RESPONSE) ? s[type] : "Unknown";
}

const char *delta_v_opc_quality_to_string(delta_v_opc_quality_t q) {
    static const char *s[] = {"GOOD","GOOD_LCL_OVRD","UNCERTAIN","UNC_LAST_KNOWN","BAD","BAD_COMM","BAD_OOS","BAD_SENSOR"};
    for (int i = 0; i < 8; i++) {
        static const uint8_t vals[] = {0xC0,0xD8,0x40,0x50,0x00,0x18,0x28,0x10};
        if ((uint8_t)q == vals[i]) return s[i];
    }
    return "Unknown";
}

const char *delta_v_modbus_func_to_string(delta_v_modbus_func_t func) {
    static const char *s[] = {"","RdCoils","RdDiscIn","RdHoldReg","RdInReg","WrSingleCoil",
        "WrSingleReg","","","","","","","","","WrMultiCoils","WrMultiRegs"};
    if (func >= 1 && func <= 16 && s[func][0] != '\0') return s[func];
    return "Unknown";
}

double delta_v_modbus_calculate_poll_success_rate(const delta_v_modbus_device_t *dev)
{
    if (!dev || dev->successful_polls + dev->failed_polls == 0) return 0.0;
    return (double)dev->successful_polls / (double)(dev->successful_polls + dev->failed_polls) * 100.0;
}

bool delta_v_modbus_diagnose_device(const delta_v_modbus_device_t *dev)
{
    if (!dev || !dev->active) return false;
    double success_rate = delta_v_modbus_calculate_poll_success_rate(dev);
    return (success_rate > 95.0 && dev->timeout_count < 100);
}

uint32_t delta_v_acn_estimate_network_load(const delta_v_acn_packet_t *pkts, uint16_t pkt_count)
{
    if (!pkts || pkt_count == 0) return 0;
    uint32_t total_bits = 0;
    for (uint16_t i = 0; i < pkt_count; i++)
        total_bits += (DELTAV_ACN_MAX_PAYLOAD + 42) * 8;
    return total_bits;
}

uint16_t delta_v_ff_h1_count_active_devices(const delta_v_ff_h1_segment_t *seg)
{
    if (!seg) return 0;
    uint16_t count = 0;
    for (uint16_t i = 0; i < seg->device_count; i++) {
        if (seg->devices[i].link_active) count++;
    }
    return count;
}

bool delta_v_eip_device_diagnose(const delta_v_eip_device_t *dev)
{
    if (!dev) return false;
    if (!dev->connected) return false;
    double timeout_rate = (dev->packet_count > 0) ? (double)dev->timeout_count / (double)dev->packet_count : 0.0;
    return (timeout_rate < 0.01);
}

double delta_v_opc_server_calculate_scan_load(const delta_v_opc_server_config_t *opc)
{
    if (!opc) return 0.0;
    return (double)opc->active_tags / (double)opc->max_tags * 100.0;
}
