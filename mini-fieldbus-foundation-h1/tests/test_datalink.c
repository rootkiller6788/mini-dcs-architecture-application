/**
 * test_datalink.c ? Tests for ff_h1_datalink.h
 */

#include "ff_h1_datalink.h"
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

static void test_las_init(void) {
    T("LAS init");
    ff_las_context_t ctx;
    ff_las_init(&ctx, 0x10);
    AE(ctx.state, FF_LAS_STATE_STARTUP, "LAS should be in STARTUP");
    AE(ctx.las_address, 0x10, "wrong LAS address");
    AE(ctx.cd_schedule.count, 0, "schedule should be empty");
    P();
}

static void test_las_cd_add(void) {
    T("LAS CD add");
    ff_las_context_t ctx;
    ff_las_init(&ctx, 0x10);
    ctx.state = FF_LAS_STATE_ACTIVE;

    ff_cd_entry_t e1 = {0x11, 0, 1000, 500};
    ff_cd_entry_t e2 = {0x12, 1, 2000, 500};
    AE(ff_las_cd_add(&ctx, &e1), 0, "add e1 should succeed");
    AE(ff_las_cd_add(&ctx, &e2), 0, "add e2 should succeed");
    AE(ctx.cd_schedule.count, 2, "should have 2 entries");
    P();
}

static void test_las_cd_add_non_monotonic(void) {
    T("LAS CD add non-monotonic rejected");
    ff_las_context_t ctx;
    ff_las_init(&ctx, 0x10);
    ctx.state = FF_LAS_STATE_ACTIVE;

    ff_cd_entry_t e1 = {0x11, 0, 2000, 500};
    ff_cd_entry_t e2 = {0x12, 1, 1000, 500}; /* offset earlier than e1 */
    AE(ff_las_cd_add(&ctx, &e1), 0, "add e1 should succeed");
    AE(ff_las_cd_add(&ctx, &e2), -1, "add e2 should fail (non-monotonic)");
    P();
}

static void test_las_macrocycle(void) {
    T("LAS run macrocycle");
    ff_las_context_t ctx;
    ff_las_init(&ctx, 0x10);
    ctx.state = FF_LAS_STATE_ACTIVE;
    ctx.cd_schedule.macrocycle_us = 100000;

    ff_cd_entry_t e1 = {0x11, 0, 1000, 500};
    ff_cd_entry_t e2 = {0x12, 1, 5000, 500};
    ff_las_cd_add(&ctx, &e1);
    ff_las_cd_add(&ctx, &e2);

    int executed = ff_las_run_macrocycle(&ctx);
    AE(executed, 2, "should execute 2 CD entries");
    AE(ctx.macrocycle_count, 1, "macrocycle count should be 1");
    P();
}

static void test_las_cd_utilization(void) {
    T("LAS CD utilization");
    ff_las_context_t ctx;
    ff_las_init(&ctx, 0x10);
    ctx.state = FF_LAS_STATE_ACTIVE;
    ctx.cd_schedule.macrocycle_us = 100000;

    ff_cd_entry_t e1 = {0x11, 0, 1000, 500};
    ff_las_cd_add(&ctx, &e1);

    double util = ff_las_cd_utilization(&ctx);
    AT(util >= 0.0 && util <= 1.0, "utilization should be between 0 and 1");
    P();
}

/* --- Live List --- */
static void test_live_list_add_find(void) {
    T("Live list add and find");
    ff_live_list_t list;
    ff_live_list_init(&list);
    AE(ff_live_list_add(&list, 0x10, 1), 0, "add should succeed");
    AT(ff_live_list_find(&list, 0x10), "should find address 0x10");
    AT(!ff_live_list_find(&list, 0x20), "should not find unadded address");
    AE(ff_live_list_count_operational(&list), 1, "should count 1 operational");
    P();
}

static void test_live_list_remove(void) {
    T("Live list remove");
    ff_live_list_t list;
    ff_live_list_init(&list);
    ff_live_list_add(&list, 0x10, 1);
    ff_live_list_add(&list, 0x11, 2);
    AE(ff_live_list_remove(&list, 0x10), 0, "remove should succeed");
    AE(list.count, 1, "count should be 1 after remove");
    AT(!ff_live_list_find(&list, 0x10), "removed address should not be found");
    P();
}

static void test_live_list_next_token(void) {
    T("Live list round-robin token");
    ff_live_list_t list;
    ff_live_list_init(&list);
    ff_live_list_add(&list, 0x10, 1);
    ff_live_list_add(&list, 0x11, 2);
    ff_live_list_add(&list, 0x12, 1);

    size_t idx = 0;
    uint8_t a1 = ff_live_list_next_token(&list, &idx);
    uint8_t a2 = ff_live_list_next_token(&list, &idx);
    uint8_t a3 = ff_live_list_next_token(&list, &idx);
    uint8_t a4 = ff_live_list_next_token(&list, &idx); /* wraps around */

    AT(a1 != a2 && a2 != a3, "three different addresses");
    AE(a1, a4, "should wrap around to first");
    P();
}

/* --- LM Election --- */
static void test_lm_holdoff(void) {
    T("LM holdoff time");
    /* Priority 0x01: holdoff = 0 * 4 = 0 ms */
    AE(ff_lm_holdoff_ms(0x01), 0, "highest priority has 0 holdoff");
    /* Priority 0x80: holdoff = 127 * 4 = 508 ms */
    AE(ff_lm_holdoff_ms(0x80), 508, "default priority holdoff");
    /* Priority 0xFF: holdoff = 254 * 4 = 1016 ms */
    AE(ff_lm_holdoff_ms(0xFF), 1016, "lowest priority holdoff");
    P();
}

static void test_lm_election(void) {
    T("LM election comparison");
    AE(ff_lm_election_compare(0x01, 0x02), 1, "lower value wins");
    AE(ff_lm_election_compare(0x80, 0x10), -1, "higher value loses");
    AE(ff_lm_election_compare(0x50, 0x50), 0, "equal priority = tie");
    P();
}

int main(void) {
    printf("=== test_datalink ===\n");
    test_las_init();
    test_las_cd_add();
    test_las_cd_add_non_monotonic();
    test_las_macrocycle();
    test_las_cd_utilization();
    test_live_list_add_find();
    test_live_list_remove();
    test_live_list_next_token();
    test_lm_holdoff();
    test_lm_election();
    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}