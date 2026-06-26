/**
 * test_application.c ? Tests for ff_h1_application.h
 */

#include "ff_h1_application.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int passed = 0, failed = 0;
#define T(name) do { printf("  TEST: %s ... ", name); } while(0)
#define P() do { printf("PASS\n"); passed++; } while(0)
#define F(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define AE(a,b,m) do { if((a)!=(b)){F(m);return;} } while(0)
#define AT(c,m) do { if(!(c)){F(m);return;} } while(0)

static void test_fb_type_name(void) {
    T("FB type names");
    AT(strcmp(ff_fb_type_name(FF_FB_AI), "AI") == 0, "AI name");
    AT(strcmp(ff_fb_type_name(FF_FB_PID), "PID") == 0, "PID name");
    AT(strcmp(ff_fb_type_name(FF_FB_RA), "RA") == 0, "RA name");
    AT(strcmp(ff_fb_type_name((ff_fb_type_t)99), "UNKNOWN") == 0, "unknown");
    P();
}

static void test_fb_param_counts(void) {
    T("FB param counts");
    AT(ff_fb_type_param_count(FF_FB_PID) > 20, "PID has many params");
    AT(ff_fb_type_param_count(FF_FB_AI) > 10, "AI has reasonable params");
    AE(ff_fb_type_param_count((ff_fb_type_t)99), 0, "invalid type = 0 params");
    P();
}

/* --- Mode Transitions --- */
static void test_mode_any_to_oos(void) {
    T("Mode: any ? OOS allowed");
    AT(ff_mode_transition_allowed(FF_MODE_AUTO, FF_MODE_OOS, 0xFF), "AUTO?OOS");
    AT(ff_mode_transition_allowed(FF_MODE_CAS, FF_MODE_OOS, 0xFF), "CAS?OOS");
    AT(ff_mode_transition_allowed(FF_MODE_IMAN, FF_MODE_OOS, 0xFF), "IMAN?OOS");
    P();
}

static void test_mode_oos_to_permitted(void) {
    T("Mode: OOS ? permitted");
    AT(ff_mode_transition_allowed(FF_MODE_OOS, FF_MODE_AUTO, FF_MODE_AUTO), "OOS?AUTO");
    AT(!ff_mode_transition_allowed(FF_MODE_OOS, FF_MODE_CAS,
        FF_MODE_AUTO | FF_MODE_MAN), "OOS?CAS not permitted");
    P();
}

static void test_mode_auto_man_bidirectional(void) {
    T("Mode: AUTO?MAN bidirectional");
    AT(ff_mode_transition_allowed(FF_MODE_AUTO, FF_MODE_MAN, FF_MODE_AUTO | FF_MODE_MAN),
       "AUTO?MAN");
    AT(ff_mode_transition_allowed(FF_MODE_MAN, FF_MODE_AUTO, FF_MODE_AUTO | FF_MODE_MAN),
       "MAN?AUTO");
    P();
}

static void test_mode_man_to_cas_prohibited(void) {
    T("Mode: MAN?CAS not directly allowed");
    AT(!ff_mode_transition_allowed(FF_MODE_MAN, FF_MODE_CAS, 0xFF),
       "MAN?CAS should be blocked");
    P();
}

static void test_mode_determine_actual_fault(void) {
    T("Mode: determine actual ? fault active");
    ff_block_mode_t actual = ff_mode_determine_actual(FF_MODE_AUTO,
        FF_MODE_AUTO | FF_MODE_LO | FF_MODE_MAN, 1, 1);
    AE(actual, FF_MODE_LO, "fault should force Local Override");
    P();
}

static void test_mode_determine_actual_cas_nready(void) {
    T("Mode: determine actual ? CAS not ready");
    ff_block_mode_t actual = ff_mode_determine_actual(FF_MODE_CAS,
        FF_MODE_CAS | FF_MODE_AUTO | FF_MODE_MAN, 0, 0);
    AE(actual, FF_MODE_AUTO, "CAS not ready, shed to AUTO");
    P();
}

/* --- Link Validation --- */
static void test_link_validate_self_loop(void) {
    T("Link validation: self-loop rejected");
    ff_link_object_t link = {0, 1, 1, 0, 1, 1, 0, 1000};
    AT(!ff_link_validate(&link), "self-loop should be invalid");
    P();
}

static void test_link_validate_valid(void) {
    T("Link validation: valid link");
    ff_link_object_t link = {0, 1, 5, 0, 3, 6, 0, 1000};
    AT(ff_link_validate(&link), "valid link should pass");
    P();
}

/* --- FB Execute --- */
static void test_fb_execute_oos(void) {
    T("FB execute: OOS block rejected");
    ff_function_block_t block;
    memset(&block, 0, sizeof(block));
    block.type = FF_FB_PID;
    block.mode.actual = FF_MODE_OOS;

    ff_fbap_device_t device;
    memset(&device, 0, sizeof(device));

    AE(ff_fb_execute(&block, &device), -1, "OOS block should not execute");
    P();
}

int main(void) {
    printf("=== test_application ===\n");
    test_fb_type_name();
    test_fb_param_counts();
    test_mode_any_to_oos();
    test_mode_oos_to_permitted();
    test_mode_auto_man_bidirectional();
    test_mode_man_to_cas_prohibited();
    test_mode_determine_actual_fault();
    test_mode_determine_actual_cas_nready();
    test_link_validate_self_loop();
    test_link_validate_valid();
    test_fb_execute_oos();
    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}