#ifndef DELTA_V_COMMUNICATION_H
#define DELTA_V_COMMUNICATION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    DELTAV_ACN_HEARTBEAT          = 0,
    DELTAV_ACN_TIME_SYNC          = 1,
    DELTAV_ACN_DOWNLOAD           = 2,
    DELTAV_ACN_ALARM              = 3,
    DELTAV_ACN_EVENT              = 4,
    DELTAV_ACN_TREND              = 5,
    DELTAV_ACN_CONTROL_REQUEST    = 6,
    DELTAV_ACN_CONTROL_RESPONSE   = 7,
    DELTAV_ACN_STATION_STATUS     = 8,
    DELTAV_ACN_LICENSE_CHECK      = 9,
    DELTAV_ACN_DIAGNOSTIC         = 10,
    DELTAV_ACN_SECURITY_CHALLENGE = 11,
    DELTAV_ACN_CONFIG_SYNC        = 12,
    DELTAV_ACN_BATCH_COMMAND      = 13,
    DELTAV_ACN_BATCH_RESPONSE     = 14
} delta_v_acn_msg_type_t;

typedef enum {
    DELTAV_ACN_PRIORITY_ALARM     = 0,
    DELTAV_ACN_PRIORITY_CONTROL   = 1,
    DELTAV_ACN_PRIORITY_SYNC      = 2,
    DELTAV_ACN_PRIORITY_NORMAL    = 3,
    DELTAV_ACN_PRIORITY_BACKGROUND = 4
} delta_v_acn_priority_t;

#define DELTAV_ACN_MAX_PAYLOAD 1472

typedef struct {
    uint8_t     dest_mac[6];
    uint8_t     src_mac[6];
    uint16_t    ether_type;
    uint16_t    delta_v_version;
    uint16_t    message_id;
    uint16_t    source_node_id;
    uint16_t    dest_node_id;
    delta_v_acn_msg_type_t msg_type;
    delta_v_acn_priority_t priority;
    uint32_t    sequence_number;
    uint32_t    timestamp_ms;
    uint16_t    payload_length;
    uint8_t     payload[DELTAV_ACN_MAX_PAYLOAD];
    uint32_t    crc32;
} delta_v_acn_packet_t;

typedef struct {
    uint64_t    master_time_ns;
    uint64_t    slave_time_ns;
    double      offset_correction_us;
    double      drift_correction_ppb;
    bool        synced;
    uint32_t    stratum;
    uint16_t    master_node_id;
} delta_v_time_sync_t;

typedef enum {
    DELTAV_OPC_BOOLEAN  = 0,
    DELTAV_OPC_INT8     = 1,
    DELTAV_OPC_INT16    = 2,
    DELTAV_OPC_INT32    = 3,
    DELTAV_OPC_UINT8    = 4,
    DELTAV_OPC_UINT16   = 5,
    DELTAV_OPC_UINT32   = 6,
    DELTAV_OPC_FLOAT    = 7,
    DELTAV_OPC_DOUBLE   = 8,
    DELTAV_OPC_STRING   = 9,
    DELTAV_OPC_DATETIME = 10,
    DELTAV_OPC_BLOB     = 11
} delta_v_opc_data_type_t;

typedef enum {
    DELTAV_OPC_QUALITY_GOOD              = 0xC0,
    DELTAV_OPC_QUALITY_GOOD_LOCAL_OVRD   = 0xD8,
    DELTAV_OPC_QUALITY_UNCERTAIN         = 0x40,
    DELTAV_OPC_QUALITY_UNCERTAIN_LAST_KNOWN = 0x50,
    DELTAV_OPC_QUALITY_BAD               = 0x00,
    DELTAV_OPC_QUALITY_BAD_COMM_FAIL     = 0x18,
    DELTAV_OPC_QUALITY_BAD_OUT_OF_SVC    = 0x28,
    DELTAV_OPC_QUALITY_BAD_SENSOR_FAIL   = 0x10
} delta_v_opc_quality_t;

typedef struct {
    char            tag_name[64];
    delta_v_opc_data_type_t data_type;
    delta_v_opc_quality_t quality;
    union {
        bool    v_bool;
        int32_t v_int32;
        double  v_double;
        char    v_string[256];
    } value;
    time_t          timestamp;
    uint16_t        node_id;
    uint32_t        module_id;
    bool            scan_active;
    uint32_t        scan_rate_ms;
} delta_v_opc_tag_t;

typedef struct {
    char            server_name[32];
    uint16_t        tcp_port;
    uint32_t        max_tags;
    uint32_t        active_tags;
    bool            ua_enabled;
    bool            da_enabled;
    uint32_t        ua_port;
    char            ua_endpoint[128];
    bool            security_enabled;
    char            certificate_path[256];
    uint32_t        client_count;
    double          scan_overrun_percent;
} delta_v_opc_server_config_t;
typedef enum {
    DELTAV_MODBUS_READ_COILS           = 1,
    DELTAV_MODBUS_READ_DISCRETE_INPUTS = 2,
    DELTAV_MODBUS_READ_HOLDING_REGS    = 3,
    DELTAV_MODBUS_READ_INPUT_REGS      = 4,
    DELTAV_MODBUS_WRITE_SINGLE_COIL    = 5,
    DELTAV_MODBUS_WRITE_SINGLE_REG     = 6,
    DELTAV_MODBUS_WRITE_MULTI_COILS    = 15,
    DELTAV_MODBUS_WRITE_MULTI_REGS     = 16
} delta_v_modbus_func_t;

typedef struct {
    uint8_t     device_address;
    delta_v_modbus_func_t function_code;
    uint16_t    start_address;
    uint16_t    quantity;
    uint8_t     data[252];
    uint8_t     data_length;
} delta_v_modbus_request_t;

typedef struct {
    uint8_t     device_address;
    delta_v_modbus_func_t function_code;
    uint8_t     data[252];
    uint8_t     data_length;
    bool        error;
    uint8_t     exception_code;
} delta_v_modbus_response_t;

typedef struct {
    char        device_name[32];
    uint8_t     slave_id;
    uint16_t    tcp_port;
    uint32_t    ip_address;
    uint32_t    poll_rate_ms;
    uint32_t    timeout_ms;
    bool        tcp_mode;
    bool        active;
    uint16_t    register_map[256];
    uint8_t     register_count;
    uint32_t    successful_polls;
    uint32_t    failed_polls;
    uint32_t    timeout_count;
} delta_v_modbus_device_t;

typedef struct {
    uint8_t     station_address;
    uint32_t    baud_rate;
    bool        master_mode;
    uint8_t     slave_count;
    uint16_t    watchdog_ms;
    uint16_t    input_size;
    uint16_t    output_size;
    uint8_t     input_data[244];
    uint8_t     output_data[244];
    bool        data_exchange_active;
    bool        diagnostic_available;
    uint8_t     diagnostic_data[16];
} delta_v_profibus_config_t;

typedef enum {
    DELTAV_FF_H1_LINK_MASTER = 0,
    DELTAV_FF_H1_BASIC       = 1,
    DELTAV_FF_H1_BRIDGE      = 2
} delta_v_ff_h1_device_class_t;

typedef struct {
    uint32_t    device_id;
    char        device_tag[32];
    delta_v_ff_h1_device_class_t device_class;
    uint8_t     vcr_count;
    uint8_t     function_block_count;
    uint8_t     transducer_block_count;
    uint32_t    macrocycle_us;
    bool        las_capable;
    bool        las_active;
    uint32_t    live_list_version;
    uint16_t    device_revision;
    bool        link_active;
} delta_v_ff_h1_device_t;

typedef struct {
    uint8_t     segment_number;
    uint16_t    device_count;
    delta_v_ff_h1_device_t devices[16];
    uint32_t    link_schedule_time_us;
    uint32_t    scheduled_time_us;
    uint32_t    unscheduled_time_us;
    double      idle_time_percent;
    bool        segment_active;
} delta_v_ff_h1_segment_t;

typedef struct {
    char        device_name[32];
    uint32_t    ip_address;
    uint16_t    slot_number;
    uint16_t    assembly_instance_input;
    uint16_t    assembly_instance_output;
    uint32_t    rpi_us;
    uint16_t    input_size;
    uint16_t    output_size;
    bool        connected;
    bool        unicast;
    uint32_t    packet_count;
    uint32_t    timeout_count;
} delta_v_eip_device_t;

void delta_v_acn_packet_init(delta_v_acn_packet_t *pkt, delta_v_acn_msg_type_t type);
bool delta_v_acn_packet_validate(const delta_v_acn_packet_t *pkt);
uint32_t delta_v_acn_crc32_calculate(const uint8_t *data, uint16_t length);
uint16_t delta_v_modbus_crc16_calculate(const uint8_t *data, uint8_t length);
uint16_t delta_v_modbus_crc16_ccitt_calculate(const uint8_t *data, uint16_t length);
bool delta_v_opc_tag_read(delta_v_opc_tag_t *tag);
bool delta_v_opc_tag_write(delta_v_opc_tag_t *tag, const void *value);
void delta_v_opc_server_init(delta_v_opc_server_config_t *opc);
void delta_v_modbus_build_request(delta_v_modbus_request_t *req, uint8_t addr, delta_v_modbus_func_t func, uint16_t start, uint16_t qty);
bool delta_v_modbus_parse_response(const delta_v_modbus_response_t *resp, uint8_t *out, uint16_t *len);
bool delta_v_modbus_device_poll(delta_v_modbus_device_t *dev);
void delta_v_ff_h1_segment_init(delta_v_ff_h1_segment_t *seg, uint8_t num);
bool delta_v_ff_h1_add_device(delta_v_ff_h1_segment_t *seg, const delta_v_ff_h1_device_t *dev);
uint32_t delta_v_ff_h1_calculate_macrocycle(const delta_v_ff_h1_segment_t *seg);
bool delta_v_eip_device_connect(delta_v_eip_device_t *dev);
bool delta_v_eip_device_read(delta_v_eip_device_t *dev, uint8_t *data, uint16_t len);

const char *delta_v_acn_msg_type_to_string(delta_v_acn_msg_type_t type);
const char *delta_v_opc_quality_to_string(delta_v_opc_quality_t q);
const char *delta_v_modbus_func_to_string(delta_v_modbus_func_t func);

#endif
