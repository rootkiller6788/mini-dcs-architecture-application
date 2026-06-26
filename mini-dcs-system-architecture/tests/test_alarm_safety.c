/**
 * @file test_alarm_safety.c
 * @brief Tests for DCS alarm management (L2-L6) and safety (L4-L6).
 */
#include "dcs_alarm.h"
#include "dcs_safety.h"
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
    printf("\n=== DCS Alarm & Safety Tests ===\n\n");

    /* ---- Alarm Tests ---- */

    dcs_alarm_config_t alarm;
    memset(&alarm, 0, sizeof(alarm));
    alarm.alarm_id = 1;
    alarm.type = DCS_ALARM_ABSOLUTE;
    alarm.priority = DCS_ALARM_PRIORITY_HIGH;
    alarm.setpoint = 100.0;
    alarm.hysteresis = 2.0;
    alarm.on_delay_s = 0.5;
    alarm.off_delay_s = 0.5;
    alarm.enabled = 1;
    alarm.response_time_s = 300.0;  /* 5 minutes to respond */
    strcpy(alarm.alarm_description, "High temperature alarm on reactor R-101");
    strcpy(alarm.consequence, "Reactor overpressure and potential rupture");

    /* L3: Alarm State Machine */
    TEST("Alarm - normal → unacknowledged active");
    int changed = dcs_alarm_process(&alarm, 105.0);
    CHECK(changed == 1 && alarm.current_state == DCS_ALARM_STATE_UNACK_ACTIVE,
          "should transition to unacknowledged active");

    TEST("Alarm - acknowledge active alarm");
    int ack = dcs_alarm_acknowledge(&alarm);
    CHECK(ack == 1 && alarm.current_state == DCS_ALARM_STATE_ACK_ACTIVE,
          "should transition to acknowledged active");

    TEST("Alarm - clear → unacknowledged clear");
    changed = dcs_alarm_process(&alarm, 90.0);
    CHECK(changed == 1 && alarm.current_state == DCS_ALARM_STATE_UNACK_CLEAR,
          "should transition to unacknowledged clear on value drop");

    /* L3: Shelving */
    /* Reset alarm state */
    alarm.current_state = DCS_ALARM_STATE_ACK_ACTIVE;
    TEST("Alarm - shelve acknowledged alarm");
    int shelved = dcs_alarm_shelve(&alarm, 30.0);
    CHECK(shelved == 1 && alarm.current_state == DCS_ALARM_STATE_SHELVED,
          "should shelve acknowledged alarm");

    /* L5: Alarm Rationalization */
    TEST("Alarm - rationalize with defined description");
    dcs_alarm_rationalization_t rationalization;
    strcpy(rationalization.operator_action, "Reduce feed rate and activate cooling");
    int justified = dcs_alarm_rationalize(&alarm, &rationalization);
    CHECK(justified == 1, "alarm with description should be justified");

    /* L5: Flood Detection */
    TEST("Alarm flood - normal rate (no flood)");
    uint64_t timestamps[] = {0, 120000, 240000, 360000, 480000};
    int32_t flood_start = -1;
    uint32_t flood_count = 0;
    int flood = dcs_alarm_detect_flood(timestamps, 5, 10.0, 10,
                                        &flood_start, &flood_count);
    CHECK(flood == 0, "spaced alarms should not trigger flood");

    TEST("Alarm flood - burst triggers flood");
    uint64_t burst[] = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
    flood = dcs_alarm_detect_flood(burst, 11, 1.0, 10,
                                    &flood_start, &flood_count);
    CHECK(flood == 1, "burst of alarms should trigger flood");

    /* L5: KPI Calculation */
    dcs_alarm_system_kpi_t kpi;
    dcs_alarm_config_t alarm_list[5];
    memset(alarm_list, 0, sizeof(alarm_list));
    for (int i = 0; i < 5; i++) {
        alarm_list[i].alarm_id = i + 1;
        alarm_list[i].enabled = 1;
        alarm_list[i].priority = (dcs_alarm_priority_t)((i % 4) + 1);
    }
    alarm_list[0].current_state = DCS_ALARM_STATE_UNACK_ACTIVE;
    alarm_list[1].current_state = DCS_ALARM_STATE_ACK_ACTIVE;
    int kpi_ok = dcs_alarm_calculate_kpi(alarm_list, 5, 24.0, &kpi);
    TEST("Alarm KPI - calculation succeeds");
    CHECK(kpi_ok == 1, "KPI calculation should succeed");

    TEST("Alarm KPI - standing alarms counted");
    CHECK(kpi.standing_alarms == 2, "should count 2 standing alarms");

    /* L6: Hysteresis Recommendation */
    double rec_hyst;
    (void)dcs_alarm_recommend_hysteresis(100.0, 1.5, &rec_hyst);
    TEST("Alarm chatter - recommends hysteresis");
    CHECK(rec_hyst > 0.0, "should recommend positive hysteresis");

    /* ---- Safety Tests ---- */

    /* L5: PFD - 1oo1 */
    double pfd_1oo1 = dcs_pfd_1oo1(1e-6, 8760.0);
    TEST("PFD 1oo1 - within expected range");
    CHECK(pfd_1oo1 > 0.0 && pfd_1oo1 < 1.0,
          "PFD for 1oo1 should be in (0,1)");

    /* PFD formula verification: PFD_1oo1 = λ_DU * TI / 2 */
    double expected_1oo1 = 1e-6 * 8760.0 / 2.0;
    TEST("PFD 1oo1 - matches formula");
    CHECK(fabs(pfd_1oo1 - expected_1oo1) < 1e-10,
          "PFD 1oo1 should equal λ*TI/2");

    /* L5: PFD - 1oo2 (better than 1oo1 due to redundancy) */
    double pfd_1oo2 = dcs_pfd_1oo2(1e-6, 8760.0, 0.02);
    TEST("PFD 1oo2 - better than 1oo1 (lower PFD)");
    CHECK(pfd_1oo2 < pfd_1oo1, "1oo2 should have lower PFD than 1oo1");

    /* L5: PFD - 2oo3 */
    double pfd_2oo3 = dcs_pfd_2oo3(1e-6, 8760.0, 0.02);
    TEST("PFD 2oo3 - between 1oo1 and 1oo2");
    CHECK(pfd_2oo3 > pfd_1oo2 && pfd_2oo3 < pfd_1oo1,
          "2oo3 PFD should be between 1oo1 and 1oo2");

    /* L5: PFD - 2oo2 (worse than 1oo1) */
    double pfd_2oo2 = dcs_pfd_2oo2(1e-6, 8760.0, 0.02);
    TEST("PFD 2oo2 - worse than 1oo1");
    CHECK(pfd_2oo2 > pfd_1oo1, "2oo2 should have higher PFD than 1oo1");

    /* L5: Complete SIF PFD */
    dcs_safety_component_reliability_t sensor, logic, actuator;
    memset(&sensor, 0, sizeof(sensor));
    sensor.lambda_du_per_hour = 1e-6;
    sensor.common_cause_beta = 0.02;

    memset(&logic, 0, sizeof(logic));
    logic.lambda_du_per_hour = 5e-7;
    logic.common_cause_beta = 0.02;

    memset(&actuator, 0, sizeof(actuator));
    actuator.lambda_du_per_hour = 2e-6;
    actuator.common_cause_beta = 0.02;

    double pfd_sif = dcs_sif_calculate_pfd(&sensor, DCS_REDUNDANCY_1OO2,
                                            &logic, DCS_REDUNDANCY_1OO1,
                                            &actuator, DCS_REDUNDANCY_1OO2,
                                            8760.0, 0.02);
    TEST("SIF PFD - complete calculation positive");
    CHECK(pfd_sif > 0.0, "SIF PFD should be positive");

    /* L5: SIL Determination */
    dcs_sil_level_t sil = dcs_determine_sil(5e-4, DCS_HFT_1,
                                              95.0, DCS_COMPONENT_TYPE_B);
    TEST("SIL determination - PFD 5e-4 → SIL 2 (PFD limit; SFF=95% for Type B caps HFT=1 at SIL3)");
    CHECK(sil == DCS_SIL_2, "should achieve SIL 2 (limited by PFD)");

    /* Edge: PFD too high for any SIL */
    sil = dcs_determine_sil(0.1, DCS_HFT_0, 50.0, DCS_COMPONENT_TYPE_B);
    TEST("SIL determination - PFD too high → SIL NONE");
    CHECK(sil == DCS_SIL_NONE, "high PFD should result in no SIL");

    /* L5: RRF */
    TEST("RRF calculation");
    double rrf_val = dcs_calculate_rrf(1e-6, 1e-3);
    CHECK(rrf_val > 1.0, "RRF should be > 1");

    /* L6: Max Proof Test Interval */
    double max_ti = dcs_calculate_max_ti(1e-6, 1e-3, DCS_REDUNDANCY_1OO1, 0.02);
    TEST("Max TI - 1oo1 for SIL 2");
    CHECK(max_ti > 0.0, "max TI should be positive");

    /* L6: SIF Verification — 1oo2 logic + shorter TI for achievable SIL 2 */
    dcs_sif_definition_t sif;
    memset(&sif, 0, sizeof(sif));
    sif.sif_id = 1;
    sif.target_sil = DCS_SIL_2;
    sif.sensor_arch = DCS_REDUNDANCY_1OO2;
    sif.logic_arch = DCS_REDUNDANCY_1OO2;   /* 1oo2 logic for lower PFD */
    sif.actuator_arch = DCS_REDUNDANCY_1OO2;
    sif.proof_test_interval_hours = 8760.0;  /* 12 months */

    int sif_ok = dcs_sif_verify(&sif, &sensor, &logic, &actuator);
    TEST("SIF verification - passes for SIL 2 with 1oo2 logic");
    CHECK(sif_ok == 1 && sif.is_verified == 1,
          "SIF should pass verification for SIL 2");

    /* Summary */
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
