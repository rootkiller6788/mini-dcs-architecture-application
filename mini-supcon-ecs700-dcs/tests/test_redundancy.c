/**
 * @file    test_redundancy.c
 * @brief   Tests for ECS-700 Redundancy module
 */

#include "ecs700_redundancy.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define TEST(n) printf("  TEST: %s ... ", n)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); failed++; } while(0)
#define CHECK(c, m) do { if (c) PASS(); else FAIL(m); } while(0)

static void test_pair_init(void)
{
    TEST("redundancy pair init");
    ecs700_redundancy_pair_t pair;
    ecs700_redundancy_pair_init(&pair, 1, 10, 11);
    CHECK(pair.pair_id == 1, "pair ID");
    CHECK(pair.primary_node_id == 10, "primary ID");
    CHECK(pair.secondary_node_id == 11, "secondary ID");
    CHECK(pair.mode == ECS700_REDUNDANCY_1V1_HOT, "mode");
}

static void test_heartbeat(void)
{
    TEST("heartbeat processing");
    ecs700_redundancy_pair_t pair;
    ecs700_redundancy_pair_init(&pair, 1, 10, 11);
    ecs700_health_score_t health;
    memset(&health, 0, sizeof(health));
    health.cpu_load = 30.0;
    health.memory_available_mb = 300.0;
    health.temperature_c = 40.0;
    health.power_supply_v = 24.0;
    ecs700_redundancy_heartbeat(&pair, &health, 1000000);
    CHECK(pair.heartbeat_miss_count == 0, "heartbeat reset miss count");
    CHECK(pair.partner_healthy, "partner should be healthy");
}

static void test_failover(void)
{
    TEST("failover execution");
    ecs700_redundancy_pair_t pair;
    ecs700_redundancy_pair_init(&pair, 1, 10, 11);
    pair.heartbeat_miss_count = ECS700_HEARTBEAT_MISS_MAX;
    pair.local_health.cpu_load = 30.0;
    pair.local_health.memory_available_mb = 512.0;
    pair.local_health.temperature_c = 40.0;
    pair.local_health.power_supply_v = 24.0;
    int ret = ecs700_redundancy_failover(&pair, 2000000);
    CHECK(ret == 0, "failover should succeed");
    CHECK(pair.primary_node_id == 11, "secondary becomes primary");
    CHECK(pair.failover_count == 1, "failover count incremented");
}

static void test_health_score_perfect(void)
{
    TEST("health score perfect");
    ecs700_health_score_t health;
    memset(&health, 0, sizeof(health));
    health.cpu_load = 30.0;
    health.memory_available_mb = 512.0;
    health.temperature_c = 40.0;
    health.power_supply_v = 24.0;
    double score = ecs700_compute_health_score(&health);
    CHECK(score >= 90.0, "perfect health ≥ 90");
}

static void test_health_score_poor(void)
{
    TEST("health score poor");
    ecs700_health_score_t health;
    memset(&health, 0, sizeof(health));
    health.cpu_load = 95.0;
    health.memory_available_mb = 32.0;
    health.temperature_c = 90.0;
    health.power_supply_v = 10.0;
    health.watchdog_timeouts = 5;
    double score = ecs700_compute_health_score(&health);
    CHECK(score < 50.0, "poor health < 50");
}

static void test_path_health_init(void)
{
    TEST("path health init");
    ecs700_path_health_t path;
    ecs700_path_health_init(&path, 1, "SCnet-A");
    CHECK(path.path_id == 1, "path ID");
    CHECK(path.link_up, "link up default");
}

static void test_path_health_update(void)
{
    TEST("path health update");
    ecs700_path_health_t path;
    ecs700_path_health_init(&path, 1, "SCnet-A");
    ecs700_path_health_update(&path, 100, 95, 5, 500.0);
    CHECK(path.packets_sent == 100, "packets sent");
    CHECK(path.packets_lost == 5, "packets lost");
    CHECK(path.packet_loss_rate > 0.0, "packet loss rate computed");
}

static void test_availability(void)
{
    TEST("availability calculation");
    double a = ecs700_compute_availability(150000.0, 4.0);
    CHECK(a > 0.9999, "availability > 4-nines");
    CHECK(a <= 1.0, "availability ≤ 1.0");
}

static void test_pfd_avg(void)
{
    TEST("PFDavg calculation");
    double pfd = ecs700_compute_pfd_avg(6.67e-6, 8760.0);
    CHECK(pfd > 0.0 && pfd < 1.0, "PFDavg in (0, 1)");
    CHECK(pfd < 0.01, "PFDavg < 1e-2 (SIL 2 range)");
}

int main(void)
{
    printf("\n=== ECS-700 Redundancy Tests ===\n\n");
    test_pair_init();
    test_heartbeat();
    test_failover();
    test_health_score_perfect();
    test_health_score_poor();
    test_path_health_init();
    test_path_health_update();
    test_availability();
    test_pfd_avg();
    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
