/**
 * @file dcs_system_db.c
 * @brief DCS system configuration database implementation.
 *
 * Covers tag database, controller and I/O configuration,
 * control module management, database validation,
 * and scan phase optimization.
 *
 * Knowledge Levels: L3, L6
 */

#include "dcs_system_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * L3: Database Initialization and Lifecycle
 *===========================================================================*/

int dcs_db_init(dcs_system_database_t *db,
                 uint32_t max_tags,
                 uint32_t max_controllers,
                 uint32_t max_io,
                 uint32_t max_modules)
{
    if (db == NULL) return 0;

    memset(db, 0, sizeof(dcs_system_database_t));

    db->max_tags        = max_tags;
    db->max_controllers = max_controllers;
    db->max_io_points   = max_io;
    db->max_control_modules = max_modules;

    /* Allocate arrays */
    if (max_tags > 0) {
        db->tags = (dcs_tag_t *)calloc(max_tags, sizeof(dcs_tag_t));
        if (db->tags == NULL) {
            dcs_db_free(db);
            return 0;
        }
    }

    if (max_controllers > 0) {
        db->controllers = (dcs_controller_config_t *)calloc(max_controllers,
                                        sizeof(dcs_controller_config_t));
        if (db->controllers == NULL) {
            dcs_db_free(db);
            return 0;
        }
    }

    if (max_io > 0) {
        db->io_points = (dcs_io_point_t *)calloc(max_io, sizeof(dcs_io_point_t));
        if (db->io_points == NULL) {
            dcs_db_free(db);
            return 0;
        }
    }

    if (max_modules > 0) {
        db->control_modules = (dcs_control_module_t *)calloc(max_modules,
                                        sizeof(dcs_control_module_t));
        if (db->control_modules == NULL) {
            dcs_db_free(db);
            return 0;
        }
    }

    /* Initialize scan phases with defaults */
    db->num_scan_phases = 5;
    const char *phase_names[] = {
        "Critical/Safety", "Regulatory PID", "Supervisory/Cascade",
        "Calculations/Logic", "Communication"
    };
    const double phase_budgets[] = {0.10, 0.30, 0.20, 0.20, 0.20};

    for (uint32_t i = 0; i < db->num_scan_phases; i++) {
        db->scan_phases[i].phase_id = i + 1;
        snprintf(db->scan_phases[i].phase_name, 32, "%s", phase_names[i]);
        db->scan_phases[i].allocated_time_us = phase_budgets[i] * 250000.0;
        db->scan_phases[i].actual_time_us = 0.0;
        db->scan_phases[i].num_modules = 0;
        db->scan_phases[i].is_complete = 1;
    }

    db->min_scan_period_ms = 50.0;
    db->max_scan_period_ms = 1000.0;
    db->is_configured = 1;
    db->is_validated = 1;

    return 1;
}

void dcs_db_free(dcs_system_database_t *db)
{
    if (db == NULL) return;

    if (db->tags != NULL) {
        free(db->tags);
        db->tags = NULL;
    }

    if (db->controllers != NULL) {
        free(db->controllers);
        db->controllers = NULL;
    }

    if (db->io_points != NULL) {
        free(db->io_points);
        db->io_points = NULL;
    }

    if (db->control_modules != NULL) {
        free(db->control_modules);
        db->control_modules = NULL;
    }

    db->num_tags = 0;
    db->num_controllers = 0;
    db->num_io_points = 0;
    db->num_control_modules = 0;
    db->is_configured = 0;
}

/*===========================================================================
 * L3: Add Operations
 *===========================================================================*/

int dcs_db_add_tag(dcs_system_database_t *db, const dcs_tag_t *tag)
{
    if (db == NULL || tag == NULL) return 0;
    if (db->tags == NULL) return 0;
    if (db->num_tags >= db->max_tags) return 0;

    /* Check for duplicate tag_id or tag_name */
    for (uint32_t i = 0; i < db->num_tags; i++) {
        if (db->tags[i].tag_id == tag->tag_id) return 0;
        if (strcmp(db->tags[i].tag_name, tag->tag_name) == 0) return 0;
    }

    /* Copy tag into database */
    memcpy(&db->tags[db->num_tags], tag, sizeof(dcs_tag_t));
    db->num_tags++;

    return 1;
}

int dcs_db_add_controller(dcs_system_database_t *db,
                           const dcs_controller_config_t *controller)
{
    if (db == NULL || controller == NULL) return 0;
    if (db->controllers == NULL) return 0;
    if (db->num_controllers >= db->max_controllers) return 0;

    /* Check for duplicate node_id */
    for (uint32_t i = 0; i < db->num_controllers; i++) {
        if (db->controllers[i].node_id == controller->node_id) return 0;
    }

    memcpy(&db->controllers[db->num_controllers], controller,
           sizeof(dcs_controller_config_t));
    db->num_controllers++;

    return 1;
}

int dcs_db_add_io_point(dcs_system_database_t *db,
                         const dcs_io_point_t *io)
{
    if (db == NULL || io == NULL) return 0;
    if (db->io_points == NULL) return 0;
    if (db->num_io_points >= db->max_io_points) return 0;

    /* Check for duplicate io_point_id */
    for (uint32_t i = 0; i < db->num_io_points; i++) {
        if (db->io_points[i].io_point_id == io->io_point_id) return 0;
    }

    memcpy(&db->io_points[db->num_io_points], io, sizeof(dcs_io_point_t));
    db->num_io_points++;

    return 1;
}

int dcs_db_add_control_module(dcs_system_database_t *db,
                               const dcs_control_module_t *module)
{
    if (db == NULL || module == NULL) return 0;
    if (db->control_modules == NULL) return 0;
    if (db->num_control_modules >= db->max_control_modules) return 0;

    /* Check for duplicate module_id */
    for (uint32_t i = 0; i < db->num_control_modules; i++) {
        if (db->control_modules[i].module_id == module->module_id) return 0;
    }

    memcpy(&db->control_modules[db->num_control_modules], module,
           sizeof(dcs_control_module_t));
    db->num_control_modules++;

    return 1;
}

/*===========================================================================
 * L3: Database Validation
 *===========================================================================*/

int dcs_db_validate(const dcs_system_database_t *db,
                     uint32_t *num_errors)
{
    if (db == NULL) {
        if (num_errors != NULL) *num_errors = 1;
        return 0;
    }

    uint32_t errors = 0;

    /* Validation 1: Tag name uniqueness (ISA-5.1 conformance) */
    for (uint32_t i = 0; i < db->num_tags; i++) {
        for (uint32_t j = i + 1; j < db->num_tags; j++) {
            if (strcmp(db->tags[i].tag_name, db->tags[j].tag_name) == 0) {
                errors++;
            }
        }
    }

    /* Validation 2: Controller ID uniqueness */
    for (uint32_t i = 0; i < db->num_controllers; i++) {
        for (uint32_t j = i + 1; j < db->num_controllers; j++) {
            if (db->controllers[i].node_id == db->controllers[j].node_id) {
                errors++;
            }
        }
    }

    /* Validation 3: I/O point ID uniqueness */
    for (uint32_t i = 0; i < db->num_io_points; i++) {
        for (uint32_t j = i + 1; j < db->num_io_points; j++) {
            if (db->io_points[i].io_point_id == db->io_points[j].io_point_id) {
                errors++;
            }
        }
    }

    /* Validation 4: Control module input/output tags exist */
    for (uint32_t i = 0; i < db->num_control_modules; i++) {
        const dcs_control_module_t *mod = &db->control_modules[i];
        for (uint32_t j = 0; j < mod->num_input_tags && j < 8; j++) {
            uint32_t found = 0;
            for (uint32_t k = 0; k < db->num_tags; k++) {
                if (db->tags[k].tag_id == mod->input_tag_ids[j]) {
                    found = 1;
                    break;
                }
            }
            if (mod->input_tag_ids[j] != 0 && !found) {
                errors++;
            }
        }
        for (uint32_t j = 0; j < mod->num_output_tags && j < 4; j++) {
            uint32_t found = 0;
            for (uint32_t k = 0; k < db->num_tags; k++) {
                if (db->tags[k].tag_id == mod->output_tag_ids[j]) {
                    found = 1;
                    break;
                }
            }
            if (mod->output_tag_ids[j] != 0 && !found) {
                errors++;
            }
        }
    }

    /* Validation 5: Controller scan period within range */
    for (uint32_t i = 0; i < db->num_controllers; i++) {
        if (db->controllers[i].scan_period_ms < db->min_scan_period_ms
            || db->controllers[i].scan_period_ms > db->max_scan_period_ms) {
            errors++;
        }
    }

    /* Validation 6: Engineering unit range sanity */
    for (uint32_t i = 0; i < db->num_tags; i++) {
        if (db->tags[i].eu_min >= db->tags[i].eu_max) {
            errors++;
        }
    }

    if (num_errors != NULL) *num_errors = errors;
    return (errors == 0) ? 1 : 0;
}

/*===========================================================================
 * L3: Tag Lookup
 *===========================================================================*/

uint32_t dcs_db_find_tag_by_name(const dcs_system_database_t *db,
                                  const char *tag_name)
{
    if (db == NULL || tag_name == NULL || db->tags == NULL) {
        return UINT32_MAX;
    }

    for (uint32_t i = 0; i < db->num_tags; i++) {
        if (strcmp(db->tags[i].tag_name, tag_name) == 0) {
            return db->tags[i].tag_id;
        }
    }

    return UINT32_MAX;
}

/*===========================================================================
 * L3: Controller Load Analysis
 *===========================================================================*/

int dcs_db_analyze_controller_load(const dcs_system_database_t *db,
                                    uint32_t controller_id,
                                    double *total_time_us,
                                    double *load_pct)
{
    if (total_time_us != NULL) *total_time_us = 0.0;
    if (load_pct != NULL) *load_pct = 0.0;

    if (db == NULL) return 0;

    /* Find controller */
    const dcs_controller_config_t *ctrl = NULL;
    for (uint32_t i = 0; i < db->num_controllers; i++) {
        if (db->controllers[i].node_id == controller_id) {
            ctrl = &db->controllers[i];
            break;
        }
    }
    if (ctrl == NULL) return 0;

    /* Sum execution times of all modules assigned to this controller */
    double total_exec_us = 0.0;
    uint32_t module_count = 0;

    for (uint32_t i = 0; i < db->num_control_modules; i++) {
        if (db->control_modules[i].controller_id == controller_id
            && db->control_modules[i].enabled) {
            total_exec_us += db->control_modules[i].execution_time_us;
            module_count++;
        }
    }

    /* Compute loading percentage */
    double scan_period_us = ctrl->scan_period_ms * 1000.0;
    double loading = 0.0;

    if (scan_period_us > 0.0) {
        /*
         * Add system overhead: 10% for OS, 5% for communication.
         * Total load = (module_exec + overhead) / scan_period
         */
        double overhead_us = total_exec_us * 0.15;
        loading = (total_exec_us + overhead_us) / scan_period_us * 100.0;
    }

    if (loading > 100.0) loading = 100.0;
    if (loading < 0.0) loading = 0.0;

    if (total_time_us != NULL) *total_time_us = total_exec_us;
    if (load_pct != NULL) *load_pct = loading;

    /* Loading is acceptable if ≤ 70% */
    return (loading <= 70.0) ? 1 : 0;
}

/*===========================================================================
 * L6: Scan Phase Optimization
 *===========================================================================*/

int dcs_db_optimize_scan_phases(dcs_system_database_t *db,
                                 uint32_t controller_id)
{
    if (db == NULL) return 0;

    /* Find controller scan period */
    const dcs_controller_config_t *ctrl = NULL;
    for (uint32_t i = 0; i < db->num_controllers; i++) {
        if (db->controllers[i].node_id == controller_id) {
            ctrl = &db->controllers[i];
            break;
        }
    }
    if (ctrl == NULL) return 0;

    double total_scan_us = ctrl->scan_period_ms * 1000.0;

    /*
     * Phase budget allocation (as fraction of scan period):
     *
     * Phase 1 (Critical): 10% — safety interlocks, anti-surge
     * Phase 2 (PID):      30% — regulatory control loops
     * Phase 3 (Advanced): 20% — cascade, feedforward, ratio
     * Phase 4 (Logic):    20% — calculations, sequences, totalizers
     * Phase 5 (Comm):     15% — communication, diagnostics
     *                     ---
     *                     95% total (5% margin)
     */
    double budget_fractions[] = {0.10, 0.30, 0.20, 0.20, 0.15};

    for (uint32_t i = 0; i < 5 && i < db->num_scan_phases; i++) {
        db->scan_phases[i].allocated_time_us = total_scan_us * budget_fractions[i];
    }

    /*
     * Assign each control module to its appropriate phase.
     *
     * Modules are scanned in order of phase, and within each phase
     * by module_id (deterministic ordering).
     *
     * Phase assignment priority:
     *   - Safety/interlock tags → Phase 1
     *   - PID modules → Phase 2
     *   - Cascade/ratio/FF modules → Phase 3
     *   - CALC/LOGIC modules → Phase 4
     *   - Unassigned → Phase 5
     */
    for (uint32_t i = 0; i < db->num_control_modules; i++) {
        dcs_control_module_t *mod = &db->control_modules[i];
        if (mod->controller_id != controller_id || !mod->enabled) continue;

        uint32_t phase;

        /* Classify module type to phase */
        if (strcmp(mod->module_type, "SAFETY") == 0
            || strcmp(mod->module_type, "INTERLOCK") == 0) {
            phase = 1;
        } else if (strcmp(mod->module_type, "PID") == 0
                   || strcmp(mod->module_type, "AI") == 0
                   || strcmp(mod->module_type, "AO") == 0) {
            phase = 2;
        } else if (strcmp(mod->module_type, "CASCADE") == 0
                   || strcmp(mod->module_type, "RATIO") == 0
                   || strcmp(mod->module_type, "FFWD") == 0
                   || strcmp(mod->module_type, "SELECT") == 0) {
            phase = 3;
        } else if (strcmp(mod->module_type, "CALC") == 0
                   || strcmp(mod->module_type, "LOGIC") == 0
                   || strcmp(mod->module_type, "TOTAL") == 0) {
            phase = 4;
        } else {
            phase = 5;
        }

        mod->scan_phase = phase;

        /* Update phase statistics */
        if (phase >= 1 && phase <= db->num_scan_phases) {
            db->scan_phases[phase - 1].num_modules++;
            db->scan_phases[phase - 1].actual_time_us += mod->execution_time_us;
        }
    }

    /* Check phase completion */
    for (uint32_t i = 0; i < db->num_scan_phases; i++) {
        db->scan_phases[i].is_complete =
            (db->scan_phases[i].actual_time_us
             <= db->scan_phases[i].allocated_time_us) ? 1 : 0;
    }

    return 1;
}

/*===========================================================================
 * L6: Capacity Metrics
 *===========================================================================*/

int dcs_db_calculate_capacity_metrics(const dcs_system_database_t *db,
                                       double *tag_density,
                                       double *growth_margin_pct)
{
    if (db == NULL) return 0;

    /* Count process areas from tag area codes */
    uint32_t num_areas = 0;
    char area_set[100][8];
    uint32_t area_count = 0;

    for (uint32_t i = 0; i < db->num_tags && area_count < 100; i++) {
        int found = 0;
        for (uint32_t j = 0; j < area_count; j++) {
            if (strcmp(db->tags[i].area_code, area_set[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && db->tags[i].area_code[0] != '\0') {
            strncpy(area_set[area_count], db->tags[i].area_code, 8);
            area_set[area_count][7] = '\0';
            area_count++;
        }
    }
    num_areas = area_count;
    if (num_areas == 0) num_areas = 1; /* At least 1 area to avoid div-by-zero */

    if (tag_density != NULL) {
        *tag_density = (double)db->num_tags / (double)num_areas;
    }

    if (growth_margin_pct != NULL) {
        if (db->max_tags > 0) {
            *growth_margin_pct = (double)(db->max_tags - db->num_tags)
                               / (double)db->max_tags * 100.0;
        } else {
            *growth_margin_pct = 0.0;
        }
    }

    return 1;
}
