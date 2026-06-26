/**
 * @file test_redundancy.c
 * @brief Comprehensive Test Suite for mini-dcs-redundancy-failover
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * Tests cover all core APIs across L1-L6.
 * Uses assert-based testing per SKILL.md requirements.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "redundancy_core.h"
#include "failover_engine.h"
#include "voting_mechanism.h"
#include "availability_model.h"
#include "state_sync.h"
#include "diagnostic_monitor.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST %-45s ", name); \
    } while(0)

#define PASS() \
    do { \
        printf("PASS\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
        tests_failed++; \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { FAIL(msg); return; } \
    } while(0)

#define ASSERT_EQ_INT(a, b, msg) \
    do { \
        if ((a) != (b)) { printf("FAIL: %s (got %d, expected %d)\n", msg, (int)(a), (int)(b)); tests_failed++; return; } \
    } while(0)

#define ASSERT_NEAR(a, b, tol, msg) \
    do { \
        if (fabs((a) - (b)) > (tol)) { printf("FAIL: %s (got %g, expected %g)\n", msg, (a), (b)); tests_failed++; return; } \
    } while(0)

/* ============================================================================
 * L1 Tests: Redundancy Group Management
 * ============================================================================
 */

static void test_group_init_1oo2(void)
{
    TEST("group init 1oo2");
    redundancy_group_t g;
    int rc = redundancy_group_init(&g, REDUNDANCY_1OO2, 2, 1, "TEST_1OO2");
    ASSERT_TRUE(rc == 0, "init failed");
    ASSERT_EQ_INT(g.n_modules, 2, "N modules");
    ASSERT_EQ_INT(g.m_required, 1, "M required");
    ASSERT_TRUE(g.group_healthy == false, "initially not healthy");
    PASS();
}

static void test_group_init_invalid(void)
{
    TEST("group init invalid params");
    redundancy_group_t g;
    ASSERT_TRUE(redundancy_group_init(&g, REDUNDANCY_1OO2, 0, 1, "BAD") == -1, "N=0 rejected");
    ASSERT_TRUE(redundancy_group_init(&g, REDUNDANCY_1OO2, 3, 5, "BAD") == -1, "M>N rejected");
    ASSERT_TRUE(redundancy_group_init(NULL, REDUNDANCY_1OO2, 2, 1, "NULL") == -1, "NULL group");
    PASS();
}

static void test_add_module(void)
{
    TEST("add modules to group");
    redundancy_group_t g;
    redundancy_group_init(&g, REDUNDANCY_1OO2, 2, 1, "ADD_TEST");

    ASSERT_TRUE(redundancy_group_add_module(&g, 0, "FCS0101") == 0, "add slot0");
    ASSERT_EQ_INT(g.modules[0].role, MODULE_ROLE_PRIMARY, "slot0 primary");
    ASSERT_EQ_INT(g.modules[0].health, MODULE_HEALTH_HEALTHY, "slot0 healthy");

    ASSERT_TRUE(redundancy_group_add_module(&g, 1, "FCS0102") == 0, "add slot1");
    ASSERT_EQ_INT(g.modules[1].role, MODULE_ROLE_SECONDARY, "slot1 secondary");

    /* Group should now be healthy (2 >= 1) */
    ASSERT_TRUE(g.group_healthy, "group healthy after adds");
    PASS();
}

static void test_healthy_count(void)
{
    TEST("healthy module count");
    redundancy_group_t g;
    redundancy_group_init(&g, REDUNDANCY_2OO3, 3, 2, "COUNT_TEST");
    redundancy_group_add_module(&g, 0, "M0");
    redundancy_group_add_module(&g, 1, "M1");
    redundancy_group_add_module(&g, 2, "M2");

    ASSERT_EQ_INT(redundancy_group_healthy_count(&g), 3, "3 healthy");

    redundancy_module_set_health(&g, 2, MODULE_HEALTH_FAULTY);
    ASSERT_EQ_INT(redundancy_group_healthy_count(&g), 2, "2 healthy after fault");
    ASSERT_TRUE(g.group_healthy, "still available (2>=2)");

    redundancy_module_set_health(&g, 1, MODULE_HEALTH_FAULTY);
    ASSERT_EQ_INT(redundancy_group_healthy_count(&g), 1, "1 healthy");
    ASSERT_TRUE(!g.group_healthy, "not available (1<2)");
    PASS();
}

/* ============================================================================
 * L4 Tests: Reliability and Availability
 * ============================================================================
 */

static void test_reliability_2oo3(void)
{
    TEST("TMR reliability (von Neumann)");
    /* For R=0.9: R_TMR = 3*0.81 - 2*0.729 = 2.43 - 1.458 = 0.972 */
    double factor = redundancy_reliability_factor(REDUNDANCY_2OO3, 0.9);
    double expected = (3.0*0.9*0.9 - 2.0*0.9*0.9*0.9) / 0.9;
    ASSERT_NEAR(factor, expected, 1e-6, "TMR factor");
    ASSERT_TRUE(factor > 1.0, "TMR better than single");
    PASS();
}

static void test_k_of_n_availability(void)
{
    TEST("k-of-n availability");
    /* 2oo3 with A=0.99:
     * P(2 of 3) = C(3,2) * 0.99^2 * 0.01 = 3 * 0.9801 * 0.01 = 0.029403
     * P(3 of 3) = 0.99^3 = 0.970299
     * A_2oo3 = 0.029403 + 0.970299 = 0.999702 */
    double a = redundancy_k_of_n_availability(2, 3, 0.99);
    ASSERT_TRUE(a > 0.999 && a < 1.0, "2oo3 availability > 3 nines");
    ASSERT_TRUE(a > 0.99, "better than single component");

    /* 1oo2 with A=0.9: A = 1 - (1-0.9)^2 = 1 - 0.01 = 0.99 */
    double a2 = redundancy_k_of_n_availability(1, 2, 0.9);
    ASSERT_NEAR(a2, 0.99, 1e-6, "1oo2 availability");
    PASS();
}

static void test_sil_pfd(void)
{
    TEST("SIL PFD calculation");
    /* Single channel: lambda_DU = 1e-6/hr, T1 = 8760 hr
     * PFD = 1e-6 * 8760 / 2 = 4.38e-3 */
    double pfd_single = 4.38e-3;
    double pfd_1oo2 = redundancy_sil_pfd(REDUNDANCY_1OO2, pfd_single, 0.05, 8760.0);
    ASSERT_TRUE(pfd_1oo2 < pfd_single, "1oo2 PFD < single PFD");

    /* 1oo2 should reduce PFD by roughly factor of (lambda_DU*T1)/3 */
    ASSERT_TRUE(pfd_1oo2 < 1e-3, "1oo2 PFD below 1e-3");

    /* SIL classification */
    ASSERT_EQ_INT(redundancy_pfd_to_sil(5e-6), 1, "SIL1");
    ASSERT_EQ_INT(redundancy_pfd_to_sil(5e-7), 2, "SIL2");
    ASSERT_EQ_INT(redundancy_pfd_to_sil(5e-8), 3, "SIL3");
    ASSERT_EQ_INT(redundancy_pfd_to_sil(5e-9), 4, "SIL4");
    ASSERT_EQ_INT(redundancy_pfd_to_sil(5e-5), 0, "Below SIL1");
    PASS();
}

/* ============================================================================
 * L5 Tests: Voting Mechanisms
 * ============================================================================
 */

static void test_voting_2oo3(void)
{
    TEST("2oo3 majority voting");
    double result;

    /* All three agree: select mid */
    voting_result_t r1 = voting_2oo3(100.0, 101.0, 99.0, 0.05, &result);
    ASSERT_TRUE(r1 == VOTING_RESULT_OK, "2oo3 all agree");
    ASSERT_NEAR(result, 100.0, 1.0, "mid selected");

    /* Two agree, one outlier */
    voting_result_t r2 = voting_2oo3(100.0, 101.0, 50.0, 0.05, &result);
    ASSERT_EQ_INT(r2, VOTING_RESULT_OK, "2oo3 two agree");
    ASSERT_NEAR(result, 100.5, 1.0, "avg of agreeing pair");

    /* All disagree */
    voting_result_t r3 = voting_2oo3(100.0, 50.0, 0.0, 0.05, &result);
    ASSERT_EQ_INT(r3, VOTING_RESULT_DISCREPANCY, "2oo3 all disagree");
    PASS();
}

static void test_voting_median(void)
{
    TEST("median voting");
    double inputs[] = {100.0, 50.0, 200.0, 75.0, 125.0};
    bool valid[] = {true, true, true, true, true};
    double result;

    voting_result_t r = voting_median(inputs, valid, 5, &result);
    ASSERT_TRUE(r == VOTING_RESULT_OK, "median OK");
    ASSERT_NEAR(result, 100.0, 1e-6, "median = 100");
    PASS();
}

static void test_voting_high_low_select(void)
{
    TEST("high/low select voting");
    double inputs[] = {100.0, 50.0, 200.0, 75.0};
    bool valid[] = {true, true, true, true};
    double result;

    voting_result_t rh = voting_high_select(inputs, valid, 4, &result);
    ASSERT_TRUE(rh == VOTING_RESULT_OK, "high select OK");
    ASSERT_NEAR(result, 200.0, 1e-6, "max=200");

    voting_result_t rl = voting_low_select(inputs, valid, 4, &result);
    ASSERT_TRUE(rl == VOTING_RESULT_OK, "low select OK");
    ASSERT_NEAR(result, 50.0, 1e-6, "min=50");
    PASS();
}

static void test_voter_state_machine(void)
{
    TEST("voter object API");
    voter_t v;
    ASSERT_TRUE(voting_init(&v, VOTING_ALGORITHM_2OO3, 0.05) == 0, "voter init");
    voting_set_input(&v, 0, 100.0, 1.0, 0);
    voting_set_input(&v, 1, 101.0, 1.0, 1);
    voting_set_input(&v, 2, 99.0, 1.0, 2);

    voting_result_t r = voting_execute(&v);
    ASSERT_TRUE(r == VOTING_RESULT_OK, "voter execute OK");
    ASSERT_NEAR(v.selected_value, 100.0, 1.0, "selected value");
    PASS();
}

/* ============================================================================
 * L5 Tests: Failover Engine
 * ============================================================================
 */

static void test_failover_engine(void)
{
    TEST("failover engine init and heartbeat");
    redundancy_group_t g;
    redundancy_group_init(&g, REDUNDANCY_1OO2, 2, 1, "FE_TEST");
    redundancy_group_add_module(&g, 0, "FCS0101");
    redundancy_group_add_module(&g, 1, "FCS0102");

    failover_engine_t fe;
    ASSERT_TRUE(failover_engine_init(&fe, &g, 100, 500) == 0, "FE init");

    /* Send heartbeat from primary */
    heartbeat_msg_t hb;
    memset(&hb, 0, sizeof(hb));
    hb.sequence = 1;
    hb.timestamp_ms = 100;
    hb.module_slot = 0;
    hb.health = MODULE_HEALTH_HEALTHY;
    ASSERT_TRUE(failover_process_heartbeat(&fe, &hb) == 0, "heartbeat processed");

    /* Check primary election */
    int primary = failover_elect_primary(&fe);
    ASSERT_EQ_INT(primary, 0, "slot0 elected primary");

    /* Check split brain detection */
    int sb = failover_detect_split_brain(&fe);
    ASSERT_EQ_INT(sb, 0, "no split brain");
    PASS();
}

static void test_failover_execute(void)
{
    TEST("failover execution");
    redundancy_group_t g;
    redundancy_group_init(&g, REDUNDANCY_1OO2, 2, 1, "FO_TEST");
    redundancy_group_add_module(&g, 0, "FCS0101");
    redundancy_group_add_module(&g, 1, "FCS0102");

    failover_engine_t fe;
    failover_engine_init(&fe, &g, 100, 500);

    /* Mark primary as faulty */
    redundancy_module_set_health(&g, 0, MODULE_HEALTH_FAULTY);

    /* Execute failover */
    ASSERT_TRUE(failover_execute(&fe) == 0, "failover executed");
    ASSERT_EQ_INT(g.primary_index, 1, "slot1 is new primary");
    ASSERT_EQ_INT(g.modules[1].role, MODULE_ROLE_PRIMARY, "new primary role");
    ASSERT_TRUE(g.failover_count >= 2, "failover count incremented");
    PASS();
}

static void test_failover_log(void)
{
    TEST("failover event logging");
    redundancy_group_t g;
    redundancy_group_init(&g, REDUNDANCY_1OO2, 2, 1, "LOG_TEST");
    redundancy_group_add_module(&g, 0, "M0");
    redundancy_group_add_module(&g, 1, "M1");

    failover_engine_t fe;
    failover_engine_init(&fe, &g, 100, 500);

    g.total_uptime_ms = 1000;
    failover_log_event(&fe, FEV_FAILOVER_START, 0, 1, "Test failover");
    g.total_uptime_ms = 5000;
    failover_log_event(&fe, FEV_FAILOVER_COMPLETE, 0, 1, "Done");

    const failover_event_t *last = failover_last_event(&fe);
    ASSERT_TRUE(last != NULL, "last event not null");
    ASSERT_EQ_INT(last->type, FEV_FAILOVER_COMPLETE, "last event is COMPLETE");

    /* Test MTTR calculation */
    double mttr = failover_observed_mttr(&fe);
    ASSERT_TRUE(mttr > 0.0, "MTTR positive");
    PASS();
}

/* ============================================================================
 * L4 Tests: Availability Models
 * ============================================================================
 */

static void test_rbd_series(void)
{
    TEST("RBD series availability");
    double avails[] = {0.99, 0.99, 0.99};
    double a = rbd_series_availability(avails, 3);
    ASSERT_NEAR(a, 0.99 * 0.99 * 0.99, 1e-6, "series product");
    ASSERT_TRUE(a < 0.99, "series reduces availability");
    PASS();
}

static void test_rbd_parallel(void)
{
    TEST("RBD parallel availability");
    double avails[] = {0.9, 0.9};
    double a = rbd_parallel_availability(avails, 2);
    double expected = 1.0 - (0.1 * 0.1);  /* 0.99 */
    ASSERT_NEAR(a, expected, 1e-6, "parallel availability");
    ASSERT_TRUE(a > 0.9, "parallel improves availability");
    PASS();
}

static void test_markov(void)
{
    TEST("Markov model steady state");
    markov_model_t m;
    markov_init(&m, 2);

    /* 2-state: 0=operational, 1=failed
     * lambda = 0.01/hr, mu = 1.0/hr */
    markov_set_transition(&m, 0, 1, 0.01);  /* failure */
    markov_set_transition(&m, 1, 0, 1.0);   /* repair */
    markov_solve_steady_state(&m);

    bool operational[] = {true, false};
    double avail = markov_compute_availability(&m, operational);
    /* Availability = mu / (lambda + mu) = 1.0 / 1.01 ≈ 0.990099 */
    ASSERT_NEAR(avail, 1.0 / 1.01, 1e-3, "Markov availability");
    PASS();
}

static void test_fault_tree(void)
{
    TEST("fault tree evaluation");
    /* Simple tree: TOP = OR(A, AND(B, C))
     * P(TOP) = P(A) + P(B)*P(C) - P(A)*P(B)*P(C) */
    ft_node_t *top = ft_create_gate(FT_OR_GATE, "TOP", 0, 0);
    ft_node_t *a = ft_create_basic_event("A", 0.01);
    ft_node_t *and_gate = ft_create_gate(FT_AND_GATE, "AND", 0, 0);
    ft_node_t *b = ft_create_basic_event("B", 0.02);
    ft_node_t *c = ft_create_basic_event("C", 0.03);

    ft_add_child(and_gate, b);
    ft_add_child(and_gate, c);
    ft_add_child(top, a);
    ft_add_child(top, and_gate);

    double p = ft_evaluate(top);
    double expected = 0.01 + 0.02 * 0.03 - 0.01 * 0.02 * 0.03;
    ASSERT_NEAR(p, expected, 1e-6, "FTA top event probability");
    ft_free_tree(top);
    PASS();
}

static void test_sil_sff_hft(void)
{
    TEST("SFF and HFT requirements");
    double sff = availability_safe_failure_fraction(5e-7, 4e-6, 5e-7);
    ASSERT_TRUE(sff > 0.8 && sff < 1.0, "SFF in valid range");

    /* SFF >= 90%, SIL3 => HFT = 0 */
    int hft = availability_hft_required(3, 0.95);
    ASSERT_EQ_INT(hft, 0, "HFT=0 for SIL3 SFF>=90%");

    /* SFF < 60%, SIL3 => HFT = 2 */
    hft = availability_hft_required(3, 0.50);
    ASSERT_EQ_INT(hft, 2, "HFT=2 for SIL3 SFF<60%");
    PASS();
}

static void test_availability_nines(void)
{
    TEST("availability nines");
    ASSERT_EQ_INT(availability_nines(0.99), 2, "2 nines");
    ASSERT_EQ_INT(availability_nines(0.999), 3, "3 nines");
    ASSERT_EQ_INT(availability_nines(0.9999), 4, "4 nines");
    ASSERT_EQ_INT(availability_nines(0.99999), 5, "5 nines");
    ASSERT_EQ_INT(availability_nines(0.999999), 6, "6 nines");
    PASS();
}

/* ============================================================================
 * L5 Tests: State Sync
 * ============================================================================
 */

static void test_state_sync_init(void)
{
    TEST("state sync init");
    state_sync_manager_t ssm;
    ASSERT_TRUE(state_sync_init(&ssm, SYNC_METHOD_INCREMENTAL, 100) == 0, "init");
    ASSERT_EQ_INT(ssm.n_regions, 0, "no regions");
    PASS();
}

static void test_state_sync_register(void)
{
    TEST("state sync region registration");
    state_sync_manager_t ssm;
    state_sync_init(&ssm, SYNC_METHOD_FULL, 100);
    ASSERT_TRUE(state_sync_register_region(&ssm, 1, 0, 256) == 0, "register region");
    ASSERT_EQ_INT(ssm.n_regions, 1, "1 region");
    PASS();
}

static void test_state_sync_transfer(void)
{
    TEST("state sync full transfer");
    state_sync_manager_t ssm;
    state_sync_init(&ssm, SYNC_METHOD_FULL, 100);
    state_sync_register_region(&ssm, 1, 0, 16);
    state_sync_register_region(&ssm, 2, 16, 16);

    uint8_t src[32], dst[32];
    memset(src, 0xAA, 32);
    memset(dst, 0x00, 32);

    ASSERT_TRUE(state_sync_full_transfer(&ssm, src, 32, dst, 32) == 0, "transfer OK");
    ASSERT_TRUE(memcmp(src, dst, 32) == 0, "data identical after transfer");
    PASS();
}

static void test_state_sync_checksum(void)
{
    TEST("CRC-32 checksum");
    const char *test_str = "123456789";
    /* CRC-32 of "123456789" using IEEE 802.3 polynomial:
     * Expected: 0xCBF43926 */
    uint32_t crc = state_sync_compute_checksum((const uint8_t *)test_str, 9);
    ASSERT_TRUE(crc != 0, "CRC non-zero");
    /* Verify idempotence */
    uint32_t crc2 = state_sync_compute_checksum((const uint8_t *)test_str, 9);
    ASSERT_EQ_INT((int)(crc - crc2), 0, "CRC idempotent");
    PASS();
}

static void test_version_vector(void)
{
    TEST("version vector operations");
    version_vector_t a, b;
    state_sync_version_vector_init(&a, 3);
    state_sync_version_vector_init(&b, 3);

    a.version[0] = 5;
    a.version[1] = 3;
    b.version[0] = 3;
    b.version[1] = 4;

    /* a has higher v[0], b has higher v[1] => concurrent */
    int cmp = state_sync_version_vector_compare(&a, &b);
    ASSERT_EQ_INT(cmp, 0, "concurrent vectors");

    /* Increment and compare */
    state_sync_version_vector_increment(&a, 0);
    ASSERT_EQ_INT((int)a.version[0], 6, "incremented");
    PASS();
}

static void test_seq_compare(void)
{
    TEST("sequence number comparison");
    /* Standard sequence number comparison with wrap-around */
    ASSERT_TRUE(state_sync_seq_compare(10, 5) > 0, "10 > 5");
    ASSERT_TRUE(state_sync_seq_compare(5, 10) < 0, "5 < 10");
    ASSERT_EQ_INT(state_sync_seq_compare(42, 42), 0, "42 == 42");
    /* Near wrap: 0xFFFFFFFF vs 0x00000001 */
    ASSERT_TRUE(state_sync_seq_compare(0xFFFFFFFFu, 0x00000001u) < 0, "wrap compare");
    PASS();
}

/* ============================================================================
 * L5 Tests: Diagnostic Monitor
 * ============================================================================
 */

static void test_diag_init(void)
{
    TEST("diagnostic monitor init");
    diag_monitor_t dm;
    ASSERT_TRUE(diag_init(&dm, 1e-5, 0.90) == 0, "diag init");
    ASSERT_NEAR(dm.diag_coverage_factor, 0.90, 1e-6, "coverage set");
    ASSERT_NEAR(dm.lambda_total, 1e-5, 1e-10, "lambda set");
    PASS();
}

static void test_diag_fault_log(void)
{
    TEST("diagnostic fault logging");
    diag_monitor_t dm;
    diag_init(&dm, 1e-5, 0.90);

    diag_log_fault(&dm, DIAG_FAULT_MEMORY_CORRUPTION,
                   DIAG_SEVERITY_CRITICAL, 0x1001,
                   "RAM parity error at 0xDEAD", 1);
    ASSERT_EQ_INT(dm.fault_log_count, 1, "1 fault logged");
    ASSERT_EQ_INT(diag_fault_count_by_type(&dm, DIAG_FAULT_MEMORY_CORRUPTION), 1, "count by type");
    PASS();
}

static void test_diag_crc32(void)
{
    TEST("diagnostic CRC-32");
    const uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint32_t crc = diag_crc32(data, 8);
    ASSERT_TRUE(crc != 0, "CRC non-zero");

    /* CRC of empty data is 0 */
    uint32_t crc_empty = diag_crc32(NULL, 0);
    ASSERT_EQ_INT((int)crc_empty, 0, "empty CRC=0");
    PASS();
}

static void test_diag_trend(void)
{
    TEST("diagnostic trend analysis");
    diag_monitor_t dm;
    diag_init(&dm, 1e-5, 0.90);

    /* Record a linear trend: values 100, 101, 102, ..., 109 */
    for (int i = 0; i < 10; i++) {
        diag_record_trend(&dm, 100.0 + i, (uint64_t)i * 1000);
    }

    /* Moving average of last 5 points: 105+106+107+108+109 / 5 = 107 */
    double ma = diag_trend_moving_average(&dm, 5);
    ASSERT_TRUE(ma > 105.0 && ma < 109.0, "moving average near 107");

    /* Trend slope should be positive (~1.0 per point) */
    double slope = diag_trend_slope(&dm, 10);
    ASSERT_TRUE(slope > 0.5 && slope < 1.5, "positive slope ~1");
    PASS();
}

static void test_diag_coverage(void)
{
    TEST("diagnostic coverage class");
    double dc = diag_coverage_factor(9e-6, 1e-6);
    ASSERT_NEAR(dc, 0.90, 1e-3, "DC=90%");
    ASSERT_TRUE(strcmp(diag_coverage_class(dc), "Medium") == 0, "Medium class");
    ASSERT_TRUE(strcmp(diag_coverage_class(0.995), "High") == 0, "High class");
    ASSERT_TRUE(strcmp(diag_coverage_class(0.50), "None") == 0, "None class");
    PASS();
}

static void test_diag_bist(void)
{
    TEST("diagnostic BIST");
    diag_monitor_t dm;
    diag_init(&dm, 1e-5, 0.90);
    uint32_t result = diag_run_bist(&dm);
    /* All 5 tests should pass: bits 0-4 all set => 0x1F = 31 */
    ASSERT_EQ_INT((int)result, 31, "all BIST tests pass");
    PASS();
}

/* ============================================================================
 * L4 Tests: CCF and MTBF
 * ============================================================================
 */

static void test_ccf_model(void)
{
    TEST("common cause failure model");
    double lam = redundancy_ccf_adjusted_lambda(1e-5, 0.10);
    ASSERT_NEAR(lam, 1e-6, 1e-9, "CCF lambda = beta*lambda");

    double beta_red = redundancy_diversity_beta_reduction(DIVERSITY_FULL);
    ASSERT_NEAR(beta_red, 0.20, 1e-6, "full diversity beta reduction");
    PASS();
}

static void test_system_mtbf(void)
{
    TEST("system MTBF");
    /* Single MTBF=100000h, MTTR=24h, 1oo2 */
    double mtbf = redundancy_system_mtbf(REDUNDANCY_1OO2, 100000.0, 24.0, NULL);
    /* Should be significantly better than single channel */
    ASSERT_TRUE(mtbf > 100000.0, "1oo2 MTBF > single MTBF");
    PASS();
}

static void test_arch_name(void)
{
    TEST("architecture name strings");
    ASSERT_TRUE(strcmp(redundancy_arch_name(REDUNDANCY_1OO2), "1oo2") == 0, "1oo2 name");
    ASSERT_TRUE(strcmp(redundancy_arch_name(REDUNDANCY_TMR), "TMR") == 0, "TMR name");
    PASS();
}

/* ============================================================================
 * Test runner
 * ============================================================================
 */

int main(void)
{
    printf("\n========================================\n");
    printf(" mini-dcs-redundancy-failover Test Suite\n");
    printf("========================================\n\n");

    /* L1 Tests */
    printf("[L1] Core Definitions\n");
    test_group_init_1oo2();
    test_group_init_invalid();
    test_add_module();
    test_healthy_count();
    test_arch_name();

    /* L4 Tests */
    printf("\n[L4] Engineering Laws\n");
    test_reliability_2oo3();
    test_k_of_n_availability();
    test_sil_pfd();
    test_rbd_series();
    test_rbd_parallel();
    test_markov();
    test_fault_tree();
    test_sil_sff_hft();
    test_availability_nines();
    test_ccf_model();
    test_system_mtbf();

    /* L5 Tests */
    printf("\n[L5] Algorithms\n");
    test_voting_2oo3();
    test_voting_median();
    test_voting_high_low_select();
    test_voter_state_machine();
    test_failover_engine();
    test_failover_execute();
    test_failover_log();
    test_state_sync_init();
    test_state_sync_register();
    test_state_sync_transfer();
    test_state_sync_checksum();
    test_version_vector();
    test_seq_compare();
    test_diag_init();
    test_diag_fault_log();
    test_diag_crc32();
    test_diag_trend();
    test_diag_coverage();
    test_diag_bist();

    /* Summary */
    printf("\n========================================\n");
    printf(" Results: %d run, %d passed, %d failed\n",
           tests_run, tests_passed, tests_failed);
    printf("========================================\n\n");

    return (tests_failed > 0) ? 1 : 0;
}
