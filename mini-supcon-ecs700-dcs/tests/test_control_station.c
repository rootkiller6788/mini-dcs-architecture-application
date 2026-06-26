/**
 * @file    test_control_station.c
 * @brief   Tests for ECS-700 Control Station module
 *
 * Tests: PID init, PID execution, anti-windup, bumpless transfer,
 * cascade control, SFC execution, interlock logic, alarm detection,
 * and relay auto-tuning.
 */

#include "ecs700_control_station.h"
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

/* --- PID Initialization --- */

static void test_pid_init_basic(void)
{
    TEST("PID init basic parameters");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "FIC101", 1.5, 30.0, 5.0, 0.2);
    CHECK(strcmp(pid.tag, "FIC101") == 0, "tag set correctly");
    CHECK(fabs(pid.kp - 1.5) < 1e-6, "Kp set correctly");
    CHECK(fabs(pid.ti - 30.0) < 1e-6, "Ti set correctly");
    CHECK(fabs(pid.td - 5.0) < 1e-6, "Td set correctly");
    CHECK(pid.mode == ECS700_PID_MODE_MANUAL, "default mode should be MANUAL");
    CHECK(pid.anti_windup_enabled == true, "anti-windup should default on");
}

static void test_pid_init_null(void)
{
    TEST("PID init null (no crash)");
    ecs700_pid_init(NULL, "test", 1.0, 10.0, 0.0, 0.1);
    PASS();
}

/* --- PID Execution --- */

static void test_pid_execute_disabled(void)
{
    TEST("PID execute when disabled");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "TIC101", 2.0, 20.0, 0.0, 0.2);
    pid.output = 45.0;
    pid.enabled = false;
    double op = ecs700_pid_execute(&pid, 1000000);
    CHECK(fabs(op - 45.0) < 1e-6, "disabled PID holds output");
}

static void test_pid_execute_proportional(void)
{
    TEST("PID execute P-only");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "PIC101", 2.0, 0.0, 0.0, 0.2);
    pid.setpoint = 50.0;
    pid.pv = 40.0;
    pid.mode = ECS700_PID_MODE_AUTO;
    pid.enabled = true;
    /* First execution initializes last_exec_time=0, uses config sample time */
    double op = ecs700_pid_execute(&pid, 200000);
    /* Error = 50 - 40 = 10, P = 2*10 = 20, direction = direct */
    CHECK(fabs(op - 20.0) < 0.5, "P-only output ≈ Kp * e");
}

static void test_pid_execute_reverse_acting(void)
{
    TEST("PID execute reverse acting");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "LIC102", 1.0, 0.0, 0.0, 0.2);
    pid.setpoint = 50.0;
    pid.pv = 40.0;
    pid.action = ECS700_PID_REVERSE_ACTING;
    pid.mode = ECS700_PID_MODE_AUTO;
    pid.enabled = true;
    /* Set output limits allowing negative output for reverse acting */
    ecs700_pid_set_output_limits(&pid, -100.0, 100.0);
    double op = ecs700_pid_execute(&pid, 200000);
    /* Error=10, P=1*10=10, reverse acting → -10 */
    CHECK(op < 0.0, "reverse acting output should be negative");
}

/* --- PID Output Limits --- */

static void test_pid_output_limits(void)
{
    TEST("PID output limits");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "FIC103", 1.0, 0.0, 0.0, 0.2);
    ecs700_pid_set_output_limits(&pid, 10.0, 90.0);
    CHECK(fabs(pid.output_lo - 10.0) < 1e-6, "low limit set");
    CHECK(fabs(pid.output_hi - 90.0) < 1e-6, "high limit set");
    /* Current output clamped */
    pid.output = 5.0;
    ecs700_pid_set_output_limits(&pid, 10.0, 90.0);
    CHECK(fabs(pid.output - 10.0) < 1e-6, "output clamped to low limit");
}

/* --- PID Mode Transition --- */

static void test_pid_mode_manual_to_auto(void)
{
    TEST("PID manual to auto transition");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "TIC104", 2.0, 60.0, 10.0, 0.2);
    pid.output = 50.0;
    pid.setpoint = 100.0;
    pid.pv = 90.0;
    pid.mode = ECS700_PID_MODE_MANUAL;
    ecs700_pid_mode_transition(&pid, ECS700_PID_MODE_AUTO);
    CHECK(pid.mode == ECS700_PID_MODE_AUTO, "mode switched to AUTO");
    CHECK(pid.enabled == true, "enabled after transition");
}

/* --- PID Alarm Detection --- */

static void test_pid_alarms_none(void)
{
    TEST("PID alarms none");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "AIC105", 1.0, 0.0, 0.0, 0.2);
    pid.pv = 50.0;
    pid.setpoint = 50.0;
    pid.pv_alarm_hi = 90.0;
    pid.pv_alarm_lo = 10.0;
    uint8_t alarms = ecs700_pid_check_alarms(&pid);
    CHECK(alarms == 0, "no alarms when PV in range");
}

static void test_pid_alarms_hi(void)
{
    TEST("PID PV high alarm");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "AIC106", 1.0, 0.0, 0.0, 0.2);
    pid.pv = 95.0;
    pid.pv_alarm_hi = 90.0;
    uint8_t alarms = ecs700_pid_check_alarms(&pid);
    CHECK(alarms & 0x02, "PV HI alarm set");
}

/* --- SFC Execution --- */

static void test_sfc_step_init(void)
{
    TEST("SFC step init");
    ecs700_sfc_step_t step;
    ecs700_sfc_step_init(&step, 1, "Init", true);
    CHECK(step.step_id == 1, "step ID set");
    CHECK(step.is_initial_step == true, "initial step flag");
    CHECK(step.active == true, "initial step starts active");
}

static void test_sfc_execute_basic(void)
{
    TEST("SFC execute basic");
    ecs700_sfc_step_t steps[3];
    ecs700_sfc_step_init(&steps[0], 1, "Fill", true);
    ecs700_sfc_step_init(&steps[1], 2, "React", false);
    ecs700_sfc_step_init(&steps[2], 3, "Drain", false);

    /* Setup transition: step 1 → step 2 */
    steps[0].num_transitions = 1;
    steps[0].next_step_ids[0] = 2;
    steps[0].min_dwell_time_ms = 0;

    uint16_t active;
    ecs700_sfc_execute(steps, 3, &active);
    CHECK(active > 0, "at least one step should be active");
}

/* --- Interlock Logic --- */

static void test_interlock_evaluate_gt(void)
{
    TEST("interlock evaluate greater than");
    ecs700_interlock_t il;
    memset(&il, 0, sizeof(il));
    il.trigger_condition = 0;  /* Greater Than */
    il.trigger_value = 100.0;
    bool tripped = ecs700_interlock_evaluate(&il, 105.0);
    CHECK(tripped, "105 > 100 should trigger");
    CHECK(il.active, "interlock should be active");
}

/* --- Cascade Control --- */

static void test_cascade_init(void)
{
    TEST("cascade init");
    ecs700_cascade_pair_t cascade;
    ecs700_cascade_init(&cascade, "TC201", "FC202",
                         2.0, 60.0, 10.0,  /* Primary: slow temperature */
                         0.5, 5.0,  0.0,   /* Secondary: fast flow */
                         0.2);
    CHECK(cascade.primary.enabled == true, "primary enabled");
    CHECK(cascade.secondary.enabled == true, "secondary enabled");
}

static void test_cascade_execute(void)
{
    TEST("cascade execute");
    ecs700_cascade_pair_t cascade;
    ecs700_cascade_init(&cascade, "TC301", "FC302",
                         2.0, 60.0, 10.0,
                         0.5, 5.0,  0.0,
                         0.2);
    cascade.cascade_enabled = true;
    /* Primary: SP=150, PV=140 → output ≈ 20 + integral */
    cascade.primary.setpoint = 150.0;
    cascade.primary.pv = 140.0;
    /* Secondary uses primary's output as SP */
    cascade.secondary.pv = 25.0;
    double op = ecs700_cascade_execute(&cascade, 140.0, 25.0, 1000000);
    (void)op;  /* Just verify no crash */
    PASS();
}

/* --- Auto-tuning --- */

static void test_autotune_basic(void)
{
    TEST("auto-tune basic");
    ecs700_pid_block_t pid;
    ecs700_pid_init(&pid, "TIC401", 1.0, 10.0, 0.0, 0.2);
    int ret = ecs700_pid_autotune_relay(&pid, 20.0, 1.0, 5);
    CHECK(ret == 0, "auto-tune should succeed");
    CHECK(pid.kp > 0.0, "auto-tuned Kp should be positive");
}

/* --- Runner --- */

int main(void)
{
    printf("\n=== ECS-700 Control Station Tests ===\n\n");

    test_pid_init_basic();
    test_pid_init_null();
    test_pid_execute_disabled();
    test_pid_execute_proportional();
    test_pid_execute_reverse_acting();
    test_pid_output_limits();
    test_pid_mode_manual_to_auto();
    test_pid_alarms_none();
    test_pid_alarms_hi();
    test_sfc_step_init();
    test_sfc_execute_basic();
    test_interlock_evaluate_gt();
    test_cascade_init();
    test_cascade_execute();
    test_autotune_basic();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
