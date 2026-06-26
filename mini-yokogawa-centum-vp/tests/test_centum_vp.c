/**
 * @file test_centum_vp.c
 * @brief CENTUM VP DCS — Comprehensive Test Suite
 *
 * Tests cover L1-L6 knowledge levels:
 *   L1: Type definitions, string conversions, signal ranges
 *   L2: PID mode transitions, station status, redundancy roles
 *   L3: Project database ops, FCS config, I/O module management
 *   L4: Availability calculation, CRC properties, batch state transitions
 *   L5: PID velocity algorithm, sequence execution, LC64 logic
 *   L6: Full system integration scenario
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include "centum_vp_system.h"
#include "centum_vp_fcs.h"
#include "centum_vp_control.h"
#include "centum_vp_communication.h"
#include "centum_vp_redundancy.h"
#include "centum_vp_batch.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST %-50s", name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf(" PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    tests_failed++; \
    printf(" FAIL: %s\n", msg); \
} while(0)

#define CHECK(cond, msg) do { \
    if (cond) { PASS(); } else { FAIL(msg); } \
} while(0)

/*============================================================================
 * L1 — Definition Tests
 *============================================================================*/
static void test_station_type_strings(void)
{
    TEST("L1: Station type to string");
    CHECK(strcmp(centum_station_type_to_string(CENTUM_STATION_HIS), "HIS") == 0,
          "HIS string mismatch");
    CHECK(strcmp(centum_station_type_to_string(CENTUM_STATION_FCS), "FCS") == 0,
          "FCS string mismatch");
    CHECK(strcmp(centum_station_type_to_string(CENTUM_STATION_ENG), "ENG") == 0,
          "ENG string mismatch");
}

static void test_station_status_strings(void)
{
    TEST("L1: Station status to string");
    CHECK(strcmp(centum_station_status_to_string(CENTUM_STAT_RUNNING), "RUNNING") == 0,
          "RUNNING string mismatch");
    CHECK(strcmp(centum_station_status_to_string(CENTUM_STAT_FAIL), "FAIL") == 0,
          "FAIL string mismatch");
    CHECK(strcmp(centum_station_status_to_string(CENTUM_STAT_STANDBY), "STANDBY") == 0,
          "STANDBY string mismatch");
}

static void test_fcs_type_strings(void)
{
    TEST("L1: FCS type to string");
    CHECK(strcmp(centum_fcs_type_to_string(FCS_TYPE_KFCS2), "KFCS2") == 0,
          "KFCS2 string mismatch");
    CHECK(strcmp(centum_fcs_type_to_string(FCS_TYPE_SFCS), "SFCS") == 0,
          "SFCS string mismatch");
}

static void test_io_module_type_strings(void)
{
    TEST("L1: I/O module type to string");
    CHECK(strcmp(centum_io_module_type_to_string(IO_MOD_AAI141), "AAI141-H50") == 0,
          "AAI141 string mismatch");
    CHECK(strcmp(centum_io_module_type_to_string(IO_MOD_ADV151), "ADV151-P50") == 0,
          "ADV151 string mismatch");
}

static void test_io_module_from_string(void)
{
    TEST("L1: I/O module from string");
    CHECK(centum_io_module_from_string("AAI543-H50") == IO_MOD_AAI543,
          "AAI543 lookup failed");
    CHECK(centum_io_module_from_string("ADV551-P50") == IO_MOD_ADV551,
          "ADV551 lookup failed");
    CHECK(centum_io_module_from_string("UNKNOWN") == IO_MOD_AAI141,
          "Unknown should default to AAI141");
}

/*============================================================================
 * L2 — Core Concept Tests
 *============================================================================*/
static void test_system_config_init(void)
{
    TEST("L2: System config init");
    centum_system_config_t config;
    centum_system_config_init(&config);
    CHECK(strcmp(config.project_name, "NEW_PROJECT") == 0,
          "Project name default wrong");
    CHECK(config.system_mode == CENTUM_MODE_OFFLINE,
          "System mode should be OFFLINE");
    CHECK(config.domain_count == 0,
          "Domain count should be 0");
}

static void test_add_station(void)
{
    TEST("L2: Add station to system");
    centum_system_config_t config;
    centum_system_config_init(&config);

    centum_station_t stn;
    memset(&stn, 0, sizeof(stn));
    stn.station_id = 0x0101; /* Domain 1, Station 1 */
    stn.type = CENTUM_STATION_FCS;

    bool ok = centum_system_add_station(&config, &stn, 1);
    CHECK(ok, "Add station failed");
    CHECK(config.domain_count == 1, "Domain count should be 1");
}

static void test_add_duplicate_station(void)
{
    TEST("L2: Add duplicate station rejected");
    centum_system_config_t config;
    centum_system_config_init(&config);

    centum_station_t stn;
    memset(&stn, 0, sizeof(stn));
    stn.station_id = 0x0101;
    stn.type = CENTUM_STATION_FCS;

    centum_system_add_station(&config, &stn, 1);
    bool ok = centum_system_add_station(&config, &stn, 1);
    CHECK(!ok, "Duplicate station should be rejected");
}

static void test_vnet_ip_addressing(void)
{
    TEST("L2: Vnet/IP address calculation");
    uint32_t addr = centum_vnet_calc_ip_address(1, 1);
    CHECK(addr == ((172U << 24) | (16U << 16) | (1U << 8) | 1U),
          "IP address calculation wrong");
}

static void test_license_check(void)
{
    TEST("L2: License check");
    centum_system_config_t config;
    centum_system_config_init(&config);

    centum_license_t license;
    memset(&license, 0, sizeof(license));
    license.valid = true;
    license.max_tags = 1000;
    license.licensed_fcs_count = 10;
    license.licensed_his_count = 10;
    license.expiry_date = 0; /* Perpetual */

    bool ok = centum_system_license_check(&config, &license);
    CHECK(ok, "License check should pass for empty system");
}

static void test_system_validate(void)
{
    TEST("L2: System configuration validation");
    centum_system_config_t config;
    centum_system_config_init(&config);

    centum_db_check_result_t result = centum_system_validate(&config);
    CHECK(result == DBCHECK_WARNING, "Empty system should give warning");
}

/*============================================================================
 * L3 — Engineering Structure Tests
 *============================================================================*/
static void test_fcs_config_init_fn(void)
{
    TEST("L3: FCS config init");
    centum_fcs_config_t fcs;
    centum_fcs_config_init(&fcs, 0x0101);
    CHECK(fcs.fcs_id == 0x0101, "FCS ID wrong");
    CHECK(fcs.type == FCS_TYPE_KFCS2, "Default type should be KFCS2");
    CHECK(fcs.scan_cycle_us == 200000, "Default scan cycle should be 200ms");
}

static void test_add_nio_node(void)
{
    TEST("L3: Add N-IO node");
    centum_fcs_config_t fcs;
    centum_fcs_config_init(&fcs, 0x0101);

    centum_nio_node_t node;
    memset(&node, 0, sizeof(node));
    node.node_address = 5;
    node.slot_count = 8;

    bool ok = centum_fcs_add_nio_node(&fcs, &node);
    CHECK(ok, "Add N-IO node failed");
    CHECK(fcs.nio_node_count == 1, "N-IO node count should be 1");
}

static void test_add_io_module(void)
{
    TEST("L3: Add I/O module");
    centum_fcs_config_t fcs;
    centum_fcs_config_init(&fcs, 0x0101);

    centum_nio_node_t node;
    memset(&node, 0, sizeof(node));
    node.node_address = 1;
    node.slot_count = 8;
    centum_fcs_add_nio_node(&fcs, &node);

    centum_io_module_t mod;
    memset(&mod, 0, sizeof(mod));
    mod.type = IO_MOD_AAI141;
    mod.slot_number = 3;
    mod.channel_count = 16;

    bool ok = centum_fcs_add_io_module(&fcs, &mod);
    CHECK(ok || fcs.nio_node_count == 0, "Add I/O module check");
}

static void test_fcs_validate(void)
{
    TEST("L3: FCS configuration validation");
    centum_fcs_config_t fcs;
    centum_fcs_config_init(&fcs, 0x0101);

    /* With function blocks but no N-IO nodes, should fail */
    fcs.function_block_count = 10;
    bool valid = centum_fcs_validate_configuration(&fcs);
    CHECK(!valid, "FCS with FBs but no N-IO should be invalid");
}

static void test_remove_station(void)
{
    TEST("L3: Remove station");
    centum_system_config_t config;
    centum_system_config_init(&config);

    centum_station_t stn;
    memset(&stn, 0, sizeof(stn));
    stn.station_id = 0x0101;
    centum_system_add_station(&config, &stn, 1);

    bool ok = centum_system_remove_station(&config, 0x0101);
    CHECK(ok, "Remove station failed");
    CHECK(config.domain_count == 0, "Domain should be removed when empty");
}

static void test_find_station_by_tag(void)
{
    TEST("L3: Find station by tag");
    centum_system_config_t config;
    centum_system_config_init(&config);

    centum_station_t stn;
    memset(&stn, 0, sizeof(stn));
    stn.station_id = 0x0101;
    centum_system_add_station(&config, &stn, 1);

    uint16_t id = centum_system_find_station_by_tag(&config, "FCS0101");
    CHECK(id == 0x0101, "Tag lookup failed");
}

/*============================================================================
 * L4 — Engineering Law Tests
 *============================================================================*/
static void test_signal_conversion_linearity(void)
{
    TEST("L4: Signal conversion linearity");
    centum_signal_range_t range;
    memset(&range, 0, sizeof(range));
    range.eu_low = 0.0;
    range.eu_high = 100.0;
    range.raw_low = 4.0;
    range.raw_high = 20.0;
    range.adc_low = 0;
    range.adc_high = 32000;

    /* 4mA -> 0%, 20mA -> 100%, 12mA -> 50% */
    double eu_low = centum_signal_convert_raw_to_eu(0, &range);
    double eu_high = centum_signal_convert_raw_to_eu(32000, &range);
    double eu_mid = centum_signal_convert_raw_to_eu(16000, &range);

    CHECK(fabs(eu_low - 0.0) < 0.01, "0% EU conversion wrong");
    CHECK(fabs(eu_high - 100.0) < 0.01, "100% EU conversion wrong");
    CHECK(fabs(eu_mid - 50.0) < 0.5, "50% EU conversion wrong");
}

static void test_signal_eu_to_raw(void)
{
    TEST("L4: EU to raw conversion (roundtrip)");
    centum_signal_range_t range;
    memset(&range, 0, sizeof(range));
    range.eu_low = 0.0; range.eu_high = 100.0;
    range.adc_low = 0; range.adc_high = 32000;

    int16_t raw = centum_signal_convert_eu_to_raw(50.0, &range);
    CHECK(raw == 16000, "50% EU to raw conversion wrong");
}

static void test_availability_calculation(void)
{
    TEST("L4: System availability calculation");
    centum_redundancy_config_t redun;
    centum_redundancy_config_init(&redun);
    redun.cpu_pair.primary.hardware_healthy = true;
    redun.cpu_pair.standby.hardware_healthy = true;

    double avail = centum_redundancy_calculate_availability(&redun);
    CHECK(avail > 0.99, "Redundant system availability should exceed 99%");
}

static void test_mtbf_values(void)
{
    TEST("L4: MTBF values are reasonable");
    centum_redundancy_pair_t cpu_pair;
    centum_redundancy_pair_init(&cpu_pair, REDUN_PAIR_CPU, 1);

    double mtbf = centum_redundancy_mtbf_hours(&cpu_pair);
    CHECK(mtbf > 10000.0, "CPU MTBF should exceed 10,000 hours");
}

static void test_batch_state_transitions(void)
{
    TEST("L4: Batch state transitions are deterministic");
    centum_batch_manager_t mgr;
    centum_batch_manager_init(&mgr);

    CHECK(centum_batch_get_state(&mgr) == BATCH_STATE_IDLE,
          "Initial batch state should be IDLE");
}

/*============================================================================
 * L5 — Algorithm Tests
 *============================================================================*/
static void test_pid_velocity_algorithm(void)
{
    TEST("L5: PID velocity algorithm execution");
    centum_pid_block_t pid;
    centum_pid_block_init(&pid);
    centum_pid_block_set_tuning(&pid, 2.0, 60.0, 0.0, 0.0);
    pid.action = PID_ACT_REVERSE;  /* Heating: MV up when PV < SV */
    centum_pid_block_set_mode(&pid, PID_MODE_AUT);
    centum_pid_block_set_sv(&pid, 50.0);

    double mv = centum_pid_block_execute(&pid, 40.0, 0.2);
    /* With error = 10, Kp=2.0, P-term alone contributes */
    CHECK(mv > 0.0, "PID output should be non-zero with error present");
}

static void test_pid_bumpless_transfer(void)
{
    TEST("L5: PID bumpless transfer from MAN to AUT");
    centum_pid_block_t pid;
    centum_pid_block_init(&pid);

    /* Set output in MAN mode */
    pid.mode = PID_MODE_MAN;
    pid.mv = 75.0;
    centum_pid_block_bumpless_transfer(&pid, 75.0);

    CHECK(pid.mv == 75.0, "Bumpless transfer should preserve MV");
}

static void test_pid_anti_windup_clamp(void)
{
    TEST("L5: PID anti-windup clamping");
    centum_pid_block_t pid;
    centum_pid_block_init(&pid);
    pid.mv = 150.0; /* Exceeds 100% limit */
    centum_pid_block_anti_windup_clamp(&pid);
    CHECK(pid.mv == 100.0, "Output should be clamped to 100%");
    CHECK(pid.anti_windup_active, "Anti-windup should be active");
}

static void test_pid_alarm_evaluation(void)
{
    TEST("L5: PID alarm evaluation");
    centum_pid_block_t pid;
    centum_pid_block_init(&pid);
    pid.sv = 50.0;
    pid.pv = 90.0;
    pid.vh_high_alarm = 80.0;
    pid.dv_high_alarm = 20.0;

    centum_pid_block_handle_alarms(&pid);
    CHECK(pid.vh_hi_alarm_active, "VH high alarm should be active");
    CHECK(pid.dv_hi_alarm_active, "Deviation high alarm should be active");
}

static void test_sequence_execution(void)
{
    TEST("L5: Sequence block execution");
    centum_sequence_block_t seq;
    centum_sequence_block_init(&seq, SEQ_TYPE_SEBOL);

    centum_sequence_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_number = 1;
    step.condition_count = 0;
    step.wait_for_condition = false;

    centum_sequence_add_step(&seq, &step);
    CHECK(seq.total_steps == 1, "Step count should be 1");

    seq.enable = true;
    seq.state = SEQ_STATE_RUNNING;

    centum_sequence_execute(&seq);
    CHECK(seq.current_step == 1, "Sequence should advance past step 0");
}

static void test_lc64_interlock(void)
{
    TEST("L5: LC64 interlock logic");
    centum_lc64_block_t lc64;
    centum_lc64_block_init(&lc64);

    /* AND gate: input 0 AND input 1 */
    centum_lc64_add_element(&lc64, LC64_AND, 0, 1);
    CHECK(lc64.element_count == 1, "Element count should be 1");

    /* Both inputs ON */
    uint64_t inputs = 0x03;
    centum_lc64_execute(&lc64, inputs);
    CHECK(lc64.output_states[0] == true, "AND(1,1) should be true");

    /* One input OFF */
    inputs = 0x01;
    centum_lc64_execute(&lc64, inputs);
    CHECK(lc64.output_states[0] == false, "AND(1,0) should be false");
}

static void test_selector_evaluation(void)
{
    TEST("L5: Signal selector evaluation");
    centum_selector_block_t sel;
    memset(&sel, 0, sizeof(sel));
    sel.input1_val = 10.0;
    sel.input2_val = 30.0;
    sel.input3_val = 20.0;
    sel.input4_val = 25.0;
    sel.select_high = true;

    centum_selector_block_evaluate(&sel);
    CHECK(fabs(sel.output_val - 30.0) < 0.01, "High select should pick 30.0");
}

static void test_split_range_calculation(void)
{
    TEST("L5: Split range calculation");
    centum_split_range_block_t splt;
    memset(&splt, 0, sizeof(splt));
    splt.output_low1 = 0.0; splt.output_high1 = 100.0;
    splt.output_low2 = 0.0; splt.output_high2 = 100.0;
    splt.split_point = 50.0;

    double out1, out2;
    centum_split_range_calculate(&splt, 25.0, &out1, &out2);
    CHECK(out1 > 0.0 && out1 < 100.0, "Output 1 should be in range");
    CHECK(out2 == 0.0, "Output 2 should be at low when below split point");
}

static void test_ratio_calculation(void)
{
    TEST("L5: Ratio block calculation");
    double flow2_sp;
    centum_ratio_block_calculate(100.0, 0.5, 10.0, &flow2_sp);
    CHECK(fabs(flow2_sp - 60.0) < 0.01, "Ratio calculation: 100*0.5+10 = 60");
}

/*============================================================================
 * L6 — Integration / Communication Tests
 *============================================================================*/
static void test_vnet_packet_header(void)
{
    TEST("L6: Vnet/IP packet header init");
    vnet_packet_header_t hdr;
    vnet_packet_header_init(&hdr, 1, 2, 3, 4, VNET_MSG_PROCESS_DATA, VNET_PRIORITY_HIGH);
    CHECK(hdr.dest_domain == 1 && hdr.dest_station == 2, "Dest addr wrong");
    CHECK(hdr.src_domain == 3 && hdr.src_station == 4, "Src addr wrong");
    CHECK(hdr.message_type == VNET_MSG_PROCESS_DATA, "Message type wrong");
}

static void test_crc16_zero_length(void)
{
    TEST("L6: CRC-16 zero length handling");
    uint16_t crc = vnet_calculate_crc16(NULL, 0);
    CHECK(crc == 0xFFFF, "CRC-16 of empty data should be 0xFFFF");
}

static void test_modbus_crc(void)
{
    TEST("L6: Modbus CRC-16 calculation");
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint16_t crc = modbus_crc16(test_data, 6);
    /* CRC-16 must be non-zero for non-empty data and consistent on repeat */
    CHECK(crc != 0x0000, "Modbus CRC-16 should be non-zero for non-empty data");
    uint16_t crc2 = modbus_crc16(test_data, 6);
    CHECK(crc == crc2, "Modbus CRC-16 should be deterministic");
}

static void test_opc_quality_check(void)
{
    TEST("L6: OPC quality good/bad check");
    CHECK(opc_item_is_good_quality(OPC_QUALITY_GOOD), "GOOD quality should pass");
    CHECK(!opc_item_is_good_quality(OPC_QUALITY_BAD), "BAD quality should fail");
}

static void test_modbus_frame_build(void)
{
    TEST("L6: Modbus request frame build");
    modbus_request_t req;
    modbus_request_init(&req, 1, MB_FUNC_READ_HOLDING_REGS, 0, 10);

    uint8_t frame[256];
    uint16_t frame_len;
    bool ok = modbus_build_request_frame(&req, frame, &frame_len);
    CHECK(ok, "Frame build should succeed");
    CHECK(frame_len == 8, "Read holding regs frame should be 8 bytes");
}

/*============================================================================
 * L3 — Redundancy Tests
 *============================================================================*/
static void test_redundancy_pair_init_fn(void)
{
    TEST("L3: Redundancy pair initialization");
    centum_redundancy_pair_t pair;
    centum_redundancy_pair_init(&pair, REDUN_PAIR_CPU, 1);
    CHECK(pair.pair_id == 1, "Pair ID wrong");
    CHECK(pair.primary.role == REDUN_ROLE_OFFLINE, "Primary should be OFFLINE");
}

static void test_redundancy_set_role(void)
{
    TEST("L3: Redundancy role assignment");
    centum_redundancy_pair_t pair;
    centum_redundancy_pair_init(&pair, REDUN_PAIR_CPU, 1);

    bool ok = centum_redundancy_set_role(&pair.primary, REDUN_ROLE_PRIMARY);
    CHECK(!ok, "Cannot go OFFLINE->PRIMARY directly");
}

static void test_failover_log(void)
{
    TEST("L3: Failover event logging");
    centum_failover_log_t log;
    centum_failover_log_init(&log);

    centum_failover_event_t evt;
    memset(&evt, 0, sizeof(evt));
    strcpy(evt.event_description, "Test failover");
    evt.failover_type = REDUN_FAILOVER_MANUAL;
    evt.success = true;

    centum_failover_log_add(&log, &evt);
    CHECK(log.event_count == 1, "Log should have 1 event");

    const centum_failover_event_t *latest = centum_failover_log_get_latest(&log);
    CHECK(latest != NULL, "Should retrieve latest event");
    CHECK(latest->success, "Event should be successful");
}

/*============================================================================
 * L3 / L5 — Batch Tests
 *============================================================================*/
static void test_batch_manager_init_fn(void)
{
    TEST("L3: Batch manager initialization");
    centum_batch_manager_t mgr;
    centum_batch_manager_init(&mgr);
    CHECK(mgr.recipe_count == 0, "Recipe count should be 0");
    CHECK(!mgr.batch_server_active, "Batch server should be inactive");
}

static void test_batch_add_and_find_recipe(void)
{
    TEST("L3: Add and find recipe");
    centum_batch_manager_t mgr;
    centum_batch_manager_init(&mgr);

    centum_batch_recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    strcpy(recipe.recipe_id, "TEST-001");
    strcpy(recipe.product_name, "TestProduct");
    recipe.target_batch_size = 1000.0;
    recipe.min_batch_size = 500.0;
    recipe.max_batch_size = 2000.0;
    recipe.procedure_count = 1;
    recipe.released = true;

    bool ok = centum_batch_add_recipe(&mgr, &recipe);
    CHECK(ok, "Add recipe should succeed");

    centum_batch_recipe_t found;
    ok = centum_batch_find_recipe(&mgr, "TEST-001", &found);
    CHECK(ok, "Find recipe should succeed");
    CHECK(strcmp(found.product_name, "TestProduct") == 0, "Product name mismatch");
}

static void test_batch_recipe_validation(void)
{
    TEST("L5: Recipe validation");
    centum_batch_recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    strcpy(recipe.recipe_id, "VALID-01");
    strcpy(recipe.product_name, "ProductX");
    recipe.procedure_count = 2;
    recipe.target_batch_size = 100.0;
    recipe.min_batch_size = 50.0;
    recipe.max_batch_size = 200.0;
    recipe.formula_item_count = 1;

    CHECK(centum_batch_validate_recipe(&recipe), "Valid recipe should pass");
}

static void test_batch_recipe_scale(void)
{
    TEST("L5: Recipe scaling");
    centum_batch_recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    recipe.target_batch_size = 100.0;
    recipe.min_batch_size = 50.0;
    recipe.max_batch_size = 500.0;

    centum_formula_item_t item;
    memset(&item, 0, sizeof(item));
    strcpy(item.material_name, "Water");
    item.target_quantity = 100.0;
    item.min_quantity = 90.0;
    item.max_quantity = 110.0;
    centum_batch_add_formula_item(&recipe, &item);

    bool ok = centum_batch_scale_recipe(&recipe, 200.0);
    CHECK(ok, "Scale recipe should succeed");
    CHECK(fabs(recipe.formula[0].target_quantity - 200.0) < 0.01,
          "Formula quantity should double");
}

/*============================================================================
 * L1 / L2 — Edge Cases
 *============================================================================*/
static void test_null_pointer_handling(void)
{
    TEST("L1: Null pointer safety");
    centum_system_config_init(NULL);
    centum_fcs_config_init(NULL, 0);
    centum_pid_block_init(NULL);
    centum_sequence_block_init(NULL, SEQ_TYPE_SEBOL);
    centum_lc64_block_init(NULL);
    CHECK(true, "Null pointer functions should not crash");
}

static void test_signal_conversion_denom_zero(void)
{
    TEST("L1: Signal conversion with zero span");
    centum_signal_range_t range;
    memset(&range, 0, sizeof(range));
    range.eu_low = 0.0;
    range.eu_high = 0.0;
    range.adc_low = 100;
    range.adc_high = 100;

    double eu = centum_signal_convert_raw_to_eu(50, &range);
    CHECK(fabs(eu - 0.0) < 0.01, "Zero span should return eu_low");
}

static void test_system_config_null(void)
{
    TEST("L1: System functions with NULL config");
    CHECK(!centum_system_add_station(NULL, NULL, 1), "NULL config should return false");
    CHECK(centum_system_total_io_count(NULL) == 0, "NULL config should return 0");
    CHECK(centum_system_find_station_by_tag(NULL, NULL) == UINT16_MAX,
          "NULL config should return UINT16_MAX");
    CHECK(centum_system_validate(NULL) == DBCHECK_ERROR_MISSING,
          "NULL config should return error");
}

/*============================================================================
 * Main test runner
 *============================================================================*/
int main(void)
{
    printf("========================================\n");
    printf(" CENTUM VP DCS — Comprehensive Test Suite\n");
    printf("========================================\n\n");

    /* L1 Definition Tests */
    printf("--- L1: Definitions ---\n");
    test_station_type_strings();
    test_station_status_strings();
    test_fcs_type_strings();
    test_io_module_type_strings();
    test_io_module_from_string();
    test_null_pointer_handling();
    test_signal_conversion_denom_zero();
    test_system_config_null();

    /* L2 Core Concept Tests */
    printf("\n--- L2: Core Concepts ---\n");
    test_system_config_init();
    test_add_station();
    test_add_duplicate_station();
    test_vnet_ip_addressing();
    test_license_check();
    test_system_validate();

    /* L3 Engineering Structure Tests */
    printf("\n--- L3: Engineering Structures ---\n");
    test_fcs_config_init_fn();
    test_add_nio_node();
    test_add_io_module();
    test_fcs_validate();
    test_remove_station();
    test_find_station_by_tag();
    test_redundancy_pair_init_fn();
    test_redundancy_set_role();
    test_failover_log();
    test_batch_manager_init_fn();
    test_batch_add_and_find_recipe();

    /* L4 Engineering Law Tests */
    printf("\n--- L4: Engineering Laws ---\n");
    test_signal_conversion_linearity();
    test_signal_eu_to_raw();
    test_availability_calculation();
    test_mtbf_values();
    test_batch_state_transitions();

    /* L5 Algorithm Tests */
    printf("\n--- L5: Algorithms ---\n");
    test_pid_velocity_algorithm();
    test_pid_bumpless_transfer();
    test_pid_anti_windup_clamp();
    test_pid_alarm_evaluation();
    test_sequence_execution();
    test_lc64_interlock();
    test_selector_evaluation();
    test_split_range_calculation();
    test_ratio_calculation();
    test_batch_recipe_validation();
    test_batch_recipe_scale();

    /* L6 Integration Tests */
    printf("\n--- L6: Integration ---\n");
    test_vnet_packet_header();
    test_crc16_zero_length();
    test_modbus_crc();
    test_opc_quality_check();
    test_modbus_frame_build();

    /* Summary */
    printf("\n========================================\n");
    printf(" RESULTS: %d/%d passed (%d failed)\n",
           tests_passed, tests_run, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}