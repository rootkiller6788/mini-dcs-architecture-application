/**
 * @file example_availability_calc.c
 * @brief End-to-End Example: Availability and SIL Calculation for Safety System
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * L6 Canonical Problem: Safety Instrumented System (SIS) availability
 * and SIL verification per IEC 61508/61511.
 *
 * Scenario:
 *   A high-integrity pressure protection system (HIPPS) for an offshore
 *   platform uses 2oo3 voting pressure transmitters, a 1oo2D logic solver,
 *   and 1oo2 final elements (shutdown valves). We compute:
 *     - PFDavg for each subsystem
 *     - Overall system PFDavg and SIL
 *     - Availability and MTBF
 *     - Fault tree for top event
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "availability_model.h"
#include "diagnostic_monitor.h"

int main(void)
{
    printf("\n=============================================\n");
    printf(" HIPPS Availability & SIL Calculation\n");
    printf(" IEC 61508 / IEC 61511 Methodology\n");
    printf("=============================================\n\n");

    /* Subsystem parameters */
    double lambda_du_sensor = 2.5e-7;     /* 2.5e-7/hr per transmitter */
    double lambda_du_logic  = 1.0e-7;     /* 1.0e-7/hr for logic solver */
    double lambda_du_valve  = 5.0e-7;     /* 5.0e-7/hr per valve */
    double t1 = 8760.0;                   /* Proof test interval: 1 year */
    double beta = 0.05;                   /* Common cause factor: 5% */

    printf("=== System Parameters ===\n");
    printf("  Proof test interval: %.0f hours (1 year)\n", t1);
    printf("  Common cause beta:   %.2f\n\n", beta);

    /* 1. Sensors: 2oo3 voting */
    printf("--- Sensor Subsystem (2oo3) ---\n");
    double pfd_single_sensor = availability_pfd_single_channel(lambda_du_sensor, t1);
    double pfd_2oo3 = availability_pfd_2oo3(lambda_du_sensor, t1, beta);

    printf("  Single sensor PFD:         %.2e\n", pfd_single_sensor);
    printf("  2oo3 sensor PFD:           %.2e (SIL %d)\n",
           pfd_2oo3, availability_sil_from_pfd(pfd_2oo3));

    /* Availability per sensor */
    double a_sensor = availability_from_mtbf_mttr(
        availability_mtbf(lambda_du_sensor), 24.0);
    double a_2oo3 = rbd_k_of_n_availability(2, 3, a_sensor);
    printf("  Single sensor availability: %f\n", a_sensor);
    printf("  2oo3 sensor availability:   %f (%d nines)\n\n",
           a_2oo3, availability_nines(a_2oo3));

    /* 2. Logic Solver: 1oo2D */
    printf("--- Logic Solver (1oo2D) ---\n");
    double pfd_single_logic = availability_pfd_single_channel(lambda_du_logic, t1);
    double pfd_1oo2 = availability_pfd_1oo2(lambda_du_logic, t1, beta);

    printf("  Single logic PFD:      %.2e\n", pfd_single_logic);
    printf("  1oo2D logic PFD:       %.2e (SIL %d)\n",
           pfd_1oo2, availability_sil_from_pfd(pfd_1oo2));

    /* Diagnostic coverage */
    double dc_logic = diag_coverage_factor(9e-7, 1e-7);
    printf("  Logic DC:               %.1f%% (%s)\n\n",
           dc_logic * 100.0, diag_coverage_class(dc_logic));

    /* 3. Final Elements: 1oo2 valves */
    printf("--- Final Element Subsystem (1oo2) ---\n");
    double pfd_single_valve = availability_pfd_single_channel(lambda_du_valve, t1);
    double pfd_1oo2_valves = availability_pfd_1oo2(lambda_du_valve, t1, beta);

    printf("  Single valve PFD:       %.2e\n", pfd_single_valve);
    printf("  1oo2 valve PFD:         %.2e (SIL %d)\n\n",
           pfd_1oo2_valves, availability_sil_from_pfd(pfd_1oo2_valves));

    /* Overall system PFD */
    printf("=== Overall HIPPS Assessment ===\n");
    /* For PFD, series approximation: PFD_total ~= PFD_sensor + PFD_logic + PFD_valve */
    double total_pfd_sum = pfd_2oo3 + pfd_1oo2 + pfd_1oo2_valves;
    printf("  Total PFD (sum):     %.2e\n", total_pfd_sum);
    printf("  Overall SIL:         %d\n", availability_sil_from_pfd(total_pfd_sum));
    printf("  RRF:                 %.0f:1\n\n", 1.0 / total_pfd_sum);

    /* Safe Failure Fraction */
    double sff = availability_safe_failure_fraction(1e-7, 5e-7, 5e-7);
    printf("  SFF:                 %.1f%%\n", sff * 100.0);
    int hft = availability_hft_required(availability_sil_from_pfd(total_pfd_sum), sff);
    printf("  HFT required:        %d\n\n", hft);

    /* Fault Tree Analysis */
    printf("=== Fault Tree Analysis ===\n");
    ft_node_t *top = ft_create_gate(FT_OR_GATE, "HIPPS Failure on Demand", 0, 0);

    ft_node_t *sensor_gate = ft_create_gate(FT_VOTING_OR_K_OF_N, "Sensor Failure (2oo3)", 2, 3);
    ft_node_t *s1 = ft_create_basic_event("PT-A Fail DU", pfd_single_sensor);
    ft_node_t *s2 = ft_create_basic_event("PT-B Fail DU", pfd_single_sensor);
    ft_node_t *s3 = ft_create_basic_event("PT-C Fail DU", pfd_single_sensor);
    ft_add_child(sensor_gate, s1);
    ft_add_child(sensor_gate, s2);
    ft_add_child(sensor_gate, s3);

    ft_node_t *logic_gate = ft_create_gate(FT_AND_GATE, "Logic Solver Fail (1oo2D)", 0, 0);
    ft_node_t *l1 = ft_create_basic_event("CPU-A Fail", pfd_single_logic);
    ft_node_t *l2 = ft_create_basic_event("CPU-B Fail", pfd_single_logic);
    ft_add_child(logic_gate, l1);
    ft_add_child(logic_gate, l2);

    ft_node_t *valve_gate = ft_create_gate(FT_AND_GATE, "Valve Fail (1oo2)", 0, 0);
    ft_node_t *v1 = ft_create_basic_event("SDV-1 Fail", pfd_single_valve);
    ft_node_t *v2 = ft_create_basic_event("SDV-2 Fail", pfd_single_valve);
    ft_add_child(valve_gate, v1);
    ft_add_child(valve_gate, v2);

    ft_add_child(top, sensor_gate);
    ft_add_child(top, logic_gate);
    ft_add_child(top, valve_gate);

    double ft_result = ft_evaluate(top);
    printf("  Fault tree top event PFD: %.2e\n", ft_result);
    printf("  FTA SIL:                  %d\n", availability_sil_from_pfd(ft_result));

    ft_free_tree(top);

    printf("\n=============================================\n");
    printf(" HIPPS SIL Verification Complete.\n");
    printf("=============================================\n\n");
    return 0;
}
