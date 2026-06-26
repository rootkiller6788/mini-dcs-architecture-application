/**
 * @file    ecs700_communication.h
 * @brief   SUPCON ECS-700 Communication — SCnet Protocol
 *
 * SCnet (System Control Network) is the redundant industrial Ethernet
 * backbone of ECS-700. It connects all system nodes (CS, OS, ES, HS)
 * through a dual-ring topology with automatic failover.
 *
 * Network Architecture:
 *   - Layer 0: Physical — 100BASE-TX or 1000BASE-T (redundant ports A/B)
 *   - Layer 1: Data Link — Modified Ethernet with deterministic scheduling
 *   - Layer 2: Transport — SCnet TPDU (Transport Protocol Data Unit)
 *   - Layer 3: Application — Data exchange, alarm, event, time sync
 *
 * SCnet Packet Types:
 *   - Real-time Data: Cyclic PV/SV/MV broadcast (highest priority)
 *   - Alarm/Event: Spontaneous alarm messages (high priority)
 *   - Configuration: Download/upload of control logic (medium priority)
 *   - Diagnostics: Health telegrams, statistics (low priority)
 *   - Time Sync: PTP (IEEE 1588) time synchronization (highest priority)
 *
 * Knowledge Coverage:
 *   L1: SCnet architecture, packet structure, node addressing
 *   L2: Cyclic vs acyclic communication, broadcast vs unicast
 *   L3: Data exchange models (producer-consumer, client-server)
 *   L4: IEEE 1588 PTP, IEC 61784-2 CPF 14 (EPA)
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#ifndef ECS700_COMMUNICATION_H
#define ECS700_COMMUNICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecs700_system_core.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * L1: Core Definitions — SCnet Communication Parameters
 * ============================================================================
 */

/** SCnet packet header size in bytes */
#define ECS700_SCNET_HEADER_SIZE         24

/** SCnet maximum payload (data) size in bytes */
#define ECS700_SCNET_MAX_PAYLOAD         1472

/** SCnet maximum packet size (header + payload) */
#define ECS700_SCNET_MAX_PACKET_SIZE     (ECS700_SCNET_HEADER_SIZE + ECS700_SCNET_MAX_PAYLOAD)

/** SCnet broadcast address */
#define ECS700_SCNET_BROADCAST_ADDR      0xFFFF

/** Maximum number of OPC UA items per subscription */
#define ECS700_OPCUA_MAX_ITEMS           1000

/** Maximum MODBUS TCP concurrent connections */
#define ECS700_MODBUS_MAX_CONNECTIONS    32

/** IEEE 1588 PTP sync interval (seconds) */
#define ECS700_PTP_SYNC_INTERVAL_S       1

/** Maximum time drift before re-sync (microseconds) */
#define ECS700_PTP_MAX_DRIFT_US          1000

/* ============================================================================
 * L1: Core Data Structures — SCnet Communication
 * ============================================================================
 */

/**
 * @brief SCnet packet type enumeration
 */
typedef enum {
    ECS700_PKT_REALTIME_DATA   = 0x01,  /**< Cyclic process data */
    ECS700_PKT_ALARM_EVENT     = 0x02,  /**< Spontaneous alarm/event */
    ECS700_PKT_CONFIG_DATA     = 0x03,  /**< Configuration download */
    ECS700_PKT_HEALTH_TELEGRAM = 0x04,  /**< Diagnostic telegram */
    ECS700_PKT_TIME_SYNC       = 0x05,  /**< IEEE 1588 PTP message */
    ECS700_PKT_HEARTBEAT       = 0x06,  /**< Node heartbeat */
    ECS700_PKT_FILE_TRANSFER   = 0x07,  /**< Bulk file transfer */
    ECS700_PKT_REMOTE_CMD      = 0x08   /**< Remote command/ack */
} ecs700_packet_type_t;

/**
 * @brief SCnet packet header
 *
 * Every SCnet packet begins with this 24-byte header.
 * The header enables routing, priority handling, and time-stamping.
 *
 * Knowledge Point: SCnet Frame Structure — the deterministic
 * communication protocol that enables real-time control over Ethernet.
 * Priority queuing ensures cyclic real-time data is never delayed by
 * lower-priority traffic.
 */
typedef struct {
    uint16_t  source_node_id;             /**< Source node ID */
    uint16_t  dest_node_id;               /**< Destination node ID (0xFFFF=broadcast) */
    uint8_t   packet_type;                /**< ecs700_packet_type_t */
    uint8_t   priority;                   /**< 0=highest, 7=lowest */
    uint16_t  sequence_number;            /**< Sequential packet number */
    uint16_t  payload_length;             /**< Data length in bytes */
    uint32_t  timestamp_sec;              /**< Seconds since epoch */
    uint32_t  timestamp_usec;             /**< Microsecond fraction */
    uint16_t  crc16;                      /**< CRC-16-CCITT checksum */
    uint8_t   version;                    /**< Protocol version */
    uint8_t   flags;                      /**< Control flags */
} ecs700_scnet_header_t;

/**
 * @brief Real-time data exchange block
 *
 * Periodic (cyclic) data exchange is the primary communication
 * mechanism in DCS. Control stations broadcast process data at
 * the configured scan rate, and all other nodes receive it.
 *
 * Knowledge Point: Producer-Consumer Model — the ECS-700 real-time
 * data exchange model. Controllers produce data; operator stations,
 * historians, and engineering stations consume it. This is the
 * foundation of OPC UA PubSub adopted in modern DCS.
 */
typedef struct {
    char      tag[ECS700_TAG_LEN_MAX];   /**< Process tag */
    double    value;                      /**< Current value */
    uint8_t   quality;                    /**< Data quality (OPC-style 0-255) */
    uint64_t  timestamp;                  /**< Value timestamp μs */
    uint16_t  source_node_id;             /**< Node that produced this value */
    bool      is_alarm;                   /**< Alarm active flag */
    uint8_t   alarm_level;                /**< Alarm priority 0-15 */
    uint16_t  update_rate_ms;            /**< Expected update rate */
} ecs700_rt_data_block_t;

/**
 * @brief OPC UA node representation
 *
 * ECS-700 supports OPC UA for interoperability with MES, ERP,
 * and third-party systems. The built-in OPC UA server exposes
 * all process points as OPC UA Variable Nodes.
 *
 * Knowledge Point: OPC UA Address Space — the information model
 * that bridges DCS real-time data to enterprise systems.
 * Based on IEC 62541 OPC Unified Architecture.
 */
typedef struct {
    uint32_t  node_id;                    /**< OPC UA NodeId numeric */
    char      browse_name[128];           /**< OPC UA BrowseName */
    char      display_name[64];           /**< OPC UA DisplayName */
    char      tag[ECS700_TAG_LEN_MAX];    /**< Mapped process tag */
    double    value;                      /**< Current value */
    uint8_t   quality;                    /**< OPC UA StatusCode quality */
    uint64_t  source_timestamp;           /**< Source timestamp */
    uint64_t  server_timestamp;           /**< Server timestamp */
    bool      is_array;                   /**< Whether array type */
    uint32_t  array_size;                 /**< Array size if is_array */
    double    eu_lo;                      /**< Engineering unit low */
    double    eu_hi;                      /**< Engineering unit high */
    char      eu_label[16];               /**< Engineering unit label */
} ecs700_opcua_node_t;

/**
 * @brief MODBUS TCP mapping record
 *
 * ECS-700 provides MODBUS TCP slave interface for legacy system
 * integration. Each register mapping maps a MODBUS address to
 * a DCS process point.
 */
typedef struct {
    uint16_t  modbus_address;             /**< MODBUS register address */
    char      dcs_tag[ECS700_TAG_LEN_MAX]; /**< Mapped DCS tag */
    uint8_t   function_code;              /**< MODBUS function code (3, 4, 6, 16) */
    uint8_t   data_type;                  /**< 0=coil, 1=discrete, 3=input, 4=holding */
    double    scale_factor;               /**< Scale MODBUS value to EU */
    double    offset;                     /**< Offset MODBUS value to EU */
    bool      enabled;                    /**< Mapping active */
    uint64_t  last_update_time;           /**< Last data update timestamp */
} ecs700_modbus_mapping_t;

/**
 * @brief SCnet node communication statistics
 */
typedef struct {
    uint16_t  node_id;                    /**< Node identifier */
    uint64_t  tx_packets;                 /**< Total packets transmitted */
    uint64_t  rx_packets;                 /**< Total packets received */
    uint64_t  tx_bytes;                   /**< Total bytes transmitted */
    uint64_t  rx_bytes;                   /**< Total bytes received */
    uint32_t  tx_errors;                  /**< Transmission errors */
    uint32_t  rx_errors;                  /**< Receive errors */
    uint32_t  crc_errors;                 /**< CRC mismatch errors */
    uint32_t  sequence_errors;            /**< Out-of-sequence packets */
    uint32_t  buffer_overruns;            /**< Receive buffer overruns */
    double    average_tx_rate_bps;        /**< Average transmit rate */
    double    average_rx_rate_bps;        /**< Average receive rate */
    double    peak_rx_rate_bps;           /**< Peak receive rate */
    uint64_t  last_activity_time;         /**< Last communication timestamp */
    bool      link_status;                /**< Link status */
} ecs700_comms_stats_t;

/* ============================================================================
 * L2: Core Concepts — SCnet Operations
 * ============================================================================
 */

/**
 * @brief Initialize SCnet header for a packet
 *
 * Fills in source/destination, packet type, priority, sequence,
 * and timestamp. CRC is computed by the driver layer.
 *
 * @param header      Header to initialize
 * @param src_node    Source node ID
 * @param dest_node   Destination node ID (0xFFFF for broadcast)
 * @param type        Packet type
 * @param priority    Priority (0-7, 0 highest)
 * @param seq_num     Sequence number
 * @param payload_len Payload length in bytes
 */
void ecs700_scnet_header_init(ecs700_scnet_header_t *header,
                               uint16_t src_node, uint16_t dest_node,
                               uint8_t type, uint8_t priority,
                               uint16_t seq_num, uint16_t payload_len);

/**
 * @brief Compute CRC-16-CCITT for SCnet packet verification
 *
 * Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
 * Initial value: 0xFFFF
 *
 * Knowledge Point: CRC-16-CCITT — the standard error detection
 * algorithm used in industrial protocols. Detects all single-bit
 * errors, all double-bit errors, all odd-numbered errors, and
 * burst errors up to 16 bits.
 *
 * @param data       Data buffer
 * @param length     Data length in bytes
 * @return CRC-16 checksum
 */
uint16_t ecs700_crc16_ccitt(const uint8_t *data, uint32_t length);

/**
 * @brief Verify SCnet packet CRC
 *
 * @param header  Packet header (CRC field must be filled)
 * @param payload Packet payload
 * @return true if CRC matches
 */
bool ecs700_scnet_verify_crc(const ecs700_scnet_header_t *header,
                              const uint8_t *payload);

/**
 * @brief Compute SCnet network utilization
 *
 * Utilization = (bytes_transmitted * 8) / (bandwidth_bps * period_s)
 *
 * SCnet design guideline: average utilization < 40% to maintain
 * deterministic real-time performance with headroom for bursts.
 *
 * @param bytes_tx_per_period Bytes during one scan period
 * @param scan_period_us      Scan period in microseconds
 * @param bandwidth_bps       Network bandwidth in bps
 * @return Utilization as percentage (0.0-100.0)
 */
double ecs700_scnet_utilization(uint32_t bytes_tx_per_period,
                                 uint32_t scan_period_us,
                                 uint64_t bandwidth_bps);

/**
 * @brief Initialize communication statistics for a node
 *
 * @param stats   Statistics structure
 * @param node_id Node identifier
 */
void ecs700_comms_stats_init(ecs700_comms_stats_t *stats, uint16_t node_id);

/**
 * @brief Record a transmitted packet in node statistics
 *
 * @param stats      Node statistics
 * @param bytes      Packet size in bytes
 * @param success    Whether transmission succeeded
 * @param current_time_us Current time
 */
void ecs700_comms_stats_record_tx(ecs700_comms_stats_t *stats,
                                   uint32_t bytes, bool success,
                                   uint64_t current_time_us);

/**
 * @brief Record a received packet in node statistics
 *
 * @param stats        Node statistics
 * @param bytes        Packet size in bytes
 * @param crc_error    Whether CRC check failed
 * @param seq_error    Whether sequence check failed
 * @param current_time_us Current time
 */
void ecs700_comms_stats_record_rx(ecs700_comms_stats_t *stats,
                                   uint32_t bytes, bool crc_error,
                                   bool seq_error, uint64_t current_time_us);

/* ============================================================================
 * L3: Engineering Structures — Data Exchange Models
 * ============================================================================
 */

/**
 * @brief Initialize real-time data block
 *
 * @param block       Data block to initialize
 * @param tag         Process tag
 * @param source_node Source node ID
 */
void ecs700_rt_data_init(ecs700_rt_data_block_t *block,
                          const char *tag, uint16_t source_node);

/**
 * @brief Update real-time data block with new value
 *
 * @param block    Data block
 * @param value    New value
 * @param quality  Data quality (0=bad, 128=good, 192=good)
 * @param time_us  Timestamp
 */
void ecs700_rt_data_update(ecs700_rt_data_block_t *block,
                            double value, uint8_t quality,
                            uint64_t time_us);

/* ============================================================================
 * L3: Engineering Structures — OPC UA Integration
 * ============================================================================
 */

/**
 * @brief Initialize OPC UA node mapping
 *
 * @param node        OPC UA node
 * @param node_id     OPC UA NodeId
 * @param tag         Mapped DCS tag
 * @param browse_name OPC UA BrowseName
 */
void ecs700_opcua_node_init(ecs700_opcua_node_t *node,
                             uint32_t node_id, const char *tag,
                             const char *browse_name);

/**
 * @brief Update OPC UA node value from DCS data
 *
 * @param node  OPC UA node
 * @param value New value
 * @param quality OPC UA quality code
 */
void ecs700_opcua_node_update(ecs700_opcua_node_t *node,
                               double value, uint8_t quality);

/* ============================================================================
 * L3: Engineering Structures — MODBUS TCP Gateway
 * ============================================================================
 */

/**
 * @brief Initialize MODBUS TCP mapping
 *
 * @param mapping        MODBUS mapping record
 * @param modbus_address MODBUS register address
 * @param dcs_tag        DCS process tag
 * @param function_code  MODBUS function code
 */
void ecs700_modbus_mapping_init(ecs700_modbus_mapping_t *mapping,
                                 uint16_t modbus_address,
                                 const char *dcs_tag,
                                 uint8_t function_code);

/**
 * @brief Convert DCS value to MODBUS register value
 *
 * MODBUS registers are 16-bit integers. This function scales
 * the DCS engineering-unit value to MODBUS raw value:
 *   modbus_val = (eu_val - offset) / scale_factor
 *
 * @param mapping  MODBUS mapping record
 * @param eu_value DCS value in engineering units
 * @return MODBUS register value (0-65535)
 */
uint16_t ecs700_dcs_to_modbus(const ecs700_modbus_mapping_t *mapping,
                               double eu_value);

/**
 * @brief Convert MODBUS register value to DCS engineering units
 *
 * @param mapping      MODBUS mapping record
 * @param modbus_value MODBUS register value (0-65535)
 * @return DCS value in engineering units
 */
double ecs700_modbus_to_dcs(const ecs700_modbus_mapping_t *mapping,
                             uint16_t modbus_value);

/* ============================================================================
 * L4: Engineering Laws — IEEE 1588 PTP Time Sync
 * ============================================================================
 */

/**
 * @brief PTP clock synchronization state
 */
typedef struct {
    bool      is_master;                  /**< This node is PTP master */
    uint64_t  master_clock_offset_ns;     /**< Offset from master clock */
    double    mean_path_delay_ns;         /**< Mean path delay */
    uint64_t  last_sync_time;             /**< Last sync timestamp */
    uint32_t  sync_interval_s;            /**< Sync interval */
    bool      synchronized;               /**< Clock is synchronized */
    uint32_t  sync_sequence;              /**< Sync message sequence */
    int64_t   clock_drift_rate_ppb;       /**< Clock drift rate (parts per billion) */
    uint64_t  local_clock_ns;             /**< Local clock value */
} ecs700_ptp_state_t;

/**
 * @brief Initialize PTP state
 *
 * @param ptp      PTP state structure
 * @param is_master Whether this node is PTP grandmaster
 */
void ecs700_ptp_init(ecs700_ptp_state_t *ptp, bool is_master);

/**
 * @brief Process PTP sync message from master
 *
 * Implements the IEEE 1588 PTP two-step clock synchronization:
 *   1. Master sends Sync message (t1)
 *   2. Slave receives Sync (t2)
 *   3. Master sends Follow_Up with t1
 *   4. Slave sends Delay_Req (t3)
 *   5. Master sends Delay_Resp with t4
 *
 * Offset = (t2 - t1) - mean_path_delay
 *
 * Knowledge Point: PTP Clock Sync — precise time synchronization
 * essential for SOE (Sequence of Events) recording and distributed
 * control coordination. ECS-700 target: < 1 μs accuracy.
 *
 * @param ptp        PTP state
 * @param t1_ns      Master send time
 * @param t2_ns      Slave receive time
 * @param current_time_ns Current local time
 */
void ecs700_ptp_process_sync(ecs700_ptp_state_t *ptp,
                              uint64_t t1_ns, uint64_t t2_ns,
                              uint64_t current_time_ns);

/**
 * @brief Apply clock correction to local time
 *
 * @param ptp  PTP state
 * @return Corrected local time in nanoseconds
 */
uint64_t ecs700_ptp_get_corrected_time(const ecs700_ptp_state_t *ptp);

/**
 * @brief Compute NTP-style time offset between two nodes
 *
 * Round-trip based clock offset estimation:
 *   t1 = client send time
 *   t2 = server receive time
 *   t3 = server send time
 *   t4 = client receive time
 *   offset = ((t2 - t1) + (t3 - t4)) / 2
 *   delay = (t4 - t1) - (t3 - t2)
 *
 * @param t1 Client send timestamp
 * @param t2 Server receive timestamp
 * @param t3 Server send timestamp
 * @param t4 Client receive timestamp
 * @param offset Output: clock offset
 * @param delay  Output: round-trip delay
 */
void ecs700_time_offset_ntp(uint64_t t1, uint64_t t2,
                             uint64_t t3, uint64_t t4,
                             int64_t *offset, int64_t *delay);

#ifdef __cplusplus
}
#endif

#endif /* ECS700_COMMUNICATION_H */
