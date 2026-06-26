/**
 * test_segment.c ? Tests for ff_h1_segment.h
 */

#include "ff_h1_segment.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

static int passed = 0, failed = 0;
#define T(name) do { printf("  TEST: %s ... ", name); } while(0)
#define P() do { printf("PASS\n"); passed++; } while(0)
#define F(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define AE(a,b,m) do { if((a)!=(b)){F(m);return;} } while(0)
#define AT(c,m) do { if(!(c)){F(m);return;} } while(0)

static void test_cable_loop_resistance(void) {
    T("Cable loop resistance");
    double r = ff_cable_loop_resistance(1000.0, FF_CABLE_TYPE_A, 20.0);
    AT(r > 40.0 && r < 48.0, "1km Type A ~44? at 20?C");

    /* At higher temp, resistance should increase */
    double r_hot = ff_cable_loop_resistance(1000.0, FF_CABLE_TYPE_A, 60.0);
    AT(r_hot > r, "higher temp ? higher resistance");
    P();
}

static void test_cable_voltage_drop(void) {
    T("Cable voltage drop");
    double vd = ff_cable_voltage_drop(500.0, FF_CABLE_TYPE_A, 500.0, 20.0);
    /* R ? 44 * 0.5 = 22?, I = 0.5A, V_drop = 22 * 0.5 = 11V */
    AT(vd > 9.0 && vd < 13.0, "500m, 500mA ? ~11V drop");
    P();
}

static void test_max_trunk_length(void) {
    T("Max trunk length calculation");
    double max_l = ff_max_trunk_length(24.0, 9.0, 200.0, FF_CABLE_TYPE_A, 20.0);
    /* Budget = 24 - 9 - 0.5 = 14.5V, I = 0.2A, R_max = 14.5/0.2 = 72.5?
     * R_km = 44?/km ? max ? 72.5/44 = 1.648km = 1648m, but capped at 1900m */
    AT(max_l > 1000.0 && max_l < 2000.0, "feasible length");
    P();
}

static void test_max_trunk_length_infeasible(void) {
    T("Max trunk length infeasible");
    /* Very high current, very low voltage ? infeasible */
    double max_l = ff_max_trunk_length(8.0, 9.0, 500.0, FF_CABLE_TYPE_A, 20.0);
    AT(max_l < 0.0, "high current with low supply ? infeasible");
    P();
}

static void test_power_budget(void) {
    T("Segment power budget");
    ff_segment_config_t config;
    memset(&config, 0, sizeof(config));
    config.power_supply.output_voltage_v = 24.0;
    config.power_supply.max_current_ma = 500.0;
    config.power_supply.conditioner_drop_v = 0.5;
    config.trunk_cable_type = FF_CABLE_TYPE_A;
    config.trunk_length_m = 500.0;
    config.num_devices = 4;
    config.temperature_c = 25.0;

    for (int i = 0; i < 4; i++) {
        config.device_current_ma[i] = 20.0;
        config.spur_cable_type[i] = FF_CABLE_TYPE_A;
        config.spur_length_m[i] = 30.0;
    }

    ff_power_budget_result_t result;
    AE(ff_segment_power_budget(&config, &result), 0, "power budget calc ok");
    AT(result.is_viable, "segment should be viable");
    AT(result.min_device_voltage_v > 9.0, "all devices > 9V");
    AT(result.margin_ma > 0.0, "positive current margin");
    P();
}

static void test_power_budget_overload(void) {
    T("Segment power budget overload");
    ff_segment_config_t config;
    memset(&config, 0, sizeof(config));
    config.power_supply.output_voltage_v = 24.0;
    config.power_supply.max_current_ma = 100.0;
    config.trunk_cable_type = FF_CABLE_TYPE_A;
    config.trunk_length_m = 500.0;
    config.num_devices = 10;
    config.temperature_c = 25.0;

    for (int i = 0; i < 10; i++) {
        config.device_current_ma[i] = 20.0;
        config.spur_cable_type[i] = FF_CABLE_TYPE_A;
        config.spur_length_m[i] = 10.0;
    }

    ff_power_budget_result_t result;
    ff_segment_power_budget(&config, &result);
    AT(!result.is_viable, "overloaded segment should not be viable");
    AT(result.power_supply_utilization > 1.0, "utilization > 100%");
    P();
}

static void test_spur_validation(void) {
    T("Spur validation");
    double spurs_ok[] = {50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0};
    AT(ff_segment_validate_spurs(8, spurs_ok), "8 devices, 120m limit ? OK");

    double spurs_bad[] = {121.0, 10.0};
    AT(!ff_segment_validate_spurs(2, spurs_bad), "spur too long ? fail");
    P();
}

static void test_fisco_compatibility(void) {
    T("FISCO compatibility check");
    ff_entity_params_t device = {
        .type = FF_IS_TYPE_FISCO, .ui_v = 24.0, .ii_ma = 250.0, .pi_w = 1.2,
        .uo_v = 0, .io_ma = 0, .po_w = 0, .ci_nf = 2.0, .li_uh = 5.0
    };
    ff_entity_params_t source = {
        .type = FF_IS_TYPE_FISCO, .ui_v = 0, .ii_ma = 0, .pi_w = 0,
        .uo_v = 15.0, .io_ma = 180.0, .po_w = 1.0, .ci_nf = 0, .li_uh = 0
    };
    AT(ff_fisco_verify_compatibility(&device, &source), "FISCO compatible");
    P();
}

static void test_segment_health(void) {
    T("Segment health evaluate");
    ff_segment_diagnostics_t diag = {
        .signal_level_pp_v = 0.85, .dc_voltage_v = 22.0, .noise_pp_mv = 25.0,
        .retransmission_rate = 0.001, .frame_error_rate = 0.000001,
        .devices_detected = 8, .devices_expected = 8, .power_supply_current_ma = 160.0
    };
    AE(ff_segment_health_evaluate(&diag), FF_SEGMENT_HEALTH_GOOD, "all good");
    P();
}

static void test_segment_health_critical(void) {
    T("Segment health critical");
    ff_segment_diagnostics_t diag = {
        .signal_level_pp_v = 0.85, .dc_voltage_v = 7.5, .noise_pp_mv = 25.0,
        .retransmission_rate = 0.001, .frame_error_rate = 0.000001,
        .devices_detected = 8, .devices_expected = 8, .power_supply_current_ma = 160.0
    };
    AE(ff_segment_health_evaluate(&diag), FF_SEGMENT_HEALTH_CRITICAL, "low voltage ? critical");
    P();
}

static void test_commissioning(void) {
    T("Commissioning checklist");
    ff_commissioning_checklist_t cl = {1,1,1,1,1,1,1,1};
    AE(ff_commissioning_pass_count(&cl), 8, "all 8 pass");
    AT(ff_commissioning_ready(&cl), "all pass ? ready");

    cl.time_sync_ok = 0;
    AE(ff_commissioning_pass_count(&cl), 7, "7 pass");
    AT(!ff_commissioning_ready(&cl), "missing time sync ? not ready");
    P();
}

int main(void) {
    printf("=== test_segment ===\n");
    test_cable_loop_resistance();
    test_cable_voltage_drop();
    test_max_trunk_length();
    test_max_trunk_length_infeasible();
    test_power_budget();
    test_power_budget_overload();
    test_spur_validation();
    test_fisco_compatibility();
    test_segment_health();
    test_segment_health_critical();
    test_commissioning();
    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}