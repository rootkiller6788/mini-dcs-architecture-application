/**
 * @file voting_mechanism.h
 * @brief Voting Mechanisms for Redundant Signal Selection
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover (7. mini-dcs-architecture-application)
 *
 * Knowledge Coverage:
 *   L5 - Algorithms: 2oo3 majority voting, weighted voting, median voting
 *   L2 - Core concepts: signal selection, discrepancy detection, Byzantine tolerance
 *   L3 - Engineering structures: voter data structures
 *
 * Reference:
 *   - von Neumann, "Probabilistic Logics and the Synthesis of Reliable
 *     Organisms from Unreliable Components" (1956) -- TMR foundation
 *   - Lamport, Shostak, Pease, "The Byzantine Generals Problem" (1982)
 *   - IEC 61508-6 Annex B -- voting architectures for safety
 */

#ifndef VOTING_MECHANISM_H
#define VOTING_MECHANISM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VOTING_MAX_INPUTS 8
#define VOTING_MAX_DISCREPANCY 0.10

typedef enum {
    VOTING_ALGORITHM_MAJORITY   = 0,
    VOTING_ALGORITHM_WEIGHTED   = 1,
    VOTING_ALGORITHM_MEDIAN     = 2,
    VOTING_ALGORITHM_MIDVALUE   = 3,
    VOTING_ALGORITHM_AVERAGE    = 4,
    VOTING_ALGORITHM_HIGH_SELECT = 5,
    VOTING_ALGORITHM_LOW_SELECT  = 6,
    VOTING_ALGORITHM_2OO3       = 7,
    VOTING_ALGORITHM_2OO4       = 8,
    VOTING_ALGORITHM_BYZANTINE  = 9
} voting_algorithm_t;

typedef enum {
    VOTING_RESULT_OK        = 0,
    VOTING_RESULT_DISCREPANCY = 1,
    VOTING_RESULT_NO_MAJORITY = 2,
    VOTING_RESULT_INSUFFICIENT_INPUTS = 3,
    VOTING_RESULT_BYZANTINE_FAULT = 4
} voting_result_t;

typedef struct {
    double value;
    bool   valid;
    double weight;
    uint8_t source_id;
} voting_input_t;

typedef struct {
    voting_algorithm_t algorithm;
    uint8_t      n_inputs;
    voting_input_t inputs[VOTING_MAX_INPUTS];
    double       discrepancy_threshold;
    double       selected_value;
    voting_result_t result;
    uint8_t      majority_count;
    uint8_t      inputs_in_agreement;
    bool         byzantine_mode;
} voter_t;

/**
 * Initialize a voter with a given algorithm.
 * @param voter      Voter to initialize
 * @param algorithm  Voting algorithm type
 * @param threshold  Discrepancy threshold (fraction 0.0-1.0)
 * @return           0 on success, -1 on error
 * Complexity: O(1)
 */
int voting_init(voter_t *voter, voting_algorithm_t algorithm, double threshold);

/**
 * Set an input value for the voter.
 * @param voter     Voter to update
 * @param index     Input index 0..VOTING_MAX_INPUTS-1
 * @param value     Signal value
 * @param weight    Voting weight (for weighted algorithms)
 * @param source_id Identifier for the source module
 * @return          0 on success, -1 on error
 * Complexity: O(1)
 */
int voting_set_input(voter_t *voter, uint8_t index,
                     double value, double weight, uint8_t source_id);

/**
 * Mark an input as invalid (e.g., sensor fault detected).
 * @param voter  Voter
 * @param index  Input index
 * @return       0 on success
 * Complexity: O(1)
 */
int voting_invalidate_input(voter_t *voter, uint8_t index);

/**
 * Execute the voting algorithm and produce a selected value.
 * @param voter Voter with inputs set
 * @return      Voting result status
 * Complexity: O(N log N) for median, O(N) otherwise
 */
voting_result_t voting_execute(voter_t *voter);

/**
 * 2oo3 majority voting: select the value that falls between the other two
 * when sorted, if all three are within discrepancy threshold.
 *
 * Theorem: With at most 1 Byzantine fault, 2oo3 voting selects the correct
 * value provided 2 of 3 inputs are correct.
 *
 * @param a First value
 * @param b Second value
 * @param c Third value
 * @param threshold Discrepancy threshold
 * @param result Output selected value
 * @return VOTING_RESULT_OK if majority found
 * Complexity: O(1)
 */
voting_result_t voting_2oo3(double a, double b, double c,
                            double threshold, double *result);

/**
 * 2oo4 voting: select the average of the two middle values from four inputs
 * (quad-redundant mid-value selection).
 *
 * @param inputs Array of 4 input values
 * @param n      Number of inputs (should be 4)
 * @param threshold Discrepancy threshold
 * @param result Output selected value
 * @return Voting result status
 * Complexity: O(1) -- fixed 4 inputs
 */
voting_result_t voting_2oo4(const double inputs[], size_t n,
                            double threshold, double *result);

/**
 * Median voting: select the median value from all valid inputs.
 * Robust to outliers; used when no assumption about fault direction.
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Number of inputs
 * @param result Output selected median value
 * @return Voting result status
 * Complexity: O(N log N) due to sorting
 */
voting_result_t voting_median(const double inputs[], const bool valid[],
                              size_t n, double *result);

/**
 * Weighted voting: each input has a configurable weight.
 * The output is the weighted average, with inputs outside the
 * discrepancy band excluded from calculation.
 *
 * @param voter Initialized voter with weighted inputs
 * @return Voting result status
 * Complexity: O(N)
 */
voting_result_t voting_weighted(voter_t *voter);

/**
 * Mid-value selection: from N sorted values, select the value at
 * position floor(N/2). Used in TMR and quad-redundant systems.
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Number of inputs
 * @param result Output selected mid-value
 * @return Voting result status
 * Complexity: O(N log N)
 */
voting_result_t voting_midvalue(const double inputs[], const bool valid[],
                                size_t n, double *result);

/**
 * Average voting with deviation check: compute the mean of all inputs
 * that are within discrepancy_threshold of the median.
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Number of inputs
 * @param threshold Discrepancy threshold
 * @param result Output selected average value
 * @return Voting result status
 * Complexity: O(N log N)
 */
voting_result_t voting_average_with_check(const double inputs[],
                                           const bool valid[],
                                           size_t n, double threshold,
                                           double *result);

/**
 * High-select voting: select the highest value among valid inputs.
 * Used for safety-critical high-limit detection (e.g., over-pressure).
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Number of inputs
 * @param result Output maximum valid value
 * @return Voting result status
 * Complexity: O(N)
 */
voting_result_t voting_high_select(const double inputs[], const bool valid[],
                                   size_t n, double *result);

/**
 * Low-select voting: select the lowest value among valid inputs.
 * Used for safety-critical low-limit detection (e.g., low-level trip).
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Number of inputs
 * @param result Output minimum valid value
 * @return Voting result status
 * Complexity: O(N)
 */
voting_result_t voting_low_select(const double inputs[], const bool valid[],
                                  size_t n, double *result);

/**
 * Check whether a set of inputs forms a valid majority.
 * For N inputs, a majority requires at least floor(N/2)+1 inputs
 * to agree within the discrepancy threshold.
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Number of inputs
 * @param threshold Discrepancy threshold
 * @return       true if majority exists
 * Complexity: O(N^2)
 */
bool voting_has_majority(const double inputs[], const bool valid[],
                         size_t n, double threshold);

/**
 * Compute the discrepancy ratio between two values.
 * discrepancy = |a - b| / max(|a|, |b|, epsilon)
 *
 * @param a First value
 * @param b Second value
 * @return  Discrepancy ratio in [0, 1]
 * Complexity: O(1)
 */
double voting_discrepancy(double a, double b);

/**
 * Byzantine-resilient voting using the median of the values that are
 * within the discrepancy threshold. Requires at least 3f+1 inputs to
 * tolerate f Byzantine faults (Lamport et al., 1982).
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Total number of inputs (>= 3f+1 for f faults)
 * @param threshold Discrepancy threshold
 * @param result Output Byzantine-resilient value
 * @return Voting result status
 * Complexity: O(N^2)
 */
voting_result_t voting_byzantine_resilient(const double inputs[],
                                            const bool valid[],
                                            size_t n, double threshold,
                                            double *result);

/**
 * Count the number of inputs that agree with a reference value
 * within the discrepancy threshold.
 *
 * @param inputs    Array of input values
 * @param valid     Array of validity flags
 * @param n         Number of inputs
 * @param reference Reference value
 * @param threshold Discrepancy threshold
 * @return          Number of agreeing inputs
 * Complexity: O(N)
 */
uint8_t voting_agreement_count(const double inputs[], const bool valid[],
                               size_t n, double reference, double threshold);

/**
 * Compute the standard deviation of valid inputs for diagnostic purposes.
 *
 * @param inputs Array of input values
 * @param valid  Array of validity flags
 * @param n      Number of inputs
 * @return       Standard deviation, or 0.0 if <2 valid inputs
 * Complexity: O(N)
 */
double voting_input_stddev(const double inputs[], const bool valid[],
                           size_t n);

#endif /* VOTING_MECHANISM_H */
