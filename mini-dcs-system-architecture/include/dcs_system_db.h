/**
 * @file dcs_system_db.h
 * @brief DCS system configuration database and tag management.
 *
 * Knowledge Level: L3 Engineering Structures + L2 Core Concepts
 *
 * References:
 *   - DCS system configuration philosophy (Honeywell Experion / Yokogawa CENTUM)
 *   - OPC UA Part 3: Address Space Model
 *   - ISA-95 Part 2: Object Model Attributes
 *
 * Covers tag database structure, control module configuration,
 * I/O assignment, scan phase management, and configuration integrity.
 */

#ifndef DCS_SYSTEM_DB_H
#define DCS_SYSTEM_DB_H

#include "dcs_types.h"
#include <stdint.h>

/*===========================================================================
 * L1: DCS Tag Definition
 *===========================================================================*/

/**
 * @brief DCS process tag (point) definition.
 *
 * A tag is the fundamental data entity in a DCS. Every measurement,
 * actuator, calculated value, or configuration parameter is a tag.
 *
 * Tag naming convention: AREA-EQUIPMENT-FUNCTION (ANSI/ISA-5.1)
 * Example: 10-FIC-101 = Area 10, Flow Indicating Controller 101
 */
typedef struct {
    uint32_t            tag_id;
    char                tag_name[40];       /* ISA-5.1 tag name */
    char                description[64];
    char                area_code[8];       /* Process area */
    char                unit_code[8];       /* Process unit */
    dcs_hierarchy_level_t level;
    uint32_t            owning_controller_id;
    uint32_t            associated_io_point_id;
    double              pv;                 /* Process value in EU */
    double              sp;                 /* Setpoint (for control tags) */
    double              op;                 /* Output value */
    double              eu_min;
    double              eu_max;
    char                eu_units[16];
    dcs_signal_quality_t quality;
    uint64_t            last_update_timestamp;
    int                 scan_enabled;
    uint32_t            scan_phase;         /* Execution phase within scan cycle */
    int                 is_critical;        /* Critical tag (redundant path) */
} dcs_tag_t;

/**
 * @brief Control module (function block) configuration.
 *
 * In DCS, control logic is built from function blocks:
 * PID, AI, AO, DI, DO, CALC, LOGIC, SPLITTER, RATIO, SELECTOR, etc.
 *
 * Each control module belongs to a controller and executes
 * in a specific scan phase.
 */
typedef struct {
    uint32_t   module_id;
    char       module_name[32];
    char       module_type[16];    /* "PID", "AI", "AO", "CALC", etc. */
    uint32_t   controller_id;
    uint32_t   num_input_tags;
    uint32_t   num_output_tags;
    uint32_t   input_tag_ids[8];   /* Up to 8 input references */
    uint32_t   output_tag_ids[4];  /* Up to 4 output references */
    uint32_t   scan_phase;
    double     execution_time_us;  /* Estimated execution time */
    int        enabled;
    int        has_alarms;
} dcs_control_module_t;

/*===========================================================================
 * L3: Scan Cycle Scheduling
 *===========================================================================*/

/**
 * @brief Scan cycle phase configuration.
 *
 * DCS controllers typically divide the scan period into phases
 * to manage processing load and ensure deterministic execution.
 *
 * Phase 1: Critical control (safety, compressor surge)
 * Phase 2: Regulatory control (PID loops)
 * Phase 3: Supervisory control (advanced control)
 * Phase 4: Calculation and logic
 * Phase 5: Communication and diagnostics
 */
typedef struct {
    uint32_t   phase_id;
    char       phase_name[32];
    double     allocated_time_us;   /* Time budget for this phase */
    double     actual_time_us;      /* Measured execution time */
    uint32_t   num_modules;
    int        is_complete;         /* Phase completed within budget */
} dcs_scan_phase_t;

/*===========================================================================
 * L3: System Configuration Database
 *===========================================================================*/

/**
 * @brief DCS system database containing all configured entities.
 *
 * The system database is the single source of truth for the
 * entire DCS configuration. It is replicated across engineering
 * stations and domain controllers.
 *
 * Sizing considerations:
 *   - Small system: < 500 tags, < 10 controllers
 *   - Medium system: 500-5000 tags, 10-50 controllers
 *   - Large system: > 5000 tags, > 50 controllers
 */
typedef struct {
    uint32_t            num_tags;
    uint32_t            max_tags;
    dcs_tag_t          *tags;             /* Dynamic array of tags */

    uint32_t            num_controllers;
    uint32_t            max_controllers;
    dcs_controller_config_t *controllers;

    uint32_t            num_io_points;
    uint32_t            max_io_points;
    dcs_io_point_t     *io_points;

    uint32_t            num_control_modules;
    uint32_t            max_control_modules;
    dcs_control_module_t *control_modules;

    uint32_t            num_scan_phases;
    dcs_scan_phase_t    scan_phases[8];     /* Up to 8 scan phases */

    double              min_scan_period_ms;
    double              max_scan_period_ms;
    int                 is_configured;
    int                 is_validated;
} dcs_system_database_t;

/*===========================================================================
 * L3: Core Functions — Configuration Management
 *===========================================================================*/

/**
 * @brief Initialize the system database with specified capacities.
 *
 * Allocates memory for tags, controllers, I/O points, and control modules.
 *
 * @param db                  Database to initialize.
 * @param max_tags            Maximum number of tags.
 * @param max_controllers     Maximum number of controllers.
 * @param max_io              Maximum number of I/O points.
 * @param max_modules         Maximum number of control modules.
 * @return                    1 on success, 0 on memory allocation failure.
 */
int dcs_db_init(dcs_system_database_t *db,
                 uint32_t max_tags,
                 uint32_t max_controllers,
                 uint32_t max_io,
                 uint32_t max_modules);

/**
 * @brief Free resources allocated by the system database.
 *
 * @param db  Database to free.
 */
void dcs_db_free(dcs_system_database_t *db);

/**
 * @brief Add a tag to the system database.
 *
 * @param db    System database.
 * @param tag   Tag to add (tag_id must be unique).
 * @return      1 on success, 0 if database full or duplicate ID.
 */
int dcs_db_add_tag(dcs_system_database_t *db, const dcs_tag_t *tag);

/**
 * @brief Add a controller to the system database.
 *
 * @param db         System database.
 * @param controller Controller to add.
 * @return           1 on success, 0 on failure.
 */
int dcs_db_add_controller(dcs_system_database_t *db,
                           const dcs_controller_config_t *controller);

/**
 * @brief Add an I/O point to the system database.
 *
 * @param db   System database.
 * @param io   I/O point to add.
 * @return     1 on success, 0 on failure.
 */
int dcs_db_add_io_point(dcs_system_database_t *db,
                         const dcs_io_point_t *io);

/**
 * @brief Add a control module to the system database.
 *
 * @param db      System database.
 * @param module  Control module to add.
 * @return        1 on success, 0 on failure.
 */
int dcs_db_add_control_module(dcs_system_database_t *db,
                               const dcs_control_module_t *module);

/*===========================================================================
 * L3: Core Functions — Database Validation
 *===========================================================================*/

/**
 * @brief Validate the system database for consistency.
 *
 * Checks:
 *   1. Every control module references existing tags.
 *   2. Every I/O point belongs to an existing controller.
 *   3. Tag names are unique.
 *   4. Scan phases are properly defined.
 *   5. Controller loading does not exceed limits.
 *   6. Critical tags have redundant communication paths.
 *
 * @param db            System database to validate.
 * @param num_errors    Output: number of validation errors found.
 * @return              1 if valid, 0 if errors found.
 */
int dcs_db_validate(const dcs_system_database_t *db,
                     uint32_t *num_errors);

/**
 * @brief Find a tag by its ISA-5.1 name in the database.
 *
 * @param db        System database.
 * @param tag_name  Tag name to search for.
 * @return          Tag ID on success, UINT32_MAX if not found.
 */
uint32_t dcs_db_find_tag_by_name(const dcs_system_database_t *db,
                                  const char *tag_name);

/**
 * @brief Analyze controller load distribution across scan phases.
 *
 * For each controller, computes the total execution time across
 * all assigned control modules and verifies it fits within the
 * scan period budget (typically 70% max utilization).
 *
 * @param db                System database.
 * @param controller_id     Controller to analyze.
 * @param total_time_us     Output: total module execution time.
 * @param load_pct          Output: CPU load as percentage of scan period.
 * @return                  1 if loading is acceptable, 0 if overloaded.
 */
int dcs_db_analyze_controller_load(const dcs_system_database_t *db,
                                    uint32_t controller_id,
                                    double *total_time_us,
                                    double *load_pct);

/*===========================================================================
 * L6: Classic Problem — Scan Phase Assignment
 *===========================================================================*/

/**
 * @brief Optimize scan phase assignment for control modules.
 *
 * Prioritization:
 *   Phase 1: Safety interlocks, compressor anti-surge
 *   Phase 2: Regulatory PID loops (flow, pressure, level, temperature)
 *   Phase 3: Cascade, ratio, feedforward, override selectors
 *   Phase 4: Calculations, totalizers, logic sequences
 *   Phase 5: Diagnostics, communication tasks (leftover time)
 *
 * @param db           System database.
 * @param controller_id Controller to optimize.
 * @return             1 on success.
 */
int dcs_db_optimize_scan_phases(dcs_system_database_t *db,
                                 uint32_t controller_id);

/**
 * @brief Calculate system tag density and growth margin.
 *
 * Tag density = num_tags / num_process_areas
 * Growth margin = (max_tags - num_tags) / max_tags * 100%
 *
 * @param db               System database.
 * @param tag_density      Output: average tags per process area.
 * @param growth_margin_pct Output: expansion margin percentage.
 * @return                 1 on success.
 */
int dcs_db_calculate_capacity_metrics(const dcs_system_database_t *db,
                                       double *tag_density,
                                       double *growth_margin_pct);

#endif /* DCS_SYSTEM_DB_H */
