/**
 * @file dcs_safety.h
 * @brief Safety Instrumented System (SIS) integration with DCS.
 *
 * Knowledge Level: L4 Engineering Standards (IEC 61508 / IEC 61511)
 *
 * References:
 *   - IEC 61508:2010 — Functional Safety of E/E/PE Safety-Related Systems
 *   - IEC 61511:2016 — Functional Safety for Process Industry Sector
 *   - ISA-TR84.00.02 — Safety Integrity Level (SIL) Verification
 *   - Gruhn & Cheddie, "Safety Instrumented Systems: Design, Analysis, and Justification" (2006)
 *
 * Covers SIL determination, PFD calculation, proof test interval analysis,
 * architectural constraints (HFT), and common cause failure analysis.
 */

#ifndef DCS_SAFETY_H
#define DCS_SAFETY_H

#include "dcs_types.h"
#include <stdint.h>

/*===========================================================================
 * L4: SIF Reliability Parameters (IEC 61508)
 *===========================================================================*/

/**
 * @brief Reliability parameters for a single safety component.
 *
 * These are the fundamental parameters used in PFD/PFH calculations
 * per IEC 61508-6.
 *
 * lambda_DD : Dangerous Detected failure rate (per hour)
 * lambda_DU : Dangerous Undetected failure rate (per hour)
 * lambda_S  : Safe failure rate (per hour)
 *
 * DC (Diagnostic Coverage) = lambda_DD / (lambda_DD + lambda_DU)
 * SFF (Safe Failure Fraction) = (lambda_S + lambda_DD) / (lambda_S + lambda_DD + lambda_DU)
 */
typedef struct {
    double   lambda_dd_per_hour;   /* Dangerous detected failure rate */
    double   lambda_du_per_hour;   /* Dangerous undetected failure rate */
    double   lambda_s_per_hour;    /* Safe failure rate */
    double   diagnostic_coverage;  /* Diagnostic coverage (0-1) */
    double   safe_failure_fraction; /* SFF per IEC 61508 */
    double   proof_test_coverage;  /* Proof test coverage (0-1, typically 0.7-0.99) */
    double   mttr_hours;           /* Mean time to repair in hours */
    double   common_cause_beta;    /* Beta factor for CCF (0.01-0.10 typical) */
    double   mission_time_hours;   /* Equipment mission time (typically 87600 = 10yr) */
} dcs_safety_component_reliability_t;

/*===========================================================================
 * L4: Architectural Constraint Analysis (IEC 61508-2)
 *===========================================================================*/

/**
 * @brief Hardware Fault Tolerance (HFT) per IEC 61508-2.
 *
 * HFT = N - M, where N = total channels, M = channels needed for function.
 * 1oo1: HFT = 0
 * 1oo2: HFT = 1
 * 2oo3: HFT = 1
 * 1oo3: HFT = 2
 */
typedef enum {
    DCS_HFT_0 = 0,
    DCS_HFT_1 = 1,
    DCS_HFT_2 = 2
} dcs_hardware_fault_tolerance_t;

/**
 * @brief Type classification for architectural constraints.
 *
 * Type A: Failure modes well-defined, behavior under fault completely
 *         known, dependable field failure data. (e.g., relays, simple sensors)
 * Type B: Complex components with undefined failure modes.
 *         (e.g., microprocessors, smart transmitters, logic solvers)
 */
typedef enum {
    DCS_COMPONENT_TYPE_A = 0,
    DCS_COMPONENT_TYPE_B = 1
} dcs_component_type_t;

/*===========================================================================
 * L5: PFD Calculation Functions
 *===========================================================================*/

/**
 * @brief Calculate PFDavg for a 1oo1 (simplex) safety function.
 *
 * PFDavg_1oo1 = lambda_DU * TI / 2
 * where TI = proof test interval.
 *
 * This is the simplest architecture — for complex architectures use
 * dcs_sif_calculate_pfd() below.
 *
 * @param lambda_du  Dangerous undetected failure rate per hour.
 * @param ti_hours   Proof test interval in hours.
 * @return           PFDavg value.
 */
double dcs_pfd_1oo1(double lambda_du, double ti_hours);

/**
 * @brief Calculate PFDavg for a 1oo2 safety function.
 *
 * PFDavg_1oo2 = (lambda_DU * TI)^2 / 3 + beta * lambda_DU * TI / 2
 * where the first term is the independent failure contribution
 * and the second term is the common cause failure contribution.
 *
 * @param lambda_du  Dangerous undetected failure rate per hour.
 * @param ti_hours   Proof test interval in hours.
 * @param beta       Common cause failure beta factor.
 * @return           PFDavg value.
 */
double dcs_pfd_1oo2(double lambda_du, double ti_hours, double beta);

/**
 * @brief Calculate PFDavg for a 2oo3 (TMR) safety function.
 *
 * PFDavg_2oo3 = (lambda_DU * TI)^2 + beta * lambda_DU * TI / 2
 * Triple Modular Redundancy with 2-out-of-3 voting.
 *
 * @param lambda_du  Dangerous undetected failure rate per hour.
 * @param ti_hours   Proof test interval in hours.
 * @param beta       Common cause failure beta factor.
 * @return           PFDavg value.
 */
double dcs_pfd_2oo3(double lambda_du, double ti_hours, double beta);

/**
 * @brief Calculate PFDavg for a 2oo2 safety function.
 *
 * PFDavg_2oo2 = lambda_DU * TI + beta * lambda_DU * TI / 2
 * Note: 2oo2 has LOWER safety availability than 1oo1 because
 * BOTH channels must work for safety function to succeed.
 * Used when spurious trip prevention is more important than safety.
 *
 * @param lambda_du  Dangerous undetected failure rate per hour.
 * @param ti_hours   Proof test interval in hours.
 * @param beta       Common cause failure beta factor.
 * @return           PFDavg value.
 */
double dcs_pfd_2oo2(double lambda_du, double ti_hours, double beta);

/*===========================================================================
 * L5: Complete SIF PFD Calculation
 *===========================================================================*/

/**
 * @brief Calculate complete SIF PFDavg combining sensor, logic solver,
 *        and final element subsystems.
 *
 * PFD_SIF = PFD_sensor + PFD_logic + PFD_final_element
 *
 * Each subsystem's PFD is calculated based on its voting architecture.
 *
 * @param sensor_comp     Reliability parameters for sensor subsystem.
 * @param sensor_arch     Voting architecture for sensors.
 * @param logic_comp      Reliability parameters for logic solver.
 * @param logic_arch      Voting architecture for logic solver.
 * @param actuator_comp   Reliability parameters for final element.
 * @param actuator_arch   Voting architecture for final element.
 * @param ti_hours        Proof test interval for the SIF.
 * @param beta            Common cause beta factor.
 * @return                Total SIF PFDavg.
 */
double dcs_sif_calculate_pfd(const dcs_safety_component_reliability_t *sensor_comp,
                              dcs_redundancy_arch_t sensor_arch,
                              const dcs_safety_component_reliability_t *logic_comp,
                              dcs_redundancy_arch_t logic_arch,
                              const dcs_safety_component_reliability_t *actuator_comp,
                              dcs_redundancy_arch_t actuator_arch,
                              double ti_hours,
                              double beta);

/*===========================================================================
 * L5: SIL Determination
 *===========================================================================*/

/**
 * @brief Determine achieved SIL based on PFDavg.
 *
 * Per IEC 61508-1 Table 2:
 *   PFDavg < 1e-5 → SIL 4
 *   PFDavg < 1e-4 → SIL 3
 *   PFDavg < 1e-3 → SIL 2
 *   PFDavg < 1e-2 → SIL 1
 *   PFDavg ≥ 1e-2 → None (below SIL 1)
 *
 * Also checks architectural constraints (HFT + SFF).
 *
 * @param pfdavg              Calculated PFDavg.
 * @param hft                 Hardware Fault Tolerance.
 * @param sff                 Safe Failure Fraction.
 * @param component_type      Type A or Type B.
 * @return                    Achieved SIL level.
 */
dcs_sil_level_t dcs_determine_sil(double pfdavg,
                                   dcs_hardware_fault_tolerance_t hft,
                                   double sff,
                                   dcs_component_type_t component_type);

/**
 * @brief Calculate required Risk Reduction Factor (RRF).
 *
 * RRF = tolerable_risk / process_risk
 * Required SIL = floor(log10(RRF)) + 1
 *
 * @param tolerable_risk    Maximum tolerable risk frequency (per year).
 * @param process_risk      Process demand frequency (per year).
 * @return                  Required RRF.
 */
double dcs_calculate_rrf(double tolerable_risk, double process_risk);

/*===========================================================================
 * L6: Classic Problem — Proof Test Interval Optimization
 *===========================================================================*/

/**
 * @brief Calculate optimal proof test interval to achieve target SIL.
 *
 * For a given lambda_DU and target PFDavg, the maximum TI is:
 *   TI_max = 2 * PFDavg_target / lambda_DU  (for 1oo1)
 *
 * @param lambda_du         Dangerous undetected failure rate per hour.
 * @param target_pfd        Target PFDavg (must be ≤ target SIL threshold).
 * @param arch              Voting architecture.
 * @param beta              Common cause beta factor.
 * @return                  Maximum proof test interval in hours.
 */
double dcs_calculate_max_ti(double lambda_du,
                             double target_pfd,
                             dcs_redundancy_arch_t arch,
                             double beta);

/*===========================================================================
 * L6: Verification Function
 *===========================================================================*/

/**
 * @brief Verify that a SIF meets all IEC 61511 requirements.
 *
 * Checks:
 *   1. PFDavg ≤ target SIL threshold
 *   2. Architectural constraints satisfied (HFT + SFF)
 *   3. Proof test interval ≤ mission time
 *   4. CCF beta factor within acceptable range
 *
 * @param sif          The SIF definition to verify.
 * @param sensor       Sensor subsystem reliability.
 * @param logic        Logic solver reliability.
 * @param actuator     Final element reliability.
 * @return             1 if SIF is verified, 0 if requirements not met.
 */
int dcs_sif_verify(dcs_sif_definition_t *sif,
                    const dcs_safety_component_reliability_t *sensor,
                    const dcs_safety_component_reliability_t *logic,
                    const dcs_safety_component_reliability_t *actuator);

#endif /* DCS_SAFETY_H */
