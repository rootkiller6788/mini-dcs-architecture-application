/**
 * @file    bench_redundancy.c
 * @brief   ECS-700 Redundancy Performance Benchmark
 *
 * Benchmarks: failover execution time, health score computation
 * throughput, and data synchronization latency.
 */

#include "ecs700_redundancy.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

static double time_diff_ms(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000.0
         + (end->tv_nsec - start->tv_nsec) / 1000000.0;
}

int main(void)
{
    printf("\n=== ECS-700 Redundancy Performance Benchmarks ===\n\n");

    struct timespec t_start, t_end;
    const int ITERATIONS = 100000;

    /* Benchmark 1: Health Score Computation */
    printf("  Benchmark 1: Health Score Computation (%d iterations)\n", ITERATIONS);
    ecs700_health_score_t health;
    memset(&health, 0, sizeof(health));
    health.cpu_load = 45.0;
    health.memory_available_mb = 300.0;
    health.temperature_c = 50.0;
    health.power_supply_v = 23.8;

    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        volatile double s = ecs700_compute_health_score(&health);
        (void)s;
    }
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    printf("    Total: %.2f ms, Per iteration: %.3f μs\n",
           time_diff_ms(&t_start, &t_end),
           time_diff_ms(&t_start, &t_end) * 1000.0 / ITERATIONS);

    /* Benchmark 2: Failover Execution */
    printf("\n  Benchmark 2: Failover Execution (%d iterations)\n", ITERATIONS / 10);
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS / 10; i++) {
        ecs700_redundancy_pair_t pair;
        ecs700_redundancy_pair_init(&pair, 1, 10, 11);
        pair.heartbeat_miss_count = ECS700_HEARTBEAT_MISS_MAX + 1;
        pair.local_health = health;
        volatile int ret = ecs700_redundancy_failover(&pair, i * 1000000ULL);
        (void)ret;
    }
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    printf("    Total: %.2f ms, Per failover: %.3f μs\n",
           time_diff_ms(&t_start, &t_end),
           time_diff_ms(&t_start, &t_end) * 1000.0 / (ITERATIONS / 10));

    /* Benchmark 3: Availability/PFD Calculation */
    printf("\n  Benchmark 3: Availability + PFD Calculation (%d iterations)\n",
           ITERATIONS);
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        volatile double a = ecs700_compute_availability(150000.0, 4.0);
        volatile double p = ecs700_compute_pfd_avg(1e-5, 8760.0);
        (void)a; (void)p;
    }
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    printf("    Total: %.2f ms, Per pair: %.3f μs\n",
           time_diff_ms(&t_start, &t_end),
           time_diff_ms(&t_start, &t_end) * 1000.0 / ITERATIONS);

    /* Benchmark 4: Path Health Update */
    printf("\n  Benchmark 4: Path Health Update (%d iterations)\n", ITERATIONS);
    ecs700_path_health_t path;
    ecs700_path_health_init(&path, 1, "SCnet-A");
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (int i = 0; i < ITERATIONS; i++) {
        ecs700_path_health_update(&path, 100, i % 101, i % 5, 500.0);
    }
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    printf("    Total: %.2f ms, Per update: %.3f μs\n",
           time_diff_ms(&t_start, &t_end),
           time_diff_ms(&t_start, &t_end) * 1000.0 / ITERATIONS);

    printf("\n=== Benchmarks Complete ===\n\n");
    return 0;
}
