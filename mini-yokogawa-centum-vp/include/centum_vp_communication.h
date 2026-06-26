#ifndef CENTUM_VP_COMMUNICATION_H
#define CENTUM_VP_COMMUNICATION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "centum_vp_system.h"

#define VNET_IP_DEFAULT_SUBNET   0xAC100000U
#define VNET_IP_SUBNET_MASK      0xFFFFFF00U
#define VNET_IP_BROADCAST_ADDR   0xAC10FFFFU
#define VNET_IP_MAX_PACKET_SIZE  1500U
#define VNET_IP_CONTROL_PORT     32000U
#define VNET_IP_DATA_PORT        32001U
#define VNET_IP_TIME_SYNC_PORT   32002U

typedef enum {
    VNET_MSG_TIME_SYNC     = 0x01,
    VNET_MSG_PROCESS_DATA  = 0x02,
    VNET_MSG_ALARM_EVENT   = 0x03,
    VNET_MSG_TREND_DATA    = 0x04,
    VNET_MSG_COMMAND_REQ   = 0x05,
    VNET_MSG_COMMAND_ACK   = 0x06,
    VNET_MSG_DOWNLOAD      = 0x07,
    VNET_MSG_SYSTEM_STATUS = 0x08,
    VNET_MSG_FILE_TRANSFER = 0x09,
    VNET_MSG_DIAGNOSTIC    = 0x0A
} vnet_message_type_t;

typedef enum {
    VNET_PRIORITY_CRITICAL = 0,
    VNET_PRIORITY_HIGH     = 1,
    VNET_PRIORITY_MEDIUM   = 2,
    VNET_PRIORITY_LOW      = 3,
    VNET_PRIORITY_BACKGROUND = 4
} vnet_priority_t;

typedef struct __attribute__((packed)) {
    uint8_t     dest_domain;
    uint8_t     dest_station;
    uint8_t     src_domain;
    uint8_t     src_station;
    uint8_t     message_type;
    uint8_t     priority;
    uint16_t    sequence_number;
    uint16_t    payload_length;
    uint32_t    timestamp_ms;
    uint16_t    crc16;
} vnet_packet_header_t;

typedef struct {
    uint32_t    packets_sent;
    uint32_t    packets_received;
    uint32_t    packets_dropped;
    uint32_t    crc_errors;
    uint32_t    sequence_gaps;
    double      avg_latency_us;
    double      max_latency_us;
    double      min_latency_us;
    double      bandwidth_usage_percent;
    uint32_t    retransmissions;
    bool        bus_a_active;
    bool        bus_b_active;
} vnet_port_statistics_t;

typedef enum {
    OPC_ITEM_INT16   = 0,
    OPC_ITEM_UINT16  = 1,
    OPC_ITEM_INT32   = 2,
    OPC_ITEM_UINT32  = 3,
    OPC_ITEM_FLOAT32 = 4,
    OPC_ITEM_FLOAT64 = 5,
    OPC_ITEM_BOOL    = 6,
    OPC_ITEM_STRING  = 7,
    OPC_ITEM_BYTE    = 8
} opc_data_type_t;

typedef enum {
    OPC_QUALITY_GOOD              = 0x00C0,
    OPC_QUALITY_GOOD_LOCAL        = 0x00D0,
    OPC_QUALITY_BAD               = 0x0000,
    OPC_QUALITY_BAD_CONFIG_ERROR  = 0x0004,
    OPC_QUALITY_BAD_NOT_CONNECTED = 0x0008,
    OPC_QUALITY_BAD_DEVICE_FAIL   = 0x000C,
    OPC_QUALITY_UNCERTAIN         = 0x0040,
    OPC_QUALITY_UNCERTAIN_SUB     = 0x0058
} opc_quality_t;

typedef struct {
    char            item_id[256];
    opc_data_type_t data_type;
    opc_quality_t   quality;
    time_t          timestamp;
    union {
        int16_t     v_i16;
        uint16_t    v_u16;
        int32_t     v_i32;
        uint32_t    v_u32;
        float       v_f32;
        double      v_f64;
        bool        v_bool;
        char        v_string[256];
        uint8_t     v_byte;
    } value;
} opc_item_value_t;

typedef struct {
    uint16_t    item_count;
    opc_item_value_t items[64];
    uint32_t    update_interval_ms;
    bool        active;
    double      scan_rate_hz;
} opc_group_config_t;

typedef enum {
    MB_FUNC_READ_COILS            = 0x01,
    MB_FUNC_READ_DISCRETE_INPUTS  = 0x02,
    MB_FUNC_READ_HOLDING_REGS     = 0x03,
    MB_FUNC_READ_INPUT_REGS       = 0x04,
    MB_FUNC_WRITE_SINGLE_COIL     = 0x05,
    MB_FUNC_WRITE_SINGLE_REG      = 0x06,
    MB_FUNC_WRITE_MULTI_COILS     = 0x0F,
    MB_FUNC_WRITE_MULTI_REGS      = 0x10
} modbus_function_code_t;

typedef struct {
    uint8_t     slave_id;
    modbus_function_code_t func_code;
    uint16_t    start_address;
    uint16_t    quantity;
    uint16_t    data[256];
    bool        success;
    uint8_t     exception_code;
} modbus_request_t;

typedef struct {
    char        connection_name[32];
    uint32_t    ip_address;
    uint16_t    tcp_port;
    uint8_t     unit_id;
    uint32_t    timeout_ms;
    uint32_t    poll_interval_ms;
    bool        connected;
    uint16_t    holding_regs[1024];
    bool        coil_states[1024];
} modbus_gateway_config_t;

typedef enum {
    FF_H1_LINK_MASTER        = 0,
    FF_H1_BASIC_DEVICE       = 1,
    FF_H1_BRIDGE             = 2
} ff_h1_device_class_t;

typedef struct {
    char        device_tag[32];
    char        device_id[32];
    ff_h1_device_class_t dev_class;
    uint8_t     node_address;
    bool        live;
    uint32_t    vcr_count;
    double      schedule_time_us;
} ff_h1_device_t;

typedef struct {
    uint8_t     segment_id;
    uint8_t     device_count;
    bool        redundant_link;
    uint32_t    macrocycle_us;
    ff_h1_device_t devices[16];
    bool        link_active_scheduler;
} ff_h1_segment_t;

typedef struct {
    vnet_port_statistics_t port_a;
    vnet_port_statistics_t port_b;
    opc_group_config_t     opc_groups[16];
    uint8_t     opc_group_count;
    modbus_gateway_config_t modbus_gw[8];
    uint8_t     modbus_gw_count;
    ff_h1_segment_t ff_segments[4];
    uint8_t     ff_segment_count;
    uint32_t    total_messages_processed;
    uint32_t    cgw_station_id;
} centum_cgw_config_t;

void vnet_packet_header_init(vnet_packet_header_t *hdr, uint8_t dest_dom, uint8_t dest_stn,
                              uint8_t src_dom, uint8_t src_stn, vnet_message_type_t msg_type,
                              vnet_priority_t prio);
uint16_t vnet_calculate_crc16(const uint8_t *data, uint16_t length);
bool vnet_verify_packet(const vnet_packet_header_t *hdr, const uint8_t *payload, uint16_t length);
void vnet_update_statistics(vnet_port_statistics_t *stats, bool success, double latency_us);

void opc_item_value_init(opc_item_value_t *item, const char *item_id, opc_data_type_t type);
void opc_item_set_quality(opc_item_value_t *item, opc_quality_t quality);
bool opc_item_is_good_quality(opc_quality_t quality);
const char *opc_quality_to_string(opc_quality_t quality);

void modbus_request_init(modbus_request_t *req, uint8_t slave, modbus_function_code_t func,
                          uint16_t addr, uint16_t qty);
bool modbus_build_request_frame(const modbus_request_t *req, uint8_t *frame, uint16_t *frame_len);
bool modbus_parse_response_frame(modbus_request_t *req, const uint8_t *frame, uint16_t frame_len);
uint16_t modbus_crc16(const uint8_t *data, uint16_t length);

void ff_h1_segment_init(ff_h1_segment_t *seg, uint8_t seg_id, uint32_t macrocycle_us);
bool ff_h1_add_device(ff_h1_segment_t *seg, const ff_h1_device_t *dev);
bool ff_h1_remove_device(ff_h1_segment_t *seg, uint8_t node_addr);
bool ff_h1_schedule_rebuild(ff_h1_segment_t *seg);

void centum_cgw_config_init(centum_cgw_config_t *cgw, uint32_t station_id);
bool centum_cgw_add_opc_group(centum_cgw_config_t *cgw, const opc_group_config_t *group);
bool centum_cgw_add_modbus_gateway(centum_cgw_config_t *cgw, const modbus_gateway_config_t *gw);
bool centum_cgw_add_ff_segment(centum_cgw_config_t *cgw, const ff_h1_segment_t *seg);

#endif