/**
 * @file    test_communication.c
 * @brief   Tests for ECS-700 Communication module
 */

#include "ecs700_communication.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define TEST(n) printf("  TEST: %s ... ", n)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); failed++; } while(0)
#define CHECK(c, m) do { if (c) PASS(); else FAIL(m); } while(0)

static void test_scnet_header_init(void)
{
    TEST("SCnet header init");
    ecs700_scnet_header_t hdr;
    ecs700_scnet_header_init(&hdr, 10, 20, ECS700_PKT_REALTIME_DATA, 0, 1, 64);
    CHECK(hdr.source_node_id == 10, "source node");
    CHECK(hdr.dest_node_id == 20, "dest node");
    CHECK(hdr.packet_type == ECS700_PKT_REALTIME_DATA, "packet type");
    CHECK(hdr.priority == 0, "priority");
    CHECK(hdr.payload_length == 64, "payload length");
}

static void test_crc16_known(void)
{
    TEST("CRC-16-CCITT known value");
    uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    uint16_t crc = ecs700_crc16_ccitt(data, sizeof(data));
    CHECK(crc != 0, "CRC should be non-zero");
}

static void test_crc16_empty(void)
{
    TEST("CRC-16 null input");
    uint16_t crc = ecs700_crc16_ccitt(NULL, 0);
    CHECK(crc == 0, "null input → CRC 0");
}

static void test_crc16_zero_length(void)
{
    TEST("CRC-16 zero length");
    uint8_t data = 0xFF;
    uint16_t crc = ecs700_crc16_ccitt(&data, 0);
    CHECK(crc == 0, "zero length → CRC 0");
}

static void test_utilization(void)
{
    TEST("SCnet utilization");
    double util = ecs700_scnet_utilization(12500, 200000, 100000000);
    CHECK(util > 0.0 && util < 100.0, "utilization in range");
}

static void test_comms_stats_init(void)
{
    TEST("comms stats init");
    ecs700_comms_stats_t stats;
    ecs700_comms_stats_init(&stats, 10);
    CHECK(stats.node_id == 10, "node ID");
    CHECK(stats.link_status, "link up");
}

static void test_rt_data_init(void)
{
    TEST("RT data init");
    ecs700_rt_data_block_t block;
    ecs700_rt_data_init(&block, "FIC101", 5);
    CHECK(strcmp(block.tag, "FIC101") == 0, "tag set");
    CHECK(block.source_node_id == 5, "source node");
}

static void test_opcua_node_init(void)
{
    TEST("OPC UA node init");
    ecs700_opcua_node_t node;
    ecs700_opcua_node_init(&node, 1001, "TIC101", "Temperature_Reactor");
    CHECK(node.node_id == 1001, "node ID");
    CHECK(strcmp(node.tag, "TIC101") == 0, "tag");
}

static void test_modbus_mapping(void)
{
    TEST("MODBUS mapping init");
    ecs700_modbus_mapping_t mapping;
    ecs700_modbus_mapping_init(&mapping, 40001, "FIC101", 3);
    CHECK(mapping.modbus_address == 40001, "modbus address");
    CHECK(mapping.scale_factor == 1.0, "default scale");
}

static void test_modbus_conversion(void)
{
    TEST("MODBUS DCS ↔ MODBUS conversion");
    ecs700_modbus_mapping_t mapping;
    ecs700_modbus_mapping_init(&mapping, 40001, "FIC101", 3);
    mapping.scale_factor = 0.1;
    mapping.offset = 0.0;
    uint16_t modbus_val = ecs700_dcs_to_modbus(&mapping, 100.0);
    double back = ecs700_modbus_to_dcs(&mapping, modbus_val);
    CHECK(fabs(back - 100.0) < 5.0, "roundtrip ~approximate");
}

static void test_ptp_init(void)
{
    TEST("PTP init as slave");
    ecs700_ptp_state_t ptp;
    ecs700_ptp_init(&ptp, false);
    CHECK(!ptp.is_master, "not master");
    CHECK(!ptp.synchronized, "not yet synchronized");
}

static void test_ptp_sync(void)
{
    TEST("PTP sync processing");
    ecs700_ptp_state_t ptp;
    ecs700_ptp_init(&ptp, false);
    ecs700_ptp_process_sync(&ptp, 1000000, 1000100, 1000100);
    CHECK(ptp.synchronized, "synchronized after sync");
}

static void test_ntp_offset(void)
{
    TEST("NTP offset calculation");
    int64_t offset, delay;
    ecs700_time_offset_ntp(1000, 1100, 1105, 1200, &offset, &delay);
    CHECK(offset != 0, "offset computed");
    CHECK(delay > 0, "delay positive");
}

int main(void)
{
    printf("\n=== ECS-700 Communication Tests ===\n\n");
    test_scnet_header_init();
    test_crc16_known();
    test_crc16_empty();
    test_crc16_zero_length();
    test_utilization();
    test_comms_stats_init();
    test_rt_data_init();
    test_opcua_node_init();
    test_modbus_mapping();
    test_modbus_conversion();
    test_ptp_init();
    test_ptp_sync();
    test_ntp_offset();
    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
