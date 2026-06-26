/**
 * @file redundancy_core.h
 * @brief DCS Redundancy Core Types, Architectures, and Configurations
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover (7. mini-dcs-architecture-application)
 *
 * Knowledge Coverage:
 *   L1 - Core definitions: redundancy architectures, module roles, health states
 *   L2 - Core concepts: degraded modes, diversity, common cause avoidance
 *   L3 - Engineering structures: redundancy group data structures
 *
 * Reference Standards:
 *   IEC 61508 Functional Safety of E/E/PE Safety-Related Systems
 *   IEC 61511 Functional Safety for the Process Industry
 *   ISA-TR84.00.02 Safety Instrumented Functions (SIF) - SIL Evaluation
 *
 * Nine-School Mapping:
 *   MIT 6.302 - Redundancy in feedback systems
 *   CMU 24-677 - Fault-tolerant architectures
 *   RWTH Aachen - DCS redundancy engineering
 *   Tsinghua - Process control system reliability
 */

#ifndef REDUNDANCY_CORE_H
#define REDUNDANCY_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * L1: Core Definitions — Redundancy Architecture Types
 * ============================================================================
 * Definition: Redundancy is the duplication of critical components to increase
 * reliability, where M out of N (MooN) means M units must function for the
 * system to be operational out of N total units. Common architectures:
 *   1oo2: One-out-of-two, either unit suffices (fault-tolerant to 1 failure)
 *   2oo3: Two-out-of-three, majority voting (Byzantine-tolerant to 1 fault)
 *   2oo4: Two-out-of-four, extended availability
 *   TMR:  Triple Modular Redundancy (synonymous with 2oo3 in voting context)
 *
 * Theorem (von Neumann 1956): TMR with perfect voter has reliability
 *   R_TMR = 3R^2 - 2R^3, which exceeds R for R > 0.5.
 */

typedef enum {
    REDUNDANCY_NONE          = 0,
    REDUNDANCY_1OO2          = 1,
    REDUNDANCY_2OO2          = 2,
    REDUNDANCY_2OO3          = 3,
    REDUNDANCY_2OO4          = 4,
    REDUNDANCY_1OO2D         = 5,
    REDUNDANCY_2OO2D         = 6,
    REDUNDANCY_TMR           = 7,
    REDUNDANCY_HOT_STANDBY   = 8,
    REDUNDANCY_WARM_STANDBY  = 9,
    REDUNDANCY_COLD_STANDBY  = 10,
    REDUNDANCY_N_PLUS_1      = 11,
    REDUNDANCY_RING_MESH     = 12,
    REDUNDANCY_DUAL_NET      = 13,
    REDUNDANCY_DUAL_POWER    = 14,
    REDUNDANCY_COUNT         = 15
} redundancy_architecture_t;

typedef enum {
    MODULE_ROLE_PRIMARY   = 0,
    MODULE_ROLE_SECONDARY = 1,
    MODULE_ROLE_STANDBY   = 2,
    MODULE_ROLE_OFFLINE   = 3
} module_role_t;

typedef enum {
    MODULE_HEALTH_HEALTHY   = 0,
    MODULE_HEALTH_DEGRADED  = 1,
    MODULE_HEALTH_FAULTY    = 2,
    MODULE_HEALTH_OFFLINE   = 3,
    MODULE_HEALTH_FAIL_SAFE = 4,
    MODULE_HEALTH_MAINT     = 5,
    MODULE_HEALTH_TESTING   = 6
} module_health_t;

typedef enum {
    FAILOVER_STATE_NORMAL      = 0,
    FAILOVER_STATE_DEGRADED    = 1,
    FAILOVER_STATE_FAILING_OVER = 2,
    FAILOVER_STATE_FAILED_OVER = 3,
    FAILOVER_STATE_RECOVERING  = 4,
    FAILOVER_STATE_SPLIT_BRAIN = 5
} failover_state_t;

#define REDUNDANCY_MAX_MODULES 8
#define REDUNDANCY_ID_MAX_LEN 32

typedef enum {
    HEALTH_CHECK_10MS   = 10,
    HEALTH_CHECK_50MS   = 50,
    HEALTH_CHECK_100MS  = 100,
    HEALTH_CHECK_250MS  = 250,
    HEALTH_CHECK_500MS  = 500,
    HEALTH_CHECK_1000MS = 1000,
    HEALTH_CHECK_5000MS = 5000
} health_check_period_ms_t;

typedef struct {
    char     module_id[REDUNDANCY_ID_MAX_LEN];
    uint8_t  slot_index;
    module_role_t    role;
    module_health_t  health;
    uint64_t uptime_ms;
    uint32_t heartbeat_counter;
    uint32_t last_heartbeat_rcvd;
    uint32_t sync_sequence;
    uint32_t fault_count;
    bool     primary_capable;
    bool     network_reachable;
    double   cpu_load;
    double   memory_available_mb;
    uint8_t  reserved[16];
} redundancy_module_t;

typedef struct {
    redundancy_architecture_t architecture;
    uint8_t  n_modules;
    uint8_t  m_required;
    redundancy_module_t modules[REDUNDANCY_MAX_MODULES];
    uint8_t  primary_index;
    uint8_t  secondary_index;
    bool     group_healthy;
    bool     degraded_mode;
    failover_state_t failover_state;
    uint32_t failover_count;
    uint64_t last_failover_time_ms;
    uint64_t total_uptime_ms;
    double   group_availability;
    char     group_id[REDUNDANCY_ID_MAX_LEN];
} redundancy_group_t;

typedef enum {
    DIVERSITY_NONE          = 0,
    DIVERSITY_HARDWARE      = 1,
    DIVERSITY_SOFTWARE      = 2,
    DIVERSITY_VENDOR        = 3,
    DIVERSITY_FULL          = 4,
    DIVERSITY_ARCHITECTURAL = 5
} diversity_type_t;

typedef struct {
    double beta_factor;
    diversity_type_t diversity;
    double alpha_high_demand;
    bool   physical_separation;
    bool   independent_power;
    bool   independent_network;
} ccf_model_t;

/* Core API */
int redundancy_group_init(redundancy_group_t *group,
                          redundancy_architecture_t arch,
                          uint8_t n, uint8_t m,
                          const char *group_id);

int redundancy_group_add_module(redundancy_group_t *group,
                                uint8_t slot,
                                const char *module_id);

int redundancy_module_set_health(redundancy_group_t *group,
                                 uint8_t slot,
                                 module_health_t health);

uint8_t redundancy_group_healthy_count(const redundancy_group_t *group);

bool redundancy_group_is_available(const redundancy_group_t *group);

int redundancy_group_primary_index(const redundancy_group_t *group);

const char *redundancy_arch_name(redundancy_architecture_t arch);

double redundancy_reliability_factor(redundancy_architecture_t arch,
                                     double single_reliability);

double redundancy_k_of_n_availability(uint8_t k, uint8_t n, double availability);

double redundancy_system_mtbf(redundancy_architecture_t arch,
                              double single_mtbf, double mttr,
                              const ccf_model_t *ccf);

double redundancy_sil_pfd(redundancy_architecture_t arch,
                          double pfd_single, double beta,
                          double proof_test_interval_hours);

int redundancy_pfd_to_sil(double pfd_avg);

double redundancy_ccf_adjusted_lambda(double single_lambda, double beta);

double redundancy_diversity_beta_reduction(diversity_type_t diversity);

bool redundancy_group_is_degraded(const redundancy_group_t *group);

int redundancy_group_fault_tolerance(const redundancy_group_t *group);

int redundancy_group_serialize(const redundancy_group_t *group,
                               uint8_t *buffer, size_t buffer_size);

int redundancy_group_deserialize(redundancy_group_t *group,
                                 const uint8_t *buffer, size_t buffer_size);

#endif /* REDUNDANCY_CORE_H */
