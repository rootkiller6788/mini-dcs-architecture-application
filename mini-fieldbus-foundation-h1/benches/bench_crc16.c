/**
 * bench_crc16.c ? CRC-16-CCITT throughput benchmark
 */
#include "ff_h1_physical.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void) {
    #define BENCH_SIZE (1024 * 1024)  /* 1 MB */
    static uint8_t data[BENCH_SIZE];
    for (int i = 0; i < BENCH_SIZE; i++) data[i] = (uint8_t)(i & 0xFF);

    clock_t start = clock();
    uint16_t crc = ff_crc16_ccitt(data, BENCH_SIZE);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double throughput_mbps = (BENCH_SIZE / (1024.0 * 1024.0)) / elapsed;

    printf("CRC-16-CCITT Benchmark:\n");
    printf("  Data size: %d bytes (%.2f MB)\n", BENCH_SIZE,
           BENCH_SIZE / (1024.0 * 1024.0));
    printf("  CRC result: 0x%04X\n", crc);
    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Throughput: %.2f MB/s\n", throughput_mbps);

    return 0;
}