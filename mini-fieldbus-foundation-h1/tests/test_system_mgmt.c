/**
 * test_system_mgmt.c ? Tests for ff_h1_system_mgmt.h
 */

#include "ff_h1_system_mgmt.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int passed = 0, failed = 0;
#define T(name) do { printf("  TEST: %s ... ", name); } while(0)
#define P() do { printf("PASS\n"); passed++; } while(0)
#define F(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define AE(a,b,m) do { if((a)!=(b)){F(m);return;} } while(0)
#define AT(c,m) do { if(!(c)){F(m);return;} } while(0)

static void test_sm_init_state(void) {
    T("SM init state");
    uint8_t dev_id[32] = {0};
    for (int i = 0; i < 32; i++) dev_id[i] = (uint8_t)i;
    ff_sm_agent_t sm;
    ff_sm_init(&sm, dev_id);
    AE(sm.state, FF_SM_STATE_UNINITIALIZED, "should start uninitialized");
    AT(memcmp(sm.device_id, dev_id, 32) == 0, "device ID copied");
    AE(sm.dl_address, 0xFC, "default temporary address");
    P();
}

static void test_sm_state_transitions(void) {
    T("SM state transitions");
    uint8_t dev_id[32] = {0};
    ff_sm_agent_t sm;
    ff_sm_init(&sm, dev_id);

    ff_sm_start_initialization(&sm);
    AE(sm.state, FF_SM_STATE_INITIALIZING, "? INITIALIZING");

    AE(ff_sm_set_operational(&sm, "TAG-001", 0x20), 0, "set operational");
    AE(sm.state, FF_SM_STATE_OPERATIONAL, "? OPERATIONAL");
    AT(strcmp((char*)sm.pd_tag, "TAG-001") == 0, "tag set");
    AE(sm.permanent_addr, 0x20, "permanent addr set");
    P();
}

static void test_sm_set_address_protocol(void) {
    T("SM Set Address protocol");
    uint8_t dev_id[32] = {0};
    for (int i = 0; i < 32; i++) dev_id[i] = (uint8_t)(i * 2);
    ff_sm_agent_t sm;
    ff_sm_init(&sm, dev_id);
    ff_sm_start_initialization(&sm);

    /* Correct device ID ? should accept */
    AE(ff_sm_process_set_address(&sm, dev_id, 0x30), 0, "accept valid Set Address");
    AE(sm.state, FF_SM_STATE_OPERATIONAL, "? OPERATIONAL after Set Address");

    /* Wrong state (already operational) ? should reject */
    uint8_t wrong_id[32] = {0xFF};
    ff_sm_agent_t sm2;
    ff_sm_init(&sm2, dev_id);
    AE(ff_sm_process_set_address(&sm2, wrong_id, 0x40), -1, "reject wrong device ID");
    P();
}

static void test_sm_find_tag_match(void) {
    T("SM Find Tag match");
    uint8_t dev_id[32] = {0};
    ff_sm_agent_t sm;
    ff_sm_init(&sm, dev_id);
    ff_sm_set_operational(&sm, "PT-101A", 0x15);

    AT(ff_sm_find_tag_match(&sm, "PT-101A"), "exact match");
    AT(!ff_sm_find_tag_match(&sm, "PT-101B"), "mismatch");
    AT(!ff_sm_find_tag_match(&sm, "pt-101a"), "case sensitive");
    P();
}

static void test_sm_time_distribution(void) {
    T("SM Time Distribution processing");
    uint8_t dev_id[32] = {0};
    ff_sm_agent_t sm;
    ff_sm_init(&sm, dev_id);

    ff_td_message_t td = {1000, 0, 1, 100};
    ff_time_sync_quality_t q = ff_sm_process_td(&sm, &td, 50);
    AE(q, FF_TIME_SYNC_COARSE, "first TD ? COARSE");

    /* Send 2 more TDs for FINE */
    td.td_sequence = 2;
    ff_sm_process_td(&sm, &td, 50);
    td.td_sequence = 3;
    q = ff_sm_process_td(&sm, &td, 50);
    AE(q, FF_TIME_SYNC_FINE, "3 TDs ? FINE");
    P();
}

static void test_sm_time_format(void) {
    T("SM time format");
    uint8_t dev_id[32] = {0};
    ff_sm_agent_t sm;
    ff_sm_init(&sm, dev_id);
    sm.current_time = 3723; /* 1h 2m 3s */
    char buf[32];
    ff_sm_time_format(&sm, buf, 32);
    AT(strlen(buf) > 0, "time formatted");
    P();
}

static void test_nm_statistics(void) {
    T("NM statistics");
    ff_nm_statistics_t stats;
    ff_nm_stats_init(&stats);
    stats.dl_tx_frames = 1000;
    stats.dl_rx_frames = 1000;
    stats.dl_tx_errors = 5;
    stats.dl_rx_errors = 3;

    double err = ff_nm_error_rate(&stats);
    AT(err > 0.003 && err < 0.005, "error rate ~0.004");

    double eff = ff_nm_efficiency(&stats);
    AT(eff > 0.99, "efficiency > 99%");
    P();
}

static void test_smib_read(void) {
    T("SMIB read");
    uint8_t dev_id[32] = {0};
    for (int i = 0; i < 32; i++) dev_id[i] = (uint8_t)i;
    ff_sm_agent_t sm;
    ff_sm_init(&sm, dev_id);
    ff_sm_set_operational(&sm, "DEV01", 0x25);

    uint8_t buf[64];
    size_t sz = sizeof(buf);
    AE(ff_smib_read(&sm, FF_SMIB_DEVICE_STATE, buf, &sz), 0, "read state");
    AE(buf[0], FF_SM_STATE_OPERATIONAL, "state is operational");
    P();
}

int main(void) {
    printf("=== test_system_mgmt ===\n");
    test_sm_init_state();
    test_sm_state_transitions();
    test_sm_set_address_protocol();
    test_sm_find_tag_match();
    test_sm_time_distribution();
    test_sm_time_format();
    test_nm_statistics();
    test_smib_read();
    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}