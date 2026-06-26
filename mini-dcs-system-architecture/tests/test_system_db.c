/**
 * @file test_system_db.c
 * @brief Tests for DCS system database (L3-L6).
 */
#include "dcs_system_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s ... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

int main(void)
{
    printf("\n=== DCS System Database Tests ===\n\n");

    /* L3: Database Init */
    dcs_system_database_t db;
    int init_ok = dcs_db_init(&db, 100, 10, 200, 50);
    TEST("DB init - allocates memory");
    CHECK(init_ok == 1 && db.tags != NULL &&
          db.controllers != NULL && db.io_points != NULL &&
          db.control_modules != NULL,
          "all arrays should be allocated");

    TEST("DB init - scan phases configured");
    CHECK(db.num_scan_phases == 5, "should have 5 scan phases");

    /* L3: Add Tag */
    dcs_tag_t tag;
    memset(&tag, 0, sizeof(tag));
    tag.tag_id = 1;
    snprintf(tag.tag_name, 40, "10-FIC-101");
    snprintf(tag.description, 64, "Reactor feed flow control");
    snprintf(tag.area_code, 8, "10");
    tag.eu_min = 0.0;
    tag.eu_max = 500.0;
    tag.level = DCS_LEVEL_1_CONTROL;

    TEST("DB add tag - success");
    CHECK(dcs_db_add_tag(&db, &tag) == 1, "should add tag successfully");

    TEST("DB add tag - duplicate ID rejected");
    CHECK(dcs_db_add_tag(&db, &tag) == 0, "should reject duplicate tag_id");

    /* Add more tags */
    tag.tag_id = 2;
    snprintf(tag.tag_name, 40, "10-TIC-102");
    dcs_db_add_tag(&db, &tag);

    tag.tag_id = 3;
    snprintf(tag.tag_name, 40, "10-PIC-103");
    dcs_db_add_tag(&db, &tag);

    /* L3: Add Controller */
    dcs_controller_config_t ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.node_id = 100;
    ctrl.node_type = DCS_NODE_CONTROLLER;
    ctrl.state = DCS_NODE_STATE_ACTIVE;
    ctrl.scan_period_ms = 250.0;
    ctrl.max_loops = 200;
    ctrl.max_tags = 1000;

    TEST("DB add controller - success");
    CHECK(dcs_db_add_controller(&db, &ctrl) == 1,
          "should add controller successfully");

    /* L3: Add I/O Point */
    dcs_io_point_t io;
    memset(&io, 0, sizeof(io));
    io.io_point_id = 1001;
    io.signal_type = DCS_SIG_AI_4_20MA;
    io.raw_min = 0.0;
    io.raw_max = 32767.0;
    io.eu_min = 0.0;
    io.eu_max = 500.0;
    io.enabled = 1;

    TEST("DB add I/O point - success");
    CHECK(dcs_db_add_io_point(&db, &io) == 1,
          "should add I/O point successfully");

    /* L3: Add Control Module */
    dcs_control_module_t mod;
    memset(&mod, 0, sizeof(mod));
    mod.module_id = 2001;
    snprintf(mod.module_name, 32, "FIC-101-PID");
    snprintf(mod.module_type, 16, "PID");
    mod.controller_id = 100;
    mod.num_input_tags = 1;
    mod.input_tag_ids[0] = 1;  /* 10-FIC-101 */
    mod.num_output_tags = 1;
    mod.output_tag_ids[0] = 3; /* 10-PIC-103 */
    mod.scan_phase = 2;
    mod.execution_time_us = 150.0;
    mod.enabled = 1;

    TEST("DB add control module - success");
    CHECK(dcs_db_add_control_module(&db, &mod) == 1,
          "should add control module");

    /* Add more modules for testing */
    mod.module_id = 2002;
    snprintf(mod.module_name, 32, "TIC-102-PID");
    mod.input_tag_ids[0] = 2;
    mod.output_tag_ids[0] = 3;
    mod.execution_time_us = 120.0;
    dcs_db_add_control_module(&db, &mod);

    /* L3: Database Validation */
    uint32_t errors = 999;
    int valid = dcs_db_validate(&db, &errors);
    TEST("DB validate - valid config passes");
    CHECK(valid == 1 && errors == 0, "valid database should pass validation");

    /* L3: Find Tag by Name */
    TEST("DB find tag - existing tag");
    CHECK(dcs_db_find_tag_by_name(&db, "10-FIC-101") == 1,
          "should find tag by name");

    TEST("DB find tag - non-existing tag");
    CHECK(dcs_db_find_tag_by_name(&db, "NONEXISTENT") == UINT32_MAX,
          "should return UINT32_MAX for non-existing tag");

    /* L3: Controller Load Analysis */
    double total_time_us = 0.0;
    double load_pct = 0.0;
    int load_ok = dcs_db_analyze_controller_load(&db, 100,
                                                   &total_time_us, &load_pct);
    TEST("DB controller load - within acceptable range");
    CHECK(load_ok == 1 && load_pct < 100.0,
          "controller loading should be acceptable");

    /* L6: Scan Phase Optimization */
    TEST("Scan phase optimization - runs successfully");
    CHECK(dcs_db_optimize_scan_phases(&db, 100) == 1,
          "scan phase optimization should succeed");

    /* Verify phases have modules after optimization */
    uint32_t total_modules_in_phases = 0;
    for (uint32_t i = 0; i < db.num_scan_phases; i++) {
        total_modules_in_phases += db.scan_phases[i].num_modules;
    }
    TEST("Scan phase optimization - modules assigned");
    CHECK(total_modules_in_phases >= db.num_control_modules,
          "all modules should be assigned to phases");

    /* L6: Capacity Metrics */
    double tag_density = 0.0;
    double growth_margin = 0.0;
    int cap_ok = dcs_db_calculate_capacity_metrics(&db, &tag_density,
                                                    &growth_margin);
    TEST("Capacity metrics - calculation succeeds");
    CHECK(cap_ok == 1, "capacity metrics should compute");
    TEST("Capacity metrics - growth margin positive");
    CHECK(growth_margin > 0.0, "growth margin should be positive");

    /* Cleanup */
    dcs_db_free(&db);
    TEST("DB cleanup - frees memory");
    CHECK(db.tags == NULL && db.controllers == NULL
          && db.io_points == NULL && db.control_modules == NULL,
          "all arrays should be NULL after free");

    /* Null safety tests */
    TEST("DB init - NULL pointer");
    CHECK(dcs_db_init(NULL, 100, 10, 200, 50) == 0,
          "NULL db should return 0");

    TEST("DB add tag - NULL db");
    CHECK(dcs_db_add_tag(NULL, &tag) == 0, "NULL db should return 0");

    TEST("DB validate - NULL db");
    uint32_t err;
    CHECK(dcs_db_validate(NULL, &err) == 0, "NULL db should return 0");

    TEST("DB find tag - NULL db");
    CHECK(dcs_db_find_tag_by_name(NULL, "X") == UINT32_MAX,
          "NULL db should return UINT32_MAX");

    /* Summary */
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
