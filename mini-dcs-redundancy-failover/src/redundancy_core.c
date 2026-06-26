/**
 * @file redundancy_core.c
 * @brief DCS Redundancy Core Implementation
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * Knowledge Coverage:
 *   L1 - Redundancy architecture type definitions and management
 *   L2 - Group health evaluation, degraded mode
 *   L3 - Serialization, diversity, CCF modeling
 *   L4 - Reliability formulas: k-of-n, SIL PFD, MTBF
 */

#include "redundancy_core.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * L1: Redundancy Group Initialization
 * ============================================================================
 * Knowledge: A redundancy group (MooN) is defined by its architecture,
 * number of modules N, and minimum required healthy modules M.
 *
 * Invariant: 0 < M <= N <= REDUNDANCY_MAX_MODULES
 * Valid architectures enforce: M=1 for 1oo2, M=2 for 2oo3, etc.
 */

int redundancy_group_init(redundancy_group_t *group,
                          redundancy_architecture_t arch,
                          uint8_t n, uint8_t m,
                          const char *group_id)
{
    if (!group || !group_id) return -1;
    if (n < 1 || n > REDUNDANCY_MAX_MODULES) return -1;
    if (m < 1 || m > n) return -1;
    /* Architecture-specific M validation */
    switch (arch) {
        case REDUNDANCY_1OO2:
        case REDUNDANCY_HOT_STANDBY:
            if (m != 1 || n != 2) return -1;
            break;
        case REDUNDANCY_2OO2:
            if (m != 2 || n != 2) return -1;
            break;
        case REDUNDANCY_2OO3:
        case REDUNDANCY_TMR:
            if (m != 2 || n != 3) return -1;
            break;
        case REDUNDANCY_2OO4:
            if (m != 2 || n != 4) return -1;
            break;
        default:
            break;
    }

    memset(group, 0, sizeof(*group));
    group->architecture = arch;
    group->n_modules = n;
    group->m_required = m;
    strncpy(group->group_id, group_id, REDUNDANCY_ID_MAX_LEN - 1);
    group->group_id[REDUNDANCY_ID_MAX_LEN - 1] = '\0';
    group->group_healthy = false;
    group->degraded_mode = false;
    group->primary_index = 0;
    group->secondary_index = (n > 1) ? 1 : 0;
    group->failover_state = FAILOVER_STATE_NORMAL;
    group->group_availability = 0.0;
    group->total_uptime_ms = 0;

    /* Initialize all modules to OFFLINE with defaults */
    for (uint8_t i = 0; i < n; i++) {
        redundancy_module_t *mod = &group->modules[i];
        memset(mod, 0, sizeof(*mod));
        mod->slot_index = i;
        mod->role = MODULE_ROLE_OFFLINE;
        mod->health = MODULE_HEALTH_OFFLINE;
        mod->primary_capable = true;
    }
    return 0;
}

int redundancy_group_add_module(redundancy_group_t *group,
                                uint8_t slot,
                                const char *module_id)
{
    if (!group || !module_id) return -1;
    if (slot >= group->n_modules) return -1;

    redundancy_module_t *mod = &group->modules[slot];
    mod->role = MODULE_ROLE_STANDBY;
    mod->health = MODULE_HEALTH_HEALTHY;
    mod->primary_capable = true;
    mod->network_reachable = true;
    mod->uptime_ms = 0;
    mod->heartbeat_counter = 0;
    mod->fault_count = 0;
    mod->cpu_load = 0.0;
    mod->memory_available_mb = 256.0;
    mod->sync_sequence = 0;
    strncpy(mod->module_id, module_id, REDUNDANCY_ID_MAX_LEN - 1);
    mod->module_id[REDUNDANCY_ID_MAX_LEN - 1] = '\0';

    /* Auto-assign primary to slot 0, secondary to slot 1 */
    if (slot == 0 && group->n_modules > 0) {
        mod->role = MODULE_ROLE_PRIMARY;
        group->primary_index = 0;
    } else if (slot == 1 && group->n_modules > 1) {
        mod->role = MODULE_ROLE_SECONDARY;
        group->secondary_index = 1;
    }

    /* Recalculate group health after adding module */
    uint8_t healthy = redundancy_group_healthy_count(group);
    group->group_healthy = (healthy >= group->m_required);

    return 0;
}

/* ============================================================================
 * L2: Module Health Management
 * ============================================================================
 * Knowledge: Health status transitions follow a state machine.
 * HEALTHY <-> DEGRADED -> FAULTY -> OFFLINE/FAIL_SAFE.
 * Setting a module health triggers group-level re-evaluation.
 */

int redundancy_module_set_health(redundancy_group_t *group,
                                 uint8_t slot,
                                 module_health_t health)
{
    if (!group) return -1;
    if (slot >= group->n_modules) return -1;

    redundancy_module_t *mod = &group->modules[slot];
    mod->health = health;

    /* Health transitions */
    if (health == MODULE_HEALTH_FAULTY || health == MODULE_HEALTH_OFFLINE) {
        mod->fault_count++;
        /* If primary fails, mark for failover consideration */
        if (slot == group->primary_index && mod->role == MODULE_ROLE_PRIMARY) {
            mod->role = MODULE_ROLE_OFFLINE;
        }
    }

    /* Re-evaluate group health */
    uint8_t healthy = redundancy_group_healthy_count(group);
    group->group_healthy = (healthy >= group->m_required);
    group->degraded_mode = (healthy < group->n_modules) && group->group_healthy;

    /* If primary is no longer healthy, try to promote secondary */
    if (slot == group->primary_index && health != MODULE_HEALTH_HEALTHY) {
        for (uint8_t i = 0; i < group->n_modules; i++) {
            if (i != slot && group->modules[i].health == MODULE_HEALTH_HEALTHY) {
                group->modules[i].role = MODULE_ROLE_PRIMARY;
                group->primary_index = i;
                group->failover_count++;
                group->last_failover_time_ms = group->total_uptime_ms;
                break;
            }
        }
    }

    return 0;
}

uint8_t redundancy_group_healthy_count(const redundancy_group_t *group)
{
    if (!group) return 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < group->n_modules; i++) {
        if (group->modules[i].health == MODULE_HEALTH_HEALTHY) {
            count++;
        }
    }
    return count;
}

bool redundancy_group_is_available(const redundancy_group_t *group)
{
    if (!group) return false;
    return group->group_healthy;
}

int redundancy_group_primary_index(const redundancy_group_t *group)
{
    if (!group) return -1;
    return (int)group->primary_index;
}

/* ============================================================================
 * L1: Architecture Name Resolution
 * ============================================================================
 */

const char *redundancy_arch_name(redundancy_architecture_t arch)
{
    switch (arch) {
        case REDUNDANCY_NONE:         return "None (Single)";
        case REDUNDANCY_1OO2:         return "1oo2";
        case REDUNDANCY_2OO2:         return "2oo2";
        case REDUNDANCY_2OO3:         return "2oo3";
        case REDUNDANCY_2OO4:         return "2oo4";
        case REDUNDANCY_1OO2D:        return "1oo2D";
        case REDUNDANCY_2OO2D:        return "2oo2D";
        case REDUNDANCY_TMR:          return "TMR";
        case REDUNDANCY_HOT_STANDBY:  return "Hot Standby";
        case REDUNDANCY_WARM_STANDBY: return "Warm Standby";
        case REDUNDANCY_COLD_STANDBY: return "Cold Standby";
        case REDUNDANCY_N_PLUS_1:     return "N+1";
        case REDUNDANCY_RING_MESH:    return "Ring/Mesh Network";
        case REDUNDANCY_DUAL_NET:     return "Dual Network";
        case REDUNDANCY_DUAL_POWER:   return "Dual Power";
        default:                      return "Unknown";
    }
}

/* ============================================================================
 * L4: Reliability Engineering Formulas
 * ============================================================================
 * Knowledge: Reliability Block Diagrams (RBD) model system reliability
 * as combinations of series and parallel blocks.
 *
 * Series: R_sys = prod(R_i) -- system fails if ANY component fails
 * Parallel: R_sys = 1 - prod(1-R_i) -- system fails only if ALL fail
 * k-of-n: R_sys = sum_{i=k}^{n} C(n,i) * R^i * (1-R)^{n-i}
 *
 * Reference: Rausand & Hoyland, System Reliability Theory (2004)
 */

static double binomial_coefficient(uint8_t n, uint8_t k)
{
    if (k > n) return 0.0;
    if (k == 0 || k == n) return 1.0;
    if (k > n - k) k = n - k;
    double result = 1.0;
    for (uint8_t i = 1; i <= k; i++) {
        result *= (double)(n - k + i) / (double)i;
    }
    return result;
}

double redundancy_reliability_factor(redundancy_architecture_t arch,
                                     double single_reliability)
{
    if (single_reliability < 0.0 || single_reliability > 1.0) return -1.0;
    double r = single_reliability;

    switch (arch) {
        case REDUNDANCY_NONE:
            return 1.0;  /* No improvement */
        case REDUNDANCY_1OO2:
        case REDUNDANCY_HOT_STANDBY:
            return (1.0 - (1.0 - r) * (1.0 - r)) / r;
        case REDUNDANCY_2OO2:
            return r;  /* Both must work, so R_2oo2 = r^2 -- actually worse */
        case REDUNDANCY_2OO3:
        case REDUNDANCY_TMR:
            /* R_TMR = r^3 + 3*r^2*(1-r) = 3r^2 - 2r^3 */
            return (3.0 * r * r - 2.0 * r * r * r) / r;
        case REDUNDANCY_2OO4:
            /* R_2oo4 = C(4,2)*r^2*(1-r)^2 + C(4,3)*r^3*(1-r) + C(4,4)*r^4 */
            {
                double r2 = r * r;
                double r3 = r2 * r;
                double r4 = r3 * r;
                double s = 1.0 - r;
                return (6.0 * r2 * s * s + 4.0 * r3 * s + r4) / r;
            }
        case REDUNDANCY_N_PLUS_1:
            return (1.0 + r) / 2.0;  /* Approximate improvement */
        default:
            return 1.0;
    }
}

double redundancy_k_of_n_availability(uint8_t k, uint8_t n, double availability)
{
    if (n == 0 || k > n) return -1.0;
    if (availability < 0.0 || availability > 1.0) return -1.0;

    double result = 0.0;
    for (uint8_t i = k; i <= n; i++) {
        double bc = binomial_coefficient(n, i);
        result += bc * pow(availability, (double)i)
                       * pow(1.0 - availability, (double)(n - i));
    }
    return result;
}

double redundancy_system_mtbf(redundancy_architecture_t arch,
                              double single_mtbf, double mttr,
                              const ccf_model_t *ccf)
{
    if (single_mtbf <= 0.0 || mttr < 0.0) return -1.0;

    double lambda = 1.0 / single_mtbf;
    double beta = (ccf) ? ccf->beta_factor : 0.0;

    switch (arch) {
        case REDUNDANCY_NONE:
            return single_mtbf;
        case REDUNDANCY_1OO2:
        case REDUNDANCY_HOT_STANDBY:
            /* 1oo2 with repair: MTBF_sys ~= MTBF^2 / (2 * MTTR)
             * with CCF: lambda_eff = beta*lambda + 2*(1-beta)^2*lambda^2*MTTR */
            {
                double lam_eff = beta * lambda
                    + 2.0 * (1.0 - beta) * (1.0 - beta)
                          * lambda * lambda * mttr;
                return (lam_eff > 0.0) ? (1.0 / lam_eff) : 1e9;
            }
        case REDUNDANCY_2OO3:
        case REDUNDANCY_TMR:
            /* 2oo3 with repair: MTBF_sys ~= 5*MTBF^2 / (6*MTTR) */
            {
                double lam_eff = beta * lambda
                    + 6.0 * (1.0 - beta) * (1.0 - beta)
                          * lambda * lambda * mttr / 5.0;
                return (lam_eff > 0.0) ? (1.0 / lam_eff) : 1e9;
            }
        case REDUNDANCY_2OO4:
            /* 2oo4: MTBF_sys ~= MTBF^3 / (12*MTTR^2) (approximation) */
            return (single_mtbf * single_mtbf * single_mtbf)
                   / (12.0 * mttr * mttr);
        default:
            return single_mtbf;
    }
}

/* ============================================================================
 * L4: SIL PFD Calculations per IEC 61508-6
 * ============================================================================
 * Knowledge: Safety Integrity Level (SIL) is determined by the average
 * Probability of Failure on Demand (PFDavg) for low-demand systems.
 *
 * SIL1: 10^-6 <= PFD < 10^-5  (Risk Reduction Factor 10-100)
 * SIL2: 10^-7 <= PFD < 10^-6  (RRF 100-1,000)
 * SIL3: 10^-8 <= PFD < 10^-7  (RRF 1,000-10,000)
 * SIL4: 10^-9 <= PFD < 10^-8  (RRF 10,000-100,000)
 *
 * PFD single-channel: PFDavg = lambda_DU * T1 / 2
 * PFD 1oo2: PFDavg = ((1-beta)*lambda_DU)^2 * T1^2 / 3 + beta*lambda_DU*T1/2
 * PFD 2oo3: PFDavg = ((1-beta)*lambda_DU)^2 * T1^2 + beta*lambda_DU*T1/2
 *
 * Reference: IEC 61508-6:2010, Annex B
 */

double redundancy_sil_pfd(redundancy_architecture_t arch,
                          double pfd_single, double beta,
                          double proof_test_interval_hours)
{
    if (pfd_single < 0.0 || beta < 0.0 || beta > 1.0) return -1.0;
    if (proof_test_interval_hours <= 0.0) return -1.0;

    /* lambda_DU from single-channel PFD: lambda_DU = 2 * PFD / T1 */
    double lambda_du = 2.0 * pfd_single / proof_test_interval_hours;

    switch (arch) {
        case REDUNDANCY_NONE:
            return lambda_du * proof_test_interval_hours / 2.0;
        case REDUNDANCY_1OO2:
        case REDUNDANCY_HOT_STANDBY: {
            double t1 = proof_test_interval_hours;
            double lam_eff = (1.0 - beta) * lambda_du;
            return lam_eff * lam_eff * t1 * t1 / 3.0
                   + beta * lambda_du * t1 / 2.0;
        }
        case REDUNDANCY_2OO3:
        case REDUNDANCY_TMR: {
            double t1 = proof_test_interval_hours;
            double lam_eff = (1.0 - beta) * lambda_du;
            return lam_eff * lam_eff * t1 * t1
                   + beta * lambda_du * t1 / 2.0;
        }
        case REDUNDANCY_2OO4: {
            double t1 = proof_test_interval_hours;
            double lam_eff = (1.0 - beta) * lambda_du;
            return 2.0 * lam_eff * lam_eff * lam_eff * t1 * t1 * t1 / 4.0
                   + beta * lambda_du * t1 / 2.0;
        }
        default:
            return pfd_single;
    }
}

int redundancy_pfd_to_sil(double pfd_avg)
{
    /* SIL ranges per IEC 61508-1 Table 2:
     * SIL4: [1e-9, 1e-8), SIL3: [1e-8, 1e-7),
     * SIL2: [1e-7, 1e-6), SIL1: [1e-6, 1e-5) */
    if (pfd_avg >= 1e-5) return 0;
    if (pfd_avg >= 1e-6) return 1;
    if (pfd_avg >= 1e-7) return 2;
    if (pfd_avg >= 1e-8) return 3;
    return 4;
}

/* ============================================================================
 * L2: Common Cause Failure Model
 * ============================================================================
 * Knowledge: The beta-factor model is the simplest CCF model:
 *   lambda_CCF = beta * lambda       (common cause portion)
 *   lambda_ind = (1-beta) * lambda   (independent portion)
 *
 * Beta values (IEC 61508-6 Table D.1 guidance):
 *   1% for highly diverse systems
 *   5% for moderate diversity
 *   10% for limited diversity
 *   20% for identical redundant channels without diversity
 */

double redundancy_ccf_adjusted_lambda(double single_lambda, double beta)
{
    if (single_lambda <= 0.0 || beta < 0.0 || beta > 1.0) return -1.0;
    return beta * single_lambda;  /* Common cause portion */
}

double redundancy_diversity_beta_reduction(diversity_type_t diversity)
{
    /* Returns the effective beta reduction factor per IEC 61508-6 Table D.1 */
    switch (diversity) {
        case DIVERSITY_NONE:          return 1.00;  /* Full beta */
        case DIVERSITY_HARDWARE:      return 0.50;  /* 50% beta reduction */
        case DIVERSITY_SOFTWARE:      return 0.60;  /* 40% beta reduction */
        case DIVERSITY_VENDOR:        return 0.40;  /* 60% beta reduction */
        case DIVERSITY_FULL:          return 0.20;  /* 80% beta reduction */
        case DIVERSITY_ARCHITECTURAL: return 0.30;  /* 70% beta reduction */
        default: return 1.0;
    }
}

/* ============================================================================
 * L3: Degraded Mode and Fault Tolerance
 * ============================================================================
 * Knowledge: Degraded mode is a safety-critical operational state.
 * The system must monitor how many faults remain tolerable before
 * overall availability is lost. This drives maintenance scheduling.
 */

bool redundancy_group_is_degraded(const redundancy_group_t *group)
{
    if (!group) return false;
    return group->degraded_mode;
}

int redundancy_group_fault_tolerance(const redundancy_group_t *group)
{
    if (!group) return -1;
    uint8_t healthy = redundancy_group_healthy_count(group);
    return (int)healthy - (int)group->m_required;
}

/* ============================================================================
 * L3: Serialization for Checkpointing
 * ============================================================================
 * Knowledge: Checkpointing allows recovery from failure by persisting
 * the state of the redundancy group. Serialization converts the in-memory
 * structure to a byte stream for storage or network transfer.
 */

int redundancy_group_serialize(const redundancy_group_t *group,
                               uint8_t *buffer, size_t buffer_size)
{
    if (!group || !buffer) return -1;
    size_t needed = sizeof(*group);
    if (buffer_size < needed) return -1;
    memcpy(buffer, group, needed);
    return (int)needed;
}

int redundancy_group_deserialize(redundancy_group_t *group,
                                 const uint8_t *buffer, size_t buffer_size)
{
    if (!group || !buffer) return -1;
    size_t needed = sizeof(*group);
    if (buffer_size < needed) return -1;
    memcpy(group, buffer, needed);
    return 0;
}
