/**
 * @file test_delta_v.c
 * @brief Comprehensive test suite for mini-emerson-deltav-system
 *
 * Covers all core APIs with assertion-based tests.
 * Each test validates one independent knowledge point.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../include/delta_v_system.h"
#include "../include/delta_v_controller.h"
#include "../include/delta_v_control.h"
#include "../include/delta_v_communication.h"
#include "../include/delta_v_redundancy.h"
#include "../include/delta_v_batch.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %-50s ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond) do { if (!(cond)) { FAIL(#cond); return; } } while(0)

/* ---- L1/L2: System tests ---- */
static void test_system_init(void)
{
    TEST("system_init_defaults");
    delta_v_system_config_t config;
    delta_v_system_init(&config);
    CHECK(strcmp(config.system_name, "DELTAV_SYSTEM") == 0);
    CHECK(config.major_version == 14);
    CHECK(config.system_mode == DELTAV_MODE_SETUP);
    CHECK(config.redundancy_enabled == true);
    CHECK(config.area_count == 0);
    PASS();
}

static void test_system_add_area(void)
{
    TEST("system_add_area_valid");
    delta_v_system_config_t config;
    delta_v_system_init(&config);
    delta_v_area_t area;
    memset(&area, 0, sizeof(area));
    area.area_id = 1;
    strcpy(area.area_name, "Reaction_Area");
    CHECK(delta_v_system_add_area(&config, &area) == true);
    CHECK(config.area_count == 1);
    PASS();
}

static void test_system_validate(void)
{
    TEST("system_validate_ok");
    delta_v_system_config_t config;
    delta_v_system_init(&config);
    CHECK(delta_v_system_validate(&config) == DELTAV_DB_OK);
    PASS();
}

static void test_status_transitions(void)
{
    TEST("status_transition_off_to_booting");
    CHECK(delta_v_is_valid_status_transition(DELTAV_STAT_OFF, DELTAV_STAT_BOOTING) == true);
    CHECK(delta_v_is_valid_status_transition(DELTAV_STAT_OFF, DELTAV_STAT_ACTIVE) == false);
    PASS();
}

static void test_ip_addressing(void)
{
    TEST("ip_addressing_scheme");
    delta_v_network_topology_t net;
    memset(&net, 0, sizeof(net));
    net.subnet_address = 0x0A040000;
    net.redundancy_enabled = true;
    uint32_t ip = delta_v_calc_primary_ip(&net, 1);
    CHECK(ip == 0x0A040001);
    PASS();
}

/* ---- L2/L5: Controller tests ---- */
static void test_controller_init(void)
{
    TEST("controller_init_defaults");
    delta_v_controller_config_t ctrl;
    delta_v_controller_config_init(&ctrl, DELTAV_CTRL_MD_PLUS, 11);
    CHECK(ctrl.controller_id == 11);
    CHECK(ctrl.hw_type == DELTAV_CTRL_MD_PLUS);
    CHECK(ctrl.mode == DELTAV_CTRL_MODE_PROGRAM);
    PASS();
}

static void test_signal_conversion(void)
{
    TEST("signal_conversion_4_20mA_to_EU");
    delta_v_charm_io_channel_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.raw_min = 4.0; ch.raw_max = 20.0;
    ch.eu_min = 0.0; ch.eu_max = 100.0;
    double eu = delta_v_signal_convert_raw_to_eu(12.0, &ch);
    CHECK(fabs(eu - 50.0) < 0.01);
    double raw = delta_v_signal_convert_eu_to_raw(50.0, &ch);
    CHECK(fabs(raw - 12.0) < 0.01);
    PASS();
}

/* ---- L2/L5: PID Control tests ---- */
static void test_pid_init(void)
{
    TEST("pid_block_init");
    delta_v_pid_block_t pid;
    delta_v_pid_block_init(&pid, DELTAV_PID_STANDARD);
    CHECK(pid.gain == 1.0);
    CHECK(pid.mode == DELTAV_PID_MAN);
    CHECK(pid.out_high_limit == 100.0);
    PASS();
}

static void test_pid_bumpless(void)
{
    TEST("pid_bumpless_transfer");
    delta_v_pid_block_t pid;
    delta_v_pid_block_init(&pid, DELTAV_PID_STANDARD);
    pid.sp = 50.0; pid.pv = 50.0; pid.out = 50.0;
    delta_v_pid_block_bumpless_transfer(&pid);
    CHECK(pid.bumpless_enabled == true);
    PASS();
}

static void test_pid_calculate(void)
{
    TEST("pid_calculate_basic");
    delta_v_pid_block_t pid;
    delta_v_pid_block_init(&pid, DELTAV_PID_STANDARD);
    pid.mode = DELTAV_PID_AUT;
    pid.gain = 2.0; pid.reset = 60.0; pid.rate = 0.0;
    pid.sp = 50.0; pid.pv = 40.0;
    delta_v_pid_block_calculate(&pid, 0.1);
    CHECK(pid.out > 0.0);
    PASS();
}

/* ---- L2/L3: Communication tests ---- */
static void test_modbus_crc16(void)
{
    TEST("modbus_crc16_calculation");
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint16_t crc = delta_v_modbus_crc16_calculate(data, 6);
    CHECK(crc != 0);
    PASS();
}

static void test_acn_packet_init(void)
{
    TEST("acn_packet_initialization");
    delta_v_acn_packet_t pkt;
    delta_v_acn_packet_init(&pkt, DELTAV_ACN_HEARTBEAT);
    CHECK(pkt.msg_type == DELTAV_ACN_HEARTBEAT);
    CHECK(pkt.delta_v_version == 14);
    PASS();
}

/* ---- L2: Redundancy tests ---- */
static void test_redundancy_pair_init(void)
{
    TEST("redundancy_pair_initialization");
    delta_v_redundancy_pair_t pair;
    delta_v_redundancy_pair_init(&pair, DELTAV_REDUN_PAIR_CONTROLLER, 1);
    CHECK(pair.pair_id == 1);
    CHECK(pair.primary.role == DELTAV_REDUN_ROLE_PRIMARY);
    CHECK(pair.standby.role == DELTAV_REDUN_ROLE_STANDBY);
    CHECK(pair.pair_healthy == true);
    PASS();
}

static void test_availability_calculation(void)
{
    TEST("availability_calculation");
    delta_v_redundancy_pair_t pair;
    delta_v_redundancy_pair_init(&pair, DELTAV_REDUN_PAIR_CONTROLLER, 1);
    double avail = delta_v_redundancy_calculate_availability(&pair);
    CHECK(avail > 0.99);
    CHECK(avail < 1.0);
    PASS();
}

/* ---- L2: Batch tests ---- */
static void test_recipe_init(void)
{
    TEST("recipe_initialization");
    delta_v_recipe_t recipe;
    delta_v_recipe_init(&recipe, "Polymerization", 500.0);
    CHECK(strcmp(recipe.recipe_name, "Polymerization") == 0);
    CHECK(recipe.master_batch_size == 500.0);
    CHECK(recipe.scalable == true);
    PASS();
}

static void test_batch_state_transitions(void)
{
    TEST("batch_state_transition_valid");
    CHECK(delta_v_batch_is_transition_valid(DELTAV_BATCH_IDLE, DELTAV_BATCH_CMD_START) == true);
    CHECK(delta_v_batch_is_transition_valid(DELTAV_BATCH_IDLE, DELTAV_BATCH_CMD_HOLD) == false);
    CHECK(delta_v_batch_is_transition_valid(DELTAV_BATCH_RUNNING, DELTAV_BATCH_CMD_HOLD) == true);
    PASS();
}

/* ---- L5: MPC tests ---- */
static void test_mpc_init(void)
{
    TEST("mpc_initialization");
    delta_v_mpc_config_t mpc;
    delta_v_mpc_init(&mpc, 2, 2);
    CHECK(mpc.mv_count == 2);
    CHECK(mpc.cv_count == 2);
    CHECK(mpc.prediction_horizon == 60);
    PASS();
}

/* ---- L5: Neural Network tests ---- */
static void test_neural_init(void)
{
    TEST("neural_network_initialization");
    delta_v_neural_config_t nn;
    delta_v_neural_init(&nn, 4, 8, 2);
    CHECK(nn.input_count == 4);
    CHECK(nn.hidden_count == 8);
    CHECK(nn.output_count == 2);
    CHECK(nn.training_rate == 0.01);
    PASS();
}

static void test_split_range(void)
{
    TEST("split_range_sequential");
    delta_v_split_range_t sr;
    delta_v_split_range_init(&sr, DELTAV_SPLIT_SEQUENTIAL, 50.0);
    delta_v_split_range_calculate(&sr, 30.0);
    CHECK(sr.valve_a_output > 0.0);
    CHECK(fabs(sr.valve_b_output) < 0.01);
    PASS();
}

static void test_ratio_control(void)
{
    TEST("ratio_control_calculation");
    delta_v_ratio_block_t rb;
    delta_v_ratio_block_init(&rb, 2.0, 10.0);
    rb.wildcard_input = 25.0;
    delta_v_ratio_block_calculate(&rb);
    CHECK(fabs(rb.output - 60.0) < 0.01);
    PASS();
}

static void test_2oo3_voting(void)
{
    TEST("2oo3_voting_three_valid");
    delta_v_2oo3_voting_t v;
    delta_v_2oo3_init(&v, 5.0);
    double result = delta_v_2oo3_vote(&v, 70.0, 72.0, 71.0, true, true, true);
    CHECK(v.voted_output_valid == true);
    CHECK(fabs(result - 71.0) < 1.0);
    PASS();
}

static void test_lead_lag(void)
{
    TEST("lead_lag_steady_state");
    delta_v_lead_lag_block_t ll;
    delta_v_lead_lag_init(&ll, 0.0, 0.0, 1.0);
    ll.gain = 1.0;
    for (int i = 0; i < 100; i++)
        delta_v_lead_lag_calculate(&ll, 50.0, 0.1);
    CHECK(fabs(ll.output - 50.0) < 1.0);
    PASS();
}

static void test_characterizer(void)
{
    TEST("signal_characterizer_linear");
    delta_v_signal_characterizer_t sc;
    delta_v_signal_characterizer_init(&sc);
    delta_v_characterizer_add_point(&sc, 0.0, 0.0);
    delta_v_characterizer_add_point(&sc, 100.0, 100.0);
    double y = delta_v_characterizer_interpolate(&sc, 50.0);
    CHECK(fabs(y - 50.0) < 0.01);
    PASS();
}

int main(void)
{
    printf("=== DeltaV System Test Suite ===\n\n");
    
    test_system_init();
    test_system_add_area();
    test_system_validate();
    test_status_transitions();
    test_ip_addressing();
    test_controller_init();
    test_signal_conversion();
    test_pid_init();
    test_pid_bumpless();
    test_pid_calculate();
    test_modbus_crc16();
    test_acn_packet_init();
    test_redundancy_pair_init();
    test_availability_calculation();
    test_recipe_init();
    test_batch_state_transitions();
    test_mpc_init();
    test_neural_init();
    test_split_range();
    test_ratio_control();
    test_2oo3_voting();
    test_lead_lag();
    test_characterizer();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
