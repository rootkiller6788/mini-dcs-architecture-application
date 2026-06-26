/**
 * @file test_architecture.c
 * @brief Tests for DCS system architecture (L2-L6).
 */
#include "dcs_architecture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s ... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

int main(void)
{
    printf("\n=== DCS Architecture Tests ===\n\n");

    /* L2: ISA-95 Level Mapping */
    TEST("ISA-95 level mapping - controller → Level 1");
    CHECK(dcs_map_node_to_isa95_level(DCS_NODE_CONTROLLER) == DCS_LEVEL_1_CONTROL,
          "controller should map to level 1");

    TEST("ISA-95 level mapping - HMI → Level 2");
    CHECK(dcs_map_node_to_isa95_level(DCS_NODE_OPERATOR_STATION) == DCS_LEVEL_2_SUPERVISORY,
          "operator station should map to level 2");

    TEST("ISA-95 level mapping - I/O → Level 0");
    CHECK(dcs_map_node_to_isa95_level(DCS_NODE_IO_SUBSYSTEM) == DCS_LEVEL_0_FIELD,
          "I/O subsystem should map to level 0");

    TEST("ISA-95 level mapping - historian → Level 3");
    CHECK(dcs_map_node_to_isa95_level(DCS_NODE_HISTORIAN) == DCS_LEVEL_3_PLANT_MES,
          "historian should map to level 3");

    /* L2: Architecture Verification */
    TEST("Architecture verification - null input");
    CHECK(dcs_verify_architecture(NULL, NULL) == 0,
          "should return 0 for NULL input");

    dcs_system_config_t config;
    memset(&config, 0, sizeof(config));
    snprintf(config.system_name, 64, "TestDCS");

    /* Minimal valid config */
    config.num_controller_nodes = 3;
    config.num_operator_stations = 2;
    config.num_engineering_stations = 1;
    config.num_io_subsystems = 4;
    config.total_ai_points = 200;
    config.total_ao_points = 100;
    config.total_di_points = 300;
    config.total_do_points = 200;
    config.controller_redundancy = 1;
    config.network_redundancy = 1;
    config.backbone_topology = DCS_TOPOLOGY_RING;
    config.backbone_speed_mbps = 1000.0;
    config.controller_scan_ms = 250.0;

    dcs_arch_verification_t result;
    int ver_result = dcs_verify_architecture(&config, &result);

    TEST("Architecture verification - valid config passes");
    CHECK(ver_result == 1, "valid config should pass verification");

    /* Test with no redundancy (should detect violations) */
    config.controller_redundancy = 0;
    config.network_redundancy = 0;
    config.backbone_topology = DCS_TOPOLOGY_BUS;
    ver_result = dcs_verify_architecture(&config, &result);
    TEST("Architecture verification - detects missing redundancy");
    CHECK(ver_result == 0, "should detect redundancy violations");

    /* Reset for further tests */
    config.controller_redundancy = 0;
    config.backbone_topology = DCS_TOPOLOGY_RING;

    /* L3: Controller Count Calculation */
    TEST("Controller count - 1000 I/O, 250ms scan");
    uint32_t ctrl_count = dcs_calculate_controller_count(1000, 250.0);
    CHECK(ctrl_count >= 2, "need at least 2 controllers for 1000 I/O");

    TEST("Controller count - zero I/O");
    CHECK(dcs_calculate_controller_count(0, 250.0) == 0,
          "zero I/O should need zero controllers");

    TEST("Controller count - fast scan limits");
    uint32_t ctrl_fast = dcs_calculate_controller_count(1000, 50.0);
    CHECK(ctrl_fast > dcs_calculate_controller_count(500, 500.0),
          "fast scan should require more controllers");

    /* L3: Bandwidth Requirement */
    config.controller_redundancy = 1;
    config.num_operator_stations = 3;
    config.total_ai_points = 500;
    config.total_ao_points = 200;
    config.total_di_points = 500;
    config.total_do_points = 300;
    config.controller_scan_ms = 250.0;

    double bw = dcs_calculate_bandwidth_requirement(&config);
    TEST("Bandwidth requirement - positive value");
    CHECK(bw > 0.0, "bandwidth should be positive");
    TEST("Bandwidth requirement - reasonable range (< 100 Mbps for 1500 I/O)");
    CHECK(bw < 100.0, "bandwidth should be < 100 Mbps for this config");

    /* L3: Network Load */
    TEST("Network load calculation");
    double load = dcs_calculate_network_load(&config, bw);
    CHECK(load >= 0.0 && load <= 100.0,
          "network load should be 0-100%");

    /* L3: Topology Verification */
    TEST("Topology - ring supports redundancy");
    CHECK(dcs_verify_topology_redundancy(DCS_TOPOLOGY_RING, 1) == 1,
          "ring should support redundancy");

    TEST("Topology - bus does not support redundancy");
    CHECK(dcs_verify_topology_redundancy(DCS_TOPOLOGY_BUS, 1) == 0,
          "bus should not support redundancy");

    TEST("Topology - mesh supports redundancy");
    CHECK(dcs_verify_topology_redundancy(DCS_TOPOLOGY_MESH, 1) == 1,
          "mesh should support redundancy");

    /* L3: Network Diameter */
    TEST("Network diameter - bus topology");
    CHECK(dcs_network_diameter(DCS_TOPOLOGY_BUS, 5) == 4,
          "bus of 5 nodes should have diameter 4");

    TEST("Network diameter - star topology");
    CHECK(dcs_network_diameter(DCS_TOPOLOGY_STAR, 10) == 2,
          "star topology should have diameter 2");

    TEST("Network diameter - mesh topology");
    CHECK(dcs_network_diameter(DCS_TOPOLOGY_MESH, 100) == 1,
          "mesh topology should have diameter 1");

    /* L3: Worst-Case Latency */
    TEST("Worst-case latency - star topology, 100 Mbps");
    double lat = dcs_worst_case_latency(DCS_TOPOLOGY_STAR, 10, 100.0, 64);
    CHECK(lat > 0.0, "latency should be positive");

    /* L6: System Availability */
    config.controller_redundancy = 1;
    config.network_redundancy = 1;
    config.server_redundancy = 1;
    config.num_operator_stations = 3;
    double avail = dcs_estimate_availability(&config);
    TEST("System availability - with full redundancy");
    CHECK(avail > 0.99, "availability with full redundancy should be > 99%");
    TEST("System availability - within [0,1]");
    CHECK(avail >= 0.0 && avail <= 1.0, "availability must be in [0,1]");

    /* L3: Controller Loading */
    double loading = dcs_analyze_controller_loading(50, 100, 50, 200, 100, 250.0);
    TEST("Controller loading - typical config");
    CHECK(loading > 0.0 && loading < 100.0, "loading should be in range (0-100)");

    /* L6: System Sizing Recommendation */
    config.num_controller_nodes = 15;
    uint32_t rec_ctrl, rec_ops;
    dcs_network_topology_t rec_topo;
    int sizing_ok = dcs_recommend_system_sizing(&config, &rec_ctrl, &rec_ops, &rec_topo);
    TEST("System sizing - returns valid recommendation");
    CHECK(sizing_ok == 1 && rec_ctrl > 0 && rec_ops > 0,
          "should return valid sizing recommendations");

    /* L2: ISA-95 level presence check */
    TEST("ISA-95 level present - Level 1 always present with controllers");
    CHECK(dcs_check_isa95_level_present(&config, DCS_LEVEL_1_CONTROL) == 1,
          "Level 1 should be present");

    TEST("ISA-95 level present - NULL config");
    CHECK(dcs_check_isa95_level_present(NULL, DCS_LEVEL_1_CONTROL) == 0,
          "NULL config should return 0");

    /* Summary */
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
