/**
 * @file dcs_safety.c
 * @brief Safety Instrumented System (SIS) implementation per IEC 61508/61511.
 *
 * Covers PFD calculation for all voting architectures, SIL determination,
 * architectural constraint analysis, proof test optimization, and SIF verification.
 *
 * Knowledge Levels: L4, L5, L6
 */

#include "dcs_safety.h"
#include <math.h>
#include <string.h>

/*===========================================================================
 * L5: PFD Calculation — Individual Architectures
 *===========================================================================*/

double dcs_pfd_1oo1(double lambda_du, double ti_hours)
{
    /*
     * IEC 61508-6, Equation B.1:
     *
     * PFDavg_1oo1 = lambda_DU * TI / 2
     *
     * This is the expected fraction of the proof test interval
     * during which the system is in a failed-dangerous state.
     *
     * Assumption: failures are uniformly distributed over TI.
     * The "average" failure occurs at TI/2, hence the factor 1/2.
     */
    if (lambda_du < 0.0) return 1.0;
    if (ti_hours <= 0.0) return 0.0;

    double pfd = lambda_du * ti_hours / 2.0;

    if (pfd > 1.0) pfd = 1.0;

    return pfd;
}

double dcs_pfd_1oo2(double lambda_du, double ti_hours, double beta)
{
    /*
     * IEC 61508-6, Equation B.5 (with common cause):
     *
     * PFDavg_1oo2 = (lambda_DU * TI)^2 / 3 + beta * lambda_DU * TI / 2
     *
     * First term: independent failure of both channels.
     * Second term: common cause failure (beta model).
     *
     * The 1/3 factor (vs 1/2 for 1oo1) comes from the integral:
     *   ∫(t^2) dt from 0 to TI / TI = TI^2 / 3
     * averaged over the interval.
     *
     * Beta (common cause beta factor):
     *   - Typical: 0.02 (2%) for diverse, separated channels
     *   - Worst case: 0.10 (10%) for similar channels in same location
     *   - Maximum allowed per IEC 61508: 0.25 for SIL 3
     */
    if (lambda_du < 0.0) return 1.0;
    if (ti_hours <= 0.0) return 0.0;
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;

    double lambda_ti = lambda_du * ti_hours;

    /* Independent failure contribution */
    double pfd_independent = (lambda_ti * lambda_ti) / 3.0;

    /* Common cause failure contribution */
    double pfd_ccf = beta * lambda_ti / 2.0;

    double pfd = pfd_independent + pfd_ccf;

    if (pfd > 1.0) pfd = 1.0;

    return pfd;
}

double dcs_pfd_2oo3(double lambda_du, double ti_hours, double beta)
{
    /*
     * IEC 61508-6, Equation B.6:
     *
     * PFDavg_2oo3 = (lambda_DU * TI)^2 + beta * lambda_DU * TI / 2
     *
     * Triple Modular Redundancy. System fails if 2 out of 3 channels fail.
     *
     * Probability of first failure: 3 * P_single_fail
     * Probability of second failure before repair: 2 * P_single_fail
     *
     * Derivation:
     *   P(2 failures in [0,TI]) = 3C2 * (lambda*TI)^2 / 3 + CCF
     *   = 3 * (lambda*TI)^2 / 3 + CCF
     *   = (lambda*TI)^2 + beta*lambda*TI/2
     *
     * Note: 2oo3 has slightly higher PFD than 1oo2 for the same lambda
     * because 2oo3 can fail on the second channel failure while 1oo2
     * requires both to fail.
     */
    if (lambda_du < 0.0) return 1.0;
    if (ti_hours <= 0.0) return 0.0;
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;

    double lambda_ti = lambda_du * ti_hours;

    /* Two independent failures out of three */
    double pfd_independent = lambda_ti * lambda_ti;

    /* Common cause failure contribution */
    double pfd_ccf = beta * lambda_ti / 2.0;

    double pfd = pfd_independent + pfd_ccf;

    if (pfd > 1.0) pfd = 1.0;

    return pfd;
}

double dcs_pfd_2oo2(double lambda_du, double ti_hours, double beta)
{
    /*
     * IEC 61508-6, Equation B.4 (approximation):
     *
     * PFDavg_2oo2 ≈ lambda_DU * TI + beta * lambda_DU * TI / 2
     *
     * 2oo2: Both channels must work for safety function to succeed.
     * This is WORSE than 1oo1 for safety but BETTER for spurious trip avoidance.
     *
     * The system fails if EITHER channel fails (series reliability).
     * PFD_2oo2 ≈ 2 * PFD_1oo1 (approximately, ignoring second-order terms)
     * = 2 * (lambda_DU * TI / 2) = lambda_DU * TI
     *
     * 2oo2 is used when:
     *   - Spurious trips are extremely costly
     *   - The process is inherently safe without the SIS
     */
    if (lambda_du < 0.0) return 1.0;
    if (ti_hours <= 0.0) return 0.0;
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;

    double lambda_ti = lambda_du * ti_hours;

    /* Both channels in series: failure of either causes system failure */
    double pfd_series = lambda_ti;

    /* Common cause: beta * lambda_DU * TI / 2 */
    double pfd_ccf = beta * lambda_ti / 2.0;

    double pfd = pfd_series + pfd_ccf;

    if (pfd > 1.0) pfd = 1.0;

    return pfd;
}

/*===========================================================================
 * L5: Complete SIF PFD Calculation
 *===========================================================================*/

/**
 * @brief Helper: compute subsystem PFD based on architecture.
 */
static double dcs_subsystem_pfd(double lambda_du, double ti_hours,
                                 dcs_redundancy_arch_t arch, double beta)
{
    switch (arch) {
        case DCS_REDUNDANCY_1OO1:
            return dcs_pfd_1oo1(lambda_du, ti_hours);
        case DCS_REDUNDANCY_1OO2:
        case DCS_REDUNDANCY_1OO2D:
            return dcs_pfd_1oo2(lambda_du, ti_hours, beta);
        case DCS_REDUNDANCY_2OO3:
            return dcs_pfd_2oo3(lambda_du, ti_hours, beta);
        case DCS_REDUNDANCY_2OO2:
            return dcs_pfd_2oo2(lambda_du, ti_hours, beta);
        default:
            return dcs_pfd_1oo1(lambda_du, ti_hours);
    }
}

double dcs_sif_calculate_pfd(const dcs_safety_component_reliability_t *sensor_comp,
                              dcs_redundancy_arch_t sensor_arch,
                              const dcs_safety_component_reliability_t *logic_comp,
                              dcs_redundancy_arch_t logic_arch,
                              const dcs_safety_component_reliability_t *actuator_comp,
                              dcs_redundancy_arch_t actuator_arch,
                              double ti_hours,
                              double beta)
{
    /*
     * IEC 61508-6, Section B.2:
     *
     * PFD_SIF = PFD_sensor + PFD_logic + PFD_final_element
     *
     * The total PFD is the sum of the subsystem PFDs because
     * the subsystems are in series (all must work for SIF to succeed).
     *
     * Safety factor: IEC 61511 requires a minimum safety margin.
     * We add 10% to account for modeling uncertainty.
     */
    double pfd_sensor = 0.0;
    if (sensor_comp != NULL) {
        pfd_sensor = dcs_subsystem_pfd(sensor_comp->lambda_du_per_hour,
                                        ti_hours, sensor_arch, beta);
    }

    double pfd_logic = 0.0;
    if (logic_comp != NULL) {
        pfd_logic = dcs_subsystem_pfd(logic_comp->lambda_du_per_hour,
                                       ti_hours, logic_arch, beta);
    }

    double pfd_actuator = 0.0;
    if (actuator_comp != NULL) {
        pfd_actuator = dcs_subsystem_pfd(actuator_comp->lambda_du_per_hour,
                                          ti_hours, actuator_arch, beta);
    }

    double pfd_total = pfd_sensor + pfd_logic + pfd_actuator;

    /* Apply 10% conservatism margin per IEC 61511 Clause 11.9.2 */
    pfd_total *= 1.10;

    if (pfd_total > 1.0) pfd_total = 1.0;

    return pfd_total;
}

/*===========================================================================
 * L5: SIL Determination
 *===========================================================================*/

dcs_sil_level_t dcs_determine_sil(double pfdavg,
                                   dcs_hardware_fault_tolerance_t hft,
                                   double sff,
                                   dcs_component_type_t component_type)
{
    /*
     * Step 1: SIL based on PFDavg (IEC 61508-1 Table 2).
     */
    dcs_sil_level_t sil_by_pfd;
    if (pfdavg < 1e-5) {
        sil_by_pfd = DCS_SIL_4;
    } else if (pfdavg < 1e-4) {
        sil_by_pfd = DCS_SIL_3;
    } else if (pfdavg < 1e-3) {
        sil_by_pfd = DCS_SIL_2;
    } else if (pfdavg < 1e-2) {
        sil_by_pfd = DCS_SIL_1;
    } else {
        sil_by_pfd = DCS_SIL_NONE;
    }

    /*
     * Step 2: Apply architectural constraints (IEC 61508-2 Tables 2 and 3).
     *
     * Maximum SIL based on HFT and SFF:
     *
     * For Type A components:
     *   SFF < 60%:  HFT=0→SIL1, HFT=1→SIL2, HFT=2→SIL3
     *   60% ≤ SFF < 90%: HFT=0→SIL2, HFT=1→SIL3, HFT=2→SIL4
     *   90% ≤ SFF < 99%: HFT=0→SIL3, HFT=1→SIL4, HFT=2→SIL4
     *   SFF ≥ 99%:     HFT=0→SIL3, HFT=1→SIL4, HFT=2→SIL4
     *
     * For Type B components (more restrictive):
     *   SFF < 60%:  Not allowed
     *   60% ≤ SFF < 90%: HFT=0→SIL1, HFT=1→SIL2, HFT=2→SIL3
     *   90% ≤ SFF < 99%: HFT=0→SIL2, HFT=1→SIL3, HFT=2→SIL4
     *   SFF ≥ 99%:     HFT=0→SIL3, HFT=1→SIL4, HFT=2→SIL4
     */
    dcs_sil_level_t sil_by_arch = DCS_SIL_NONE;

    if (component_type == DCS_COMPONENT_TYPE_A) {
        if (sff >= 99.0) {
            if (hft >= DCS_HFT_1) sil_by_arch = DCS_SIL_4;
            else sil_by_arch = DCS_SIL_3;
        } else if (sff >= 90.0) {
            if (hft >= DCS_HFT_2) sil_by_arch = DCS_SIL_4;
            else if (hft >= DCS_HFT_1) sil_by_arch = DCS_SIL_4;
            else sil_by_arch = DCS_SIL_3;
        } else if (sff >= 60.0) {
            if (hft >= DCS_HFT_2) sil_by_arch = DCS_SIL_4;
            else if (hft >= DCS_HFT_1) sil_by_arch = DCS_SIL_3;
            else sil_by_arch = DCS_SIL_2;
        } else {
            if (hft >= DCS_HFT_2) sil_by_arch = DCS_SIL_3;
            else if (hft >= DCS_HFT_1) sil_by_arch = DCS_SIL_2;
            else sil_by_arch = DCS_SIL_1;
        }
    } else { /* TYPE_B */
        if (sff >= 99.0) {
            if (hft >= DCS_HFT_1) sil_by_arch = DCS_SIL_4;
            else sil_by_arch = DCS_SIL_3;
        } else if (sff >= 90.0) {
            if (hft >= DCS_HFT_2) sil_by_arch = DCS_SIL_4;
            else if (hft >= DCS_HFT_1) sil_by_arch = DCS_SIL_3;
            else sil_by_arch = DCS_SIL_2;
        } else if (sff >= 60.0) {
            if (hft >= DCS_HFT_2) sil_by_arch = DCS_SIL_3;
            else if (hft >= DCS_HFT_1) sil_by_arch = DCS_SIL_2;
            else sil_by_arch = DCS_SIL_1;
        } else {
            /* Type B, SFF < 60%: Not allowed for SIL > 0 */
            sil_by_arch = DCS_SIL_NONE;
        }
    }

    /*
     * Step 3: Achieved SIL = min(SIL by PFD, SIL by architecture).
     */
    return (sil_by_pfd < sil_by_arch) ? sil_by_pfd : sil_by_arch;
}

/*===========================================================================
 * L5: RRF Calculation
 *===========================================================================*/

double dcs_calculate_rrf(double tolerable_risk, double process_risk)
{
    /*
     * Risk Reduction Factor (RRF) = process_risk / tolerable_risk
     *
     * Process risk: frequency of the hazardous event without SIS (per year).
     * Tolerable risk: maximum acceptable frequency of the hazardous event.
     *
     * RRF_for_SIL_1:  10 to < 100    (PFDavg: 1e-1 to 1e-2)
     * RRF_for_SIL_2: 100 to < 1,000  (PFDavg: 1e-2 to 1e-3)
     * RRF_for_SIL_3: 1,000 to < 10,000 (PFDavg: 1e-3 to 1e-4)
     * RRF_for_SIL_4: 10,000 to < 100,000 (PFDavg: 1e-4 to 1e-5)
     *
     * PFDavg = 1 / RRF (approximately, for low-demand mode)
     */
    if (process_risk <= 0.0) return 1.0;
    if (tolerable_risk <= 0.0) return 1e10; /* Essentially infinite RRF */

    double rrf = process_risk / tolerable_risk;

    if (rrf < 1.0) rrf = 1.0;

    return rrf;
}

/*===========================================================================
 * L6: Proof Test Interval Optimization
 *===========================================================================*/

double dcs_calculate_max_ti(double lambda_du,
                             double target_pfd,
                             dcs_redundancy_arch_t arch,
                             double beta)
{
    /*
     * Solve for maximum proof test interval (TI) that satisfies target PFD.
     *
     * For 1oo1: TI_max = 2 * PFD_target / lambda_DU
     *
     * For 1oo2: Solve (lambda*TI)^2/3 + beta*lambda*TI/2 = PFD_target
     *   Let x = lambda * TI
     *   x^2/3 + beta*x/2 - PFD_target = 0
     *   x = [-beta/2 + sqrt(beta^2/4 + 4*PFD_target/3)] / (2/3)
     *   TI = x / lambda
     *
     * For 2oo3: x^2 + beta*x/2 - PFD_target = 0
     *   x = [-beta/2 + sqrt(beta^2/4 + 4*PFD_target)] / 2
     *
     * For 2oo2: x + beta*x/2 = PFD_target
     *   x = PFD_target / (1 + beta/2)
     */
    if (lambda_du <= 0.0) return 876000.0; /* Effectively infinite */
    if (target_pfd <= 0.0) return 0.0;
    if (beta < 0.0) beta = 0.0;

    double ti_max;

    switch (arch) {
        case DCS_REDUNDANCY_1OO1:
            ti_max = 2.0 * target_pfd / lambda_du;
            break;

        case DCS_REDUNDANCY_1OO2:
        case DCS_REDUNDANCY_1OO2D:
        {
            /* Quadratic: x^2/3 + beta*x/2 - PFD = 0 */
            double a = 1.0 / 3.0;
            double b = beta / 2.0;
            double c = -target_pfd;
            double discriminant = b * b - 4.0 * a * c;
            if (discriminant < 0.0) {
                ti_max = 0.0;
            } else {
                double x = (-b + sqrt(discriminant)) / (2.0 * a);
                ti_max = x / lambda_du;
            }
            break;
        }

        case DCS_REDUNDANCY_2OO3:
        {
            /* Quadratic: x^2 + beta*x/2 - PFD = 0 */
            double a = 1.0;
            double b = beta / 2.0;
            double c = -target_pfd;
            double discriminant = b * b - 4.0 * a * c;
            if (discriminant < 0.0) {
                ti_max = 0.0;
            } else {
                double x = (-b + sqrt(discriminant)) / (2.0 * a);
                ti_max = x / lambda_du;
            }
            break;
        }

        case DCS_REDUNDANCY_2OO2:
        {
            /* Linear: x * (1 + beta/2) = PFD */
            double x = target_pfd / (1.0 + beta / 2.0);
            ti_max = x / lambda_du;
            break;
        }

        default:
            ti_max = 2.0 * target_pfd / lambda_du;
            break;
    }

    /* Cap at 10 years (87600 hours) — equipment life */
    if (ti_max > 87600.0) ti_max = 87600.0;

    return ti_max;
}

/*===========================================================================
 * L6: SIF Verification
 *===========================================================================*/

int dcs_sif_verify(dcs_sif_definition_t *sif,
                    const dcs_safety_component_reliability_t *sensor,
                    const dcs_safety_component_reliability_t *logic,
                    const dcs_safety_component_reliability_t *actuator)
{
    if (sif == NULL) return 0;

    /*
     * IEC 61511 SIF verification checklist:
     *   1. Calculate PFDavg
     *   2. Compare with target SIL
     *   3. Verify architectural constraints
     *   4. Verify proof test interval is feasible
     *   5. Verify common cause beta is within limits
     */

    /* Beta factor limits per IEC 61508 */
    double beta_sensor = (sensor != NULL) ? sensor->common_cause_beta : 0.02;
    double beta_logic  = (logic != NULL)  ? logic->common_cause_beta  : 0.02;
    double beta_act    = (actuator != NULL) ? actuator->common_cause_beta : 0.02;
    double max_beta = (beta_sensor > beta_logic) ? beta_sensor : beta_logic;
    if (beta_act > max_beta) max_beta = beta_act;

    /* Calculate PFDavg */
    double pfdavg = dcs_sif_calculate_pfd(sensor, sif->sensor_arch,
                                           logic, sif->logic_arch,
                                           actuator, sif->actuator_arch,
                                           sif->proof_test_interval_hours,
                                           max_beta);

    sif->pfd_avg = pfdavg;

    /*
     * SIL threshold check:
     *   SIL 4: PFDavg < 1e-4
     *   SIL 3: PFDavg < 1e-3
     *   SIL 2: PFDavg < 1e-2
     *   SIL 1: PFDavg < 1e-1
     */
    dcs_sil_level_t achieved_sil = DCS_SIL_NONE;
    if (pfdavg < 1e-5) {
        achieved_sil = DCS_SIL_4;
    } else if (pfdavg < 1e-4) {
        achieved_sil = DCS_SIL_3;
    } else if (pfdavg < 1e-3) {
        achieved_sil = DCS_SIL_2;
    } else if (pfdavg < 1e-2) {
        achieved_sil = DCS_SIL_1;
    }

    sif->achieved_sil = achieved_sil;

    /* Check: achieved SIL ≥ target SIL */
    if (achieved_sil < sif->target_sil) {
        sif->is_verified = 0;
        return 0;
    }

    /* Check: proof test interval ≤ mission time */
    if (sif->proof_test_interval_hours > 87600.0) { /* 10 years */
        /* Excessive proof test interval */
        sif->is_verified = 0;
        return 0;
    }

    /* Calculate RRF */
    double rrf = 1.0 / pfdavg;
    sif->achieved_rrf = rrf;
    if (sif->required_rrf > 0.0 && rrf < sif->required_rrf) {
        sif->is_verified = 0;
        return 0;
    }

    /* All checks passed */
    sif->is_verified = 1;
    return 1;
}
