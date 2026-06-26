/**
 * @file example_safety_sif_calculation.c
 * @brief L6 Canonical Problem: Safety Instrumented Function (SIF) design
 *        per IEC 61508/61511 for a high-pressure reactor protection system.
 *
 * This example walks through a complete SIL verification for a reactor
 * overpressure protection SIF, modeled after:
 *   - Reactor high-pressure trip at a petrochemical plant
 *   - Sensor: 2oo3 pressure transmitters
 *   - Logic Solver: 1oo1 safety PLC (SIL 3 certified)
 *   - Final Element: 1oo2 shutdown valves
 */

#include "dcs_safety.h"
#include "dcs_redundancy.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void)
{
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  IEC 61508/61511 — Reactor Overpressure SIF Design  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Hazard scenario:
     *   - Reactor R-101: exothermic polymerization
     *   - Hazard: runaway reaction → overpressure → vessel rupture
     *   - Demand rate (without protection): 1 per 10 years = 0.1/year
     */

    printf("Hazard Scenario: Exothermic Reactor Overpressure\n");
    printf("  Process:           Exothermic polymerization\n");
    printf("  Demand rate:       0.1 per year (once per 10 years)\n");
    printf("  Tolerable risk:    1.0e-5 per year (1 per 100,000 years)\n\n");

    double process_risk = 0.1;       /* 1 per 10 years */
    double tolerable_risk = 1.0e-5;  /* 1 per 100,000 years */

    /* Step 1: Calculate required RRF */
    double rrf_required = dcs_calculate_rrf(tolerable_risk, process_risk);

    printf("Step 1 — Risk Reduction Requirement:\n");
    printf("  Required RRF:       %.0f\n", rrf_required);
    printf("  Required SIL:       ");
    if (rrf_required > 10000.0) printf("SIL 4\n");
    else if (rrf_required > 1000.0) printf("SIL 3\n");
    else if (rrf_required > 100.0) printf("SIL 2\n");
    else if (rrf_required > 10.0) printf("SIL 1\n");
    else printf("Below SIL 1\n");
    printf("  Equivalent PFDavg:  %.1e\n\n", 1.0 / rrf_required);

    /* Step 2: Define safety components */
    dcs_safety_component_reliability_t sensor, logic, actuator;

    /* Sensor: 2oo3 pressure transmitters (Type B, smart transmitter)
     *   lambda_DU = 1.0e-6/hr (typical for smart transmitters)
     *   DC = 90%, SFF = 95%
     */
    memset(&sensor, 0, sizeof(sensor));
    sensor.lambda_du_per_hour = 1.0e-6;
    sensor.diagnostic_coverage = 0.90;
    sensor.safe_failure_fraction = 0.95;
    sensor.proof_test_coverage = 0.95;
    sensor.mttr_hours = 8.0;
    sensor.common_cause_beta = 0.02;   /* 2% CCF for diverse sensors */

    /* Logic Solver: SIL 3 certified safety PLC (Type B)
     *   lambda_DU = 5.0e-8/hr (certified for SIL 3)
     *   DC = 99%, SFF = 99%, HFT = 0
     */
    memset(&logic, 0, sizeof(logic));
    logic.lambda_du_per_hour = 5.0e-8;
    logic.diagnostic_coverage = 0.99;
    logic.safe_failure_fraction = 0.99;
    logic.proof_test_coverage = 0.99;
    logic.mttr_hours = 8.0;
    logic.common_cause_beta = 0.01;

    /* Final Element: 1oo2 shutdown valves (Type A, ball valve + actuator)
     *   lambda_DU = 2.0e-6/hr (valve + actuator + solenoid)
     *   DC = 70%, SFF = 80%, HFT = 1 (1oo2)
     */
    memset(&actuator, 0, sizeof(actuator));
    actuator.lambda_du_per_hour = 2.0e-6;
    actuator.diagnostic_coverage = 0.70;
    actuator.safe_failure_fraction = 0.80;
    actuator.proof_test_coverage = 0.90;
    actuator.mttr_hours = 12.0;
    actuator.common_cause_beta = 0.05;  /* 5% CCF for identical valves */

    printf("Step 2 — Component Reliability Parameters:\n");
    printf("  Sensor (2oo3):   λ_DU=%.1e/hr, SFF=%.0f%%, β=%.0f%%\n",
           sensor.lambda_du_per_hour, sensor.safe_failure_fraction * 100.0,
           sensor.common_cause_beta * 100.0);
    printf("  Logic  (1oo1):   λ_DU=%.1e/hr, SFF=%.0f%%, β=%.0f%%\n",
           logic.lambda_du_per_hour, logic.safe_failure_fraction * 100.0,
           logic.common_cause_beta * 100.0);
    printf("  Valve  (1oo2):   λ_DU=%.1e/hr, SFF=%.0f%%, β=%.0f%%\n\n",
           actuator.lambda_du_per_hour, actuator.safe_failure_fraction * 100.0,
           actuator.common_cause_beta * 100.0);

    /* Step 3: Vary proof test interval and compute PFD */
    printf("Step 3 — PFDavg vs. Proof Test Interval:\n");
    printf("  %-12s  %-12s  %-12s  %-12s  %-12s  %-12s\n",
           "TI (months)", "Sensor PFD", "Logic PFD", "Valve PFD",
           "Total PFD", "Achieved SIL");
    printf("  %-12s  %-12s  %-12s  %-12s  %-12s  %-12s\n",
           "────────────", "──────────", "──────────", "──────────",
           "──────────", "────────────");

    double ti_months[] = {3, 6, 12, 24, 36, 60};
    for (int i = 0; i < 6; i++) {
        double ti_hours = ti_months[i] * 30.0 * 24.0;
        double beta_max = 0.05;  /* Max of all CCF factors */

        double pfd = dcs_sif_calculate_pfd(
            &sensor, DCS_REDUNDANCY_2OO3,
            &logic,  DCS_REDUNDANCY_1OO1,
            &actuator, DCS_REDUNDANCY_1OO2,
            ti_hours, beta_max);

        dcs_sil_level_t sil = DCS_SIL_NONE;
        if (pfd < 1e-5) sil = DCS_SIL_4;
        else if (pfd < 1e-4) sil = DCS_SIL_3;
        else if (pfd < 1e-3) sil = DCS_SIL_2;
        else if (pfd < 1e-2) sil = DCS_SIL_1;

        printf("  %-12.0f  %-12.2e  %-12.2e  %-12.2e  %-12.2e  SIL %-8d\n",
               ti_months[i],
               dcs_pfd_2oo3(sensor.lambda_du_per_hour, ti_hours, 0.02),
               dcs_pfd_1oo1(logic.lambda_du_per_hour, ti_hours),
               dcs_pfd_1oo2(actuator.lambda_du_per_hour, ti_hours, 0.05),
               pfd, (int)sil);
    }

    /* Step 4: Optimal proof test interval for target SIL 3 */
    double target_pfd_sil3 = 1.0e-4;
    double max_ti = dcs_calculate_max_ti(
        actuator.lambda_du_per_hour, target_pfd_sil3 / 2.0,
        DCS_REDUNDANCY_1OO2, 0.05);

    printf("\nStep 4 — Optimal Proof Test Interval for SIL 3:\n");
    printf("  Target PFDavg:      ≤ %.1e\n", target_pfd_sil3);
    printf("  Max TI (1oo2 valve): %.0f hours (%.0f months)\n",
           max_ti, max_ti / (30.0 * 24.0));
    printf("  Recommended TI:     12 months (standard maintenance cycle)\n");

    /* Step 5: SIF Verification */
    dcs_sif_definition_t sif;
    memset(&sif, 0, sizeof(sif));
    sif.sif_id = 1;
    snprintf(sif.sif_name, 64, "R-101 Overpressure Protection");
    snprintf(sif.hazard_description, 128,
             "Reactor runaway overpressure → vessel rupture");
    sif.target_sil = DCS_SIL_3;
    sif.required_rrf = rrf_required;
    sif.sensor_arch = DCS_REDUNDANCY_2OO3;
    sif.logic_arch = DCS_REDUNDANCY_1OO1;
    sif.actuator_arch = DCS_REDUNDANCY_1OO2;
    sif.proof_test_interval_hours = 8760.0;  /* 12 months */

    int verified = dcs_sif_verify(&sif, &sensor, &logic, &actuator);

    printf("\nStep 5 — Complete SIF Verification:\n");
    printf("  PFDavg:             %.2e\n", sif.pfd_avg);
    printf("  Achieved SIL:       %d\n", (int)sif.achieved_sil);
    printf("  Target SIL:         %d\n", (int)sif.target_sil);
    printf("  Achieved RRF:       %.0f\n", sif.achieved_rrf);
    printf("  Required RRF:       %.0f\n", sif.required_rrf);
    printf("  Verification:       %s\n",
           verified ? "PASSED ✓" : "FAILED ✗");
    printf("  Status:             %s\n",
           sif.is_verified ? "SIF MEETS ALL IEC 61511 REQUIREMENTS"
                           : "SIF DOES NOT MEET REQUIREMENTS");

    /* Step 6: Architectural constraint check */
    dcs_sil_level_t sil_by_arch = dcs_determine_sil(
        sif.pfd_avg, DCS_HFT_1, actuator.safe_failure_fraction * 100.0,
        DCS_COMPONENT_TYPE_A);

    printf("\nStep 6 — Architectural Constraint Analysis:\n");
    printf("  HFT (1oo2 valve):    1\n");
    printf("  SFF (valve):         %.0f%%\n",
           actuator.safe_failure_fraction * 100.0);
    printf("  Component Type:      A (valves, relays)\n");
    printf("  Max SIL by Arch:     %d\n", (int)sil_by_arch);
    printf("  Constraint met:      %s\n\n",
           sil_by_arch >= DCS_SIL_3 ? "YES ✓" : "NO ✗");

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  SIF Design Complete — Ready for SRS Documentation  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
