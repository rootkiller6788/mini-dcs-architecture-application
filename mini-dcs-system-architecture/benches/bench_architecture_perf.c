/**
 * @file bench_architecture_perf.c
 * @brief Performance benchmark for DCS architecture operations.
 *
 * Benchmarks:
 *   1. Architecture verification (10K iterations)
 *   2. PFD calculation (100K iterations)
 *   3. Alarm flood detection (1M timestamps)
 */
#include "dcs_architecture.h"
#include "dcs_safety.h"
#include "dcs_alarm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double get_time_sec(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(void)
{
    printf("=== DCS Architecture Benchmarks ===\n\n");

    double start, elapsed;
    volatile double result = 0.0;

    /* Benchmark 1: Architecture Verification */
    dcs_system_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_controller_nodes = 10;
    config.num_operator_stations = 4;
    config.num_io_subsystems = 8;
    config.total_ai_points = 500;
    config.total_ao_points = 200;
    config.total_di_points = 500;
    config.total_do_points = 300;
    config.controller_redundancy = 1;
    config.network_redundancy = 1;
    config.backbone_topology = DCS_TOPOLOGY_RING;
    config.backbone_speed_mbps = 1000.0;
    config.controller_scan_ms = 250.0;
    config.server_redundancy = 1;

    const int N_VERIFY = 10000;
    dcs_arch_verification_t ver_result;

    start = get_time_sec();
    for (int i = 0; i < N_VERIFY; i++) {
        dcs_verify_architecture(&config, &ver_result);
        result += (double)ver_result.violations;
    }
    elapsed = get_time_sec() - start;
    printf("Architecture Verification (%d iters): %.3f ms (%.0f ops/s)\n",
           N_VERIFY, elapsed * 1000.0, (double)N_VERIFY / elapsed);

    /* Benchmark 2: PFD Calculation */
    const int N_PFD = 100000;

    start = get_time_sec();
    for (int i = 0; i < N_PFD; i++) {
        double pfd = dcs_sif_calculate_pfd(NULL, 0, NULL, 0, NULL, 0,
                                            8760.0, 0.02);
        result += pfd;
    }
    elapsed = get_time_sec() - start;
    printf("PFD Calculation (%d iters):        %.3f ms (%.0f ops/s)\n",
           N_PFD, elapsed * 1000.0, (double)N_PFD / elapsed);

    /* Benchmark 3: Alarm Flood Detection */
    const int N_ALARMS = 1000000;
    uint64_t *timestamps = (uint64_t *)malloc(N_ALARMS * sizeof(uint64_t));
    if (timestamps != NULL) {
        for (int i = 0; i < N_ALARMS; i++) {
            timestamps[i] = (uint64_t)i * 1000; /* 1 second apart */
        }
        int32_t flood_start;
        uint32_t flood_count;

        start = get_time_sec();
        dcs_alarm_detect_flood(timestamps, N_ALARMS, 10.0, 10,
                                &flood_start, &flood_count);
        elapsed = get_time_sec() - start;
        printf("Alarm Flood Detect (%d alarms):    %.3f ms\n",
               N_ALARMS, elapsed * 1000.0);
        free(timestamps);
    }

    /* Benchmark 4: Controller Loading */
    const int N_LOAD = 100000;
    start = get_time_sec();
    for (int i = 0; i < N_LOAD; i++) {
        double load = dcs_analyze_controller_loading(50, 100, 50, 200, 100, 250.0);
        result += load;
    }
    elapsed = get_time_sec() - start;
    printf("Controller Loading (%d iters):      %.3f ms (%.0f ops/s)\n",
           N_LOAD, elapsed * 1000.0, (double)N_LOAD / elapsed);

    /* Prevent compiler from optimizing away the loop */
    if (result < 0.0) printf(".");

    printf("\n=== Benchmarks Complete ===\n");
    return 0;
}
