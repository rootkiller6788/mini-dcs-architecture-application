/**
 * @file    test_io_subsystem.c
 * @brief   Tests for ECS-700 I/O Subsystem module
 */

#include "ecs700_io_subsystem.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define TEST(n) printf("  TEST: %s ... ", n)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); failed++; } while(0)
#define CHECK(c, m) do { if (c) PASS(); else FAIL(m); } while(0)

static void test_io_channel_init(void)
{
    TEST("I/O channel init");
    ecs700_io_channel_t ch;
    ecs700_io_channel_init(&ch, 0, ECS700_SIGNAL_AI_4_20MA, 0.0, 100.0, "%", "FIC101");
    CHECK(ch.channel_index == 0, "channel index");
    CHECK(ch.signal_type == ECS700_SIGNAL_AI_4_20MA, "signal type");
    CHECK(strcmp(ch.tag, "FIC101") == 0, "tag");
    CHECK(ch.filter_tc == ECS700_DEFAULT_FILTER_TC, "default filter TC");
}

static void test_io_process_input(void)
{
    TEST("I/O process input 4-20mA");
    ecs700_io_channel_t ch;
    ecs700_io_channel_init(&ch, 0, ECS700_SIGNAL_AI_4_20MA, 0.0, 100.0, "%", "TAG1");
    /* 32767 ≈ 50% of 4-20mA range → ~12mA → ~50% EU */
    ecs700_io_process_input(&ch, 32767, 1000000);
    CHECK(ch.quality == ECS700_IO_QUALITY_GOOD, "quality good");
    CHECK(ch.eu_value > 0.0 && ch.eu_value < 100.0, "EU value in range");
}

static void test_io_set_output(void)
{
    TEST("I/O set output");
    ecs700_io_channel_t ch;
    ecs700_io_channel_init(&ch, 0, ECS700_SIGNAL_AO_4_20MA, 0.0, 100.0, "%", "TAG2");
    uint16_t dac = ecs700_io_set_output(&ch, 50.0);
    CHECK(dac > 32000 && dac < 33500, "50% output ~32767");
}

static void test_open_wire_detection(void)
{
    TEST("NAMUR NE43 open wire");
    CHECK(ecs700_io_detect_open_wire(1.0), "1 mA → open wire");
    CHECK(ecs700_io_detect_open_wire(0.0), "0 mA → open wire");
    CHECK(!ecs700_io_detect_open_wire(4.0), "4 mA → normal");
    CHECK(!ecs700_io_detect_open_wire(12.0), "12 mA → normal");
}

static void test_overrange_detection(void)
{
    TEST("NAMUR NE43 overrange");
    CHECK(ecs700_io_detect_overrange(21.0), "21 mA → overrange");
    CHECK(!ecs700_io_detect_overrange(12.0), "12 mA → normal");
}

static void test_io_module_init(void)
{
    TEST("I/O module init");
    ecs700_io_module_t module;
    ecs700_io_module_init(&module, 1, ECS700_MODULE_AI711, 10, "AI-Module-1");
    CHECK(module.module_id == 1, "module ID");
    CHECK(module.module_type == ECS700_MODULE_AI711, "module type");
    CHECK(module.num_channels == 8, "8 analog inputs");
}

static void test_cjc_compensate(void)
{
    TEST("CJC compensate type K");
    /* Type K at 100°C produces about 4.096 mV
     * With CJC at 25°C → ~1.015 mV compensation */
    double temp = ecs700_io_cjc_compensate(4.096, 25.0, 0);
    CHECK(temp > 50.0 && temp < 150.0, "compensated temperature in range");
}

static void test_sqrt_extract(void)
{
    TEST("sqrt extraction");
    double flow = ecs700_io_sqrt_extract(25.0, 1.0);
    CHECK(fabs(flow - 50.0) < 1.0, "sqrt(25%) → 50% flow");
    double flow_cut = ecs700_io_sqrt_extract(0.5, 1.0);
    CHECK(flow_cut == 0.0, "below cut-off → zero");
}

static void test_rtd_to_temp(void)
{
    TEST("RTD to temperature Pt100 0°C");
    double temp = ecs700_io_rtd_to_temp(100.0);
    CHECK(fabs(temp) < 2.0, "100Ω → ~0°C");
}

static void test_accuracy_calculation(void)
{
    TEST("accuracy % of span");
    double acc = ecs700_io_accuracy_pct_span(100.3, 100.0, 200.0);
    CHECK(fabs(acc - 0.15) < 0.01, "0.3 error on 200 span → 0.15%");
}

static void test_snr_calculation(void)
{
    TEST("SNR calculation");
    double snr = ecs700_io_compute_snr(10.0, 0.01);
    CHECK(snr >= 59.0 && snr <= 61.0, "SNR ~60 dB for 1000:1 ratio");
}

static void test_enob(void)
{
    TEST("ENOB calculation");
    double enob = ecs700_io_enob(85.0);
    CHECK(enob > 13.0 && enob < 14.5, "85dB SNR → ~13.8 bits ENOB");
}

int main(void)
{
    printf("\n=== ECS-700 I/O Subsystem Tests ===\n\n");
    test_io_channel_init();
    test_io_process_input();
    test_io_set_output();
    test_open_wire_detection();
    test_overrange_detection();
    test_io_module_init();
    test_cjc_compensate();
    test_sqrt_extract();
    test_rtd_to_temp();
    test_accuracy_calculation();
    test_snr_calculation();
    test_enob();
    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
