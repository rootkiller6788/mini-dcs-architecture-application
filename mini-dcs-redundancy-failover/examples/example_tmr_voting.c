/**
 * @file example_tmr_voting.c
 * @brief End-to-End Example: Triple Modular Redundancy (TMR) Sensor Voting
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * L6 Canonical Problem: Triple-redundant sensor voting with fault injection.
 *
 * Scenario:
 *   A critical reactor pressure measurement uses three redundant pressure
 *   transmitters (PT-101A, PT-101B, PT-101C). The 2oo3 voting mechanism
 *   selects the correct measurement. We inject faults into one sensor
 *   and verify that the voting mechanism correctly rejects the faulty
 *   reading.
 *
 *   This demonstrates von Neumann's TMR theorem:
 *     R_TMR = 3R^2 - 2R^3 > R for R > 0.5
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "voting_mechanism.h"

int main(void)
{
    printf("\n=============================================\n");
    printf(" TMR Sensor Voting Example\n");
    printf("=============================================\n\n");

    /* Configure 2oo3 voter with 5% discrepancy threshold */
    voter_t tmr_voter;
    voting_init(&tmr_voter, VOTING_ALGORITHM_2OO3, 0.05);

    /* Test Case 1: Normal operation — all sensors agree */
    printf("=== Test 1: Normal Operation ===\n");
    printf("  PT-101A: 100.0 bar\n");
    printf("  PT-101B: 101.0 bar\n");
    printf("  PT-101C:  99.5 bar\n");

    voting_set_input(&tmr_voter, 0, 100.0, 1.0, 0);
    voting_set_input(&tmr_voter, 1, 101.0, 1.0, 1);
    voting_set_input(&tmr_voter, 2, 99.5, 1.0, 2);

    voting_result_t r = voting_execute(&tmr_voter);
    printf("  Voter result: %s\n", (r == VOTING_RESULT_OK) ? "OK" : "DISCREPANCY");
    printf("  Selected value: %.1f bar\n\n", tmr_voter.selected_value);

    /* Test Case 2: PT-101B fails high (stuck at 150 bar) */
    printf("=== Test 2: Sensor Fault — PT-101B Stuck High ===\n");
    printf("  PT-101A: 100.0 bar\n");
    printf("  PT-101B: 150.0 bar (FAULT — stuck high)\n");
    printf("  PT-101C:  99.5 bar\n");

    voting_set_input(&tmr_voter, 0, 100.0, 1.0, 0);
    voting_set_input(&tmr_voter, 1, 150.0, 1.0, 1);
    voting_set_input(&tmr_voter, 2, 99.5, 1.0, 2);

    r = voting_execute(&tmr_voter);
    double correct = 100.0;
    double error = fabs(tmr_voter.selected_value - correct) / correct * 100.0;
    printf("  Voter result: %s\n", (r == VOTING_RESULT_OK) ? "OK" : "DISCREPANCY");
    printf("  Selected value: %.1f bar (error: %.1f%% vs correct)\n\n",
           tmr_voter.selected_value, error);

    /* Test Case 3: Two sensors fail — no majority possible */
    printf("=== Test 3: Double Fault — No Majority ===\n");
    printf("  PT-101A: 100.0 bar\n");
    printf("  PT-101B: 150.0 bar (FAULT)\n");
    printf("  PT-101C:  50.0 bar (FAULT)\n");

    voting_set_input(&tmr_voter, 0, 100.0, 1.0, 0);
    voting_set_input(&tmr_voter, 1, 150.0, 1.0, 1);
    voting_set_input(&tmr_voter, 2, 50.0, 1.0, 2);

    r = voting_execute(&tmr_voter);
    printf("  Voter result: %s (expected: DISCREPANCY)\n",
           (r == VOTING_RESULT_OK) ? "OK" : "DISCREPANCY");
    printf("  Consensus impossible with 2 faults out of 3.\n\n");

    /* Test Case 4: Weighted voting — calibrated sensor higher weight */
    printf("=== Test 4: Weighted Voting (Calibrated Sensor) ===\n");
    voter_t wv;
    voting_init(&wv, VOTING_ALGORITHM_WEIGHTED, 0.05);

    voting_set_input(&wv, 0, 100.0, 1.0, 0);   /* Standard */
    voting_set_input(&wv, 1, 102.0, 3.0, 1);   /* Calibrated, weight 3x */
    voting_set_input(&wv, 2, 101.0, 1.0, 2);   /* Standard */

    r = voting_execute(&wv);
    printf("  Voter result: %s\n", (r == VOTING_RESULT_OK) ? "OK" : "DISCREPANCY");
    printf("  Weighted value: %.1f bar (biased toward calibrated sensor)\n\n",
           wv.selected_value);

    /* Test Case 5: Median voting with 5 sensors */
    printf("=== Test 5: Median Voting (5 Sensors) ===\n");
    double inputs[] = {100.0, 101.0, 150.0, 99.0, 102.0};
    bool valid[] = {true, true, true, true, true};
    double median_result;
    r = voting_median(inputs, valid, 5, &median_result);
    printf("  Inputs: {100, 101, 150, 99, 102}\n");
    printf("  Median: %.1f bar (robust to 150 outlier)\n\n", median_result);

    /* Reliability analysis */
    printf("=== Reliability Analysis ===\n");
    double r_single = 0.95;  /* Single sensor reliability */
    double r_tmr = 3.0 * r_single * r_single - 2.0 * r_single * r_single * r_single;
    printf("  Single sensor reliability: %.4f\n", r_single);
    printf("  TMR system reliability:    %.4f\n", r_tmr);
    printf("  Improvement:               %.1f%%\n",
           (r_tmr - r_single) / r_single * 100.0);
    printf("  Von Neumann threshold:     R > 0.5 for TMR benefit.\n");

    printf("\n=============================================\n");
    printf(" TMR Voting Example Complete.\n");
    printf("=============================================\n\n");
    return 0;
}
