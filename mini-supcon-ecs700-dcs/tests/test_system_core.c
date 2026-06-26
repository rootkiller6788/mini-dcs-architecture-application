/**
 * @file    test_system_core.c
 * @brief   Tests for ECS-700 System Core module
 *
 * Tests: system init, domain registration, load factor computation,
 * network bandwidth estimation, configuration validation, signal
 * scaling correctness, signal filtering, and rate-of-change.
 */

#include "ecs700_system_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s ... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

/* --- System Initialization --- */

static void test_system_init_valid(void)
{
    TEST("system_init with valid name");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "TestPlant");
    CHECK(strcmp(config.system_name, "TestPlant") == 0,
          "system name mismatch");
}

static void test_system_init_null(void)
{
    TEST("system_init with NULL config (no crash)");
    ecs700_system_init(NULL, "test");
    PASS();  /* Should not crash */
}

static void test_system_init_defaults(void)
{
    TEST("system_init defaults");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "Plant");
    CHECK(config.global_scan_period_us == ECS700_CS_STANDARD_SCAN_US,
          "default scan period mismatch");
    CHECK(config.scnet_redundancy_enabled == true,
          "SCnet redundancy should default true");
    CHECK(config.time_sync_enabled == true,
          "time sync should default true");
    CHECK(config.num_domains == 0,
          "should have 0 domains initially");
}

/* --- Domain Registration --- */

static void test_domain_register_single(void)
{
    TEST("register single domain");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "Test");
    uint8_t id = ecs700_domain_register(&config, "Reactor Area");
    CHECK(id == 1, "first domain should get ID 1");
    CHECK(config.num_domains == 1, "domain count should be 1");
}

static void test_domain_register_max(void)
{
    TEST("register maximum domains");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "Test");
    for (int i = 0; i < ECS700_MAX_DOMAINS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "Domain_%d", i);
        uint8_t id = ecs700_domain_register(&config, name);
        assert(id == (uint8_t)(i + 1));
    }
    /* One more should fail */
    uint8_t id = ecs700_domain_register(&config, "Overflow");
    CHECK(id == 0, "should fail when at max capacity");
}

/* --- Control Station Assignment --- */

static void test_domain_add_cs_valid(void)
{
    TEST("add CS to domain");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "Test");
    ecs700_domain_register(&config, "Area1");
    bool ok = ecs700_domain_add_cs(&config, 1, 100);
    CHECK(ok, "should succeed");
    CHECK(config.domains[0].num_control_stations == 1,
          "CS count should be 1");
    CHECK(config.total_control_stations == 1,
          "total CS count should be 1");
}

static void test_domain_add_cs_duplicate(void)
{
    TEST("add duplicate CS (should fail)");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "Test");
    ecs700_domain_register(&config, "Area1");
    ecs700_domain_add_cs(&config, 1, 100);
    bool ok = ecs700_domain_add_cs(&config, 1, 100);
    CHECK(!ok, "duplicate CS should be rejected");
}

/* --- Load Factor Computation --- */

static void test_load_factor_normal(void)
{
    TEST("load factor 50%");
    double load = ecs700_compute_load_factor(100000.0, 200000.0);
    CHECK(fabs(load - 50.0) < 0.01, "50% load factor");
}

static void test_load_factor_zero_period(void)
{
    TEST("load factor with zero period (100%)");
    double load = ecs700_compute_load_factor(100.0, 0.0);
    CHECK(fabs(load - 100.0) < 0.01, "zero period → 100% load");
}

static void test_load_factor_overload(void)
{
    TEST("load factor over 100% (capped)");
    double load = ecs700_compute_load_factor(300000.0, 200000.0);
    CHECK(fabs(load - 100.0) < 0.01, "overload capped at 100%");
}

/* --- Network Bandwidth Estimation --- */

static void test_bandwidth_basic(void)
{
    TEST("basic bandwidth estimation");
    double bw = ecs700_estimate_network_bandwidth(1000, 200000, 32);
    CHECK(bw > 0.0, "bandwidth should be positive");
}

static void test_bandwidth_zero_period(void)
{
    TEST("bandwidth with zero period");
    double bw = ecs700_estimate_network_bandwidth(100, 0, 32);
    CHECK(bw == 0.0, "zero period → zero bandwidth");
}

/* --- Configuration Validation --- */

static void test_validate_empty_domains(void)
{
    TEST("validate with empty domains");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "Test");
    int ret = ecs700_validate_config(&config);
    CHECK(ret == 2, "should error: no domains");
}

static void test_validate_valid_config(void)
{
    TEST("validate valid configuration");
    ecs700_system_config_t config;
    ecs700_system_init(&config, "TestPlant");
    ecs700_domain_register(&config, "Area1");
    ecs700_domain_add_cs(&config, 1, 100);
    int ret = ecs700_validate_config(&config);
    CHECK(ret == 0, "should be valid");
}

/* --- Signal Scaling --- */

static void test_raw_to_eu_midpoint(void)
{
    TEST("raw to EU midpoint");
    ecs700_eu_range_t range = {0.0, 65535.0, 0.0, 100.0, "%", 2};
    double eu = ecs700_raw_to_eu(32767, &range);
    CHECK(fabs(eu - 50.0) < 0.1, "midpoint should be ~50%");
}

static void test_raw_to_eu_zero(void)
{
    TEST("raw to EU zero point");
    ecs700_eu_range_t range = {0.0, 65535.0, 4.0, 20.0, "mA", 3};
    double eu = ecs700_raw_to_eu(0, &range);
    CHECK(fabs(eu - 4.0) < 0.01, "raw=0 → eu=4.0 mA");
}

static void test_raw_to_eu_null_range(void)
{
    TEST("raw to EU null range (no crash)");
    double eu = ecs700_raw_to_eu(100, NULL);
    CHECK(eu == 0.0, "null range → 0.0");
}

static void test_eu_to_raw_roundtrip(void)
{
    TEST("EU ↔ Raw roundtrip");
    ecs700_eu_range_t range = {0.0, 65535.0, -50.0, 50.0, "°C", 1};
    double original_eu = 25.0;
    uint16_t raw = ecs700_eu_to_raw(original_eu, &range);
    double back_eu = ecs700_raw_to_eu(raw, &range);
    CHECK(fabs(back_eu - original_eu) < 0.1, "roundtrip accuracy");
}

/* --- Signal Filtering --- */

static void test_signal_filter_basic(void)
{
    TEST("signal filter basic response");
    double filtered = ecs700_apply_signal_filter(100.0, 0.0, 0.2, 1.0);
    CHECK(filtered > 0.0 && filtered < 100.0, "filtered value between 0 and 100");
}

static void test_signal_filter_steady_state(void)
{
    TEST("signal filter steady state convergence");
    double filt = 0.0;
    for (int i = 0; i < 100; i++) {
        filt = ecs700_apply_signal_filter(50.0, filt, 0.2, 1.0);
    }
    CHECK(fabs(filt - 50.0) < 1.0, "convergence to input value");
}

/* --- Rate of Change --- */

static void test_pv_rate_positive(void)
{
    TEST("positive rate of change");
    double rate = ecs700_compute_pv_rate(60.0, 50.0, 1.0, 0.0);
    CHECK(fabs(rate - 10.0) < 0.01, "rate = 10 units/s");
}

static void test_pv_rate_deadband(void)
{
    TEST("rate of change with deadband");
    double rate = ecs700_compute_pv_rate(50.1, 50.0, 1.0, 0.5);
    CHECK(rate == 0.0, "below deadband → zero rate");
}

static void test_pv_rate_negative(void)
{
    TEST("negative rate of change");
    double rate = ecs700_compute_pv_rate(40.0, 50.0, 2.0, 0.0);
    CHECK(fabs(rate - (-5.0)) < 0.01, "rate = -5 units/s");
}

/* --- Health Collection --- */

static void test_collect_health_basic(void)
{
    TEST("collect health basic");
    ecs700_system_config_t config;
    ecs700_system_health_t health;
    ecs700_system_init(&config, "Test");
    ecs700_domain_register(&config, "Area1");
    ecs700_domain_add_cs(&config, 1, 10);
    ecs700_collect_health(&config, &health);
    CHECK(health.active_domains == 1, "one active domain");
    CHECK(health.primary_controllers == 1, "one controller");
}

/* --- Test Runner --- */

int main(void)
{
    printf("\n=== ECS-700 System Core Tests ===\n\n");

    test_system_init_valid();
    test_system_init_null();
    test_system_init_defaults();
    test_domain_register_single();
    test_domain_register_max();
    test_domain_add_cs_valid();
    test_domain_add_cs_duplicate();
    test_load_factor_normal();
    test_load_factor_zero_period();
    test_load_factor_overload();
    test_bandwidth_basic();
    test_bandwidth_zero_period();
    test_validate_empty_domains();
    test_validate_valid_config();
    test_raw_to_eu_midpoint();
    test_raw_to_eu_zero();
    test_raw_to_eu_null_range();
    test_eu_to_raw_roundtrip();
    test_signal_filter_basic();
    test_signal_filter_steady_state();
    test_pv_rate_positive();
    test_pv_rate_deadband();
    test_pv_rate_negative();
    test_collect_health_basic();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
