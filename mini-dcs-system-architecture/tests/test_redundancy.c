/**
 * @file test_redundancy.c
 * @brief Tests for DCS redundancy management (L2-L6).
 */
#include "dcs_redundancy.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s ... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

int main(void)
{
    printf("\n=== DCS Redundancy Tests ===\n\n");

    /* L3: Redundancy Init */
    dcs_redundant_pair_t pair;
    int init_ok = dcs_redundancy_init(&pair, 10, 11,
                                       DCS_REDUNDANCY_MODE_HOT_STANDBY,
                                       DCS_REDUNDANCY_1OO2);
    TEST("Redundancy init - valid pair");
    CHECK(init_ok == 1 && pair.primary_active == 1,
          "should initialize hot standby pair");

    TEST("Redundancy init - null pair rejected");
    CHECK(dcs_redundancy_init(NULL, 10, 11, DCS_REDUNDANCY_MODE_HOT_STANDBY,
                              DCS_REDUNDANCY_1OO2) == 0,
          "NULL pair should be rejected");

    TEST("Redundancy init - duplicate nodes rejected");
    CHECK(dcs_redundancy_init(&pair, 10, 10, DCS_REDUNDANCY_MODE_HOT_STANDBY,
                              DCS_REDUNDANCY_1OO2) == 0,
          "duplicate node IDs should be rejected");

    /* Reset for further tests */
    dcs_redundancy_init(&pair, 10, 11, DCS_REDUNDANCY_MODE_HOT_STANDBY,
                         DCS_REDUNDANCY_1OO2);

    /* L3: Standby Ready */
    TEST("Standby ready - hot standby initially ready");
    CHECK(dcs_redundancy_standby_ready(&pair, 95.0) == 1,
          "hot standby should start ready");

    /* L3: Synchronize */
    TEST("Synchronize - updates sync completeness");
    uint32_t synced = 0;
    double sync_pct = dcs_redundancy_synchronize(&pair, &synced);
    CHECK(sync_pct <= 100.0, "sync completeness should not exceed 100%");

    /* L3: Switchover */
    dcs_node_health_t health;
    memset(&health, 0, sizeof(health));
    health.node_id = 10;
    health.power_ok = 0;  /* Primary has failed */
    health.overall_healthy = 0;

    TEST("Switchover - fault-triggered with unhealthy primary");
    int sw_result = dcs_redundancy_switchover(&pair, DCS_SWITCHOVER_FAULT,
                                               &health);
    CHECK(sw_result == 1, "switchover should succeed when primary unhealthy");

    TEST("Switchover - roles swapped");
    CHECK(pair.primary_active == 1 && pair.primary_node_id == 11,
          "standby (11) should now be primary");

    /* L3: Switchover Time */
    TEST("Switchover time - hot standby < 1 scan");
    double sw_time = dcs_calculate_switchover_time(DCS_REDUNDANCY_MODE_HOT_STANDBY, 250.0);
    CHECK(sw_time < 250.0, "hot standby switchover should be < 1 scan");

    TEST("Switchover time - cold standby >> hot");
    double sw_cold = dcs_calculate_switchover_time(DCS_REDUNDANCY_MODE_COLD_STANDBY, 250.0);
    CHECK(sw_cold > sw_time, "cold standby switchover should be slower");

    /* L5: Bumpless Transfer */
    dcs_bumpless_config_t bt_config = {5.0, 1.0, 5.0}; /* 5s ramp, 5%/step */
    double new_output = 0.0;
    dcs_bumpless_transfer_step(20.0, 80.0, 100.0, 0.1,
                                &bt_config, &new_output);
    TEST("Bumpless transfer - step towards target");
    CHECK(new_output > 20.0 && new_output <= 80.0,
          "output should move towards target");

    TEST("Bumpless transfer - null config");
    CHECK(dcs_bumpless_transfer_step(0, 100, 100, 0.1, NULL, &new_output) == 0,
          "NULL config should return 0");

    /* L6: Availability Analysis */
    double a_1oo1 = dcs_analyze_redundancy_availability(0.99, DCS_REDUNDANCY_1OO1);
    double a_1oo2 = dcs_analyze_redundancy_availability(0.99, DCS_REDUNDANCY_1OO2);
    TEST("Availability - 1oo2 better than 1oo1");
    CHECK(a_1oo2 > a_1oo1, "1oo2 should have higher availability than 1oo1");

    double a_2oo2 = dcs_analyze_redundancy_availability(0.99, DCS_REDUNDANCY_2OO2);
    TEST("Availability - 2oo2 worse than 1oo1 (safety architecture)");
    CHECK(a_2oo2 < a_1oo1, "2oo2 should have lower availability than 1oo1");

    double a_2oo3 = dcs_analyze_redundancy_availability(0.99, DCS_REDUNDANCY_2OO3);
    TEST("Availability - 2oo3 between 1oo1 and 1oo2");
    CHECK(a_2oo3 > a_1oo1 && a_2oo3 < a_1oo2,
          "2oo3 availability should be between 1oo1 and 1oo2");

    /* Edge cases */
    TEST("Availability - 0% component");
    CHECK(dcs_analyze_redundancy_availability(0.0, DCS_REDUNDANCY_1OO2) == 0.0,
          "0% component → 0% system");

    TEST("Availability - 100% component");
    CHECK(dcs_analyze_redundancy_availability(1.0, DCS_REDUNDANCY_1OO2) == 1.0,
          "100% component → 100% system");

    /* L6: Effective MTTR */
    double mttr_1oo1 = dcs_calculate_effective_mttr(8.0, DCS_REDUNDANCY_1OO1);
    double mttr_1oo2 = dcs_calculate_effective_mttr(8.0, DCS_REDUNDANCY_1OO2);
    TEST("Effective MTTR - 1oo2 much better than 1oo1");
    CHECK(mttr_1oo2 > mttr_1oo1,
          "1oo2 effective MTTR should exceed 1oo1");

    /* Summary */
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
