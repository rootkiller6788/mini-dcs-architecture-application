/**
 * @file voting_mechanism.c
 * @brief Voting Mechanism Implementations for Redundant Signals
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * Knowledge Coverage:
 *   L5 - Voting algorithms: 2oo3, 2oo4, median, weighted, Byzantine
 *   L2 - Signal selection, discrepancy detection
 *   L3 - Voter data structures
 *
 * Reference:
 *   von Neumann (1956) -- TMR probabilistic foundations
 *   Lamport, Shostak, Pease (1982) -- Byzantine Generals Problem
 *   IEC 61508-6 Annex B -- Voting architectures for safety applications
 */

#include "voting_mechanism.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ============================================================================
 * L5: Voter Initialization
 * ============================================================================
 */

int voting_init(voter_t *voter, voting_algorithm_t algorithm, double threshold)
{
    if (!voter) return -1;
    if (threshold < 0.0 || threshold > 1.0) return -1;

    memset(voter, 0, sizeof(*voter));
    voter->algorithm = algorithm;
    voter->discrepancy_threshold = threshold;
    voter->n_inputs = 0;
    voter->result = VOTING_RESULT_OK;

    for (uint8_t i = 0; i < VOTING_MAX_INPUTS; i++) {
        voter->inputs[i].valid = false;
        voter->inputs[i].weight = 1.0;
    }

    return 0;
}

int voting_set_input(voter_t *voter, uint8_t index,
                     double value, double weight, uint8_t source_id)
{
    if (!voter) return -1;
    if (index >= VOTING_MAX_INPUTS) return -1;

    voting_input_t *inp = &voter->inputs[index];
    inp->value = value;
    inp->valid = true;
    inp->weight = (weight > 0.0) ? weight : 1.0;
    inp->source_id = source_id;

    if (index >= voter->n_inputs) {
        voter->n_inputs = index + 1;
    }

    return 0;
}

int voting_invalidate_input(voter_t *voter, uint8_t index)
{
    if (!voter) return -1;
    if (index >= VOTING_MAX_INPUTS) return -1;
    voter->inputs[index].valid = false;
    return 0;
}

/* ============================================================================
 * L5: 2oo3 Majority Voting
 * ============================================================================
 * Knowledge: In a Triple Modular Redundant (TMR) system, three identical
 * modules compute the same function. The output is determined by majority
 * vote. If one module produces a faulty output, the other two outvote it.
 *
 * Algorithm (2oo3 with discrepancy check):
 *   1. Sort the three values: lo, mid, hi
 *   2. Check if |mid - lo| <= threshold AND |hi - mid| <= threshold
 *      - If both OK, select mid (all three agree broadly)
 *   3. If only one pair agrees (within threshold), select the pair's average
 *   4. If no pair agrees, return NO_MAJORITY
 *
 * Theorem (von Neumann): With perfect voting, TMR reliability
 *   R_TMR = 3R^2 - 2R^3 > R for R > 0.5
 */

voting_result_t voting_2oo3(double a, double b, double c,
                            double threshold, double *result)
{
    if (!result) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    /* Sort three values: bubble sort 3 elements */
    double vals[3] = {a, b, c};
    if (vals[0] > vals[1]) { double t = vals[0]; vals[0] = vals[1]; vals[1] = t; }
    if (vals[1] > vals[2]) { double t = vals[1]; vals[1] = vals[2]; vals[2] = t; }
    if (vals[0] > vals[1]) { double t = vals[0]; vals[0] = vals[1]; vals[1] = t; }

    double lo = vals[0], mid = vals[1], hi = vals[2];

    double d_lo_mid = fabs(mid - lo);
    double d_mid_hi = fabs(hi - mid);
    double scale = fmax(fmax(fabs(lo), fabs(mid)), fabs(hi));
    if (scale < 1e-10) scale = 1.0;

    if (d_lo_mid <= threshold * scale && d_mid_hi <= threshold * scale) {
        /* All three agree within threshold: select mid value */
        *result = mid;
        return VOTING_RESULT_OK;
    }

    /* Check if two values agree (either lo-mid or mid-hi) */
    if (d_lo_mid <= threshold * scale) {
        *result = (lo + mid) / 2.0;
        return VOTING_RESULT_OK;
    }
    if (d_mid_hi <= threshold * scale) {
        *result = (mid + hi) / 2.0;
        return VOTING_RESULT_OK;
    }

    *result = mid;  /* Default to median despite discrepancy */
    return VOTING_RESULT_DISCREPANCY;
}

/* ============================================================================
 * L5: 2oo4 Voting — Quad-Redundant Mid-Value Selection
 * ============================================================================
 * Knowledge: 2oo4 (two-out-of-four) voting is used in quad-redundant
 * systems for higher availability than TMR. The algorithm selects
 * the average of the two middle values from four sorted inputs.
 *
 * 2oo4 provides tolerance to 2 failures (fail-safe) while maintaining
 * higher availability than 2oo3 during normal operation.
 */

voting_result_t voting_2oo4(const double inputs[], size_t n,
                            double threshold, double *result)
{
    if (!inputs || !result || n < 4) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    /* Copy and sort the 4 values */
    double vals[4];
    memcpy(vals, inputs, 4 * sizeof(double));

    /* Insertion sort for 4 elements */
    for (int i = 1; i < 4; i++) {
        double key = vals[i];
        int j = i - 1;
        while (j >= 0 && vals[j] > key) {
            vals[j + 1] = vals[j];
            j--;
        }
        vals[j + 1] = key;
    }

    double scale = fmax(fmax(fabs(vals[0]), fabs(vals[3])), 1.0);

    /* Check if values 1 and 2 (the middle two) agree with value 0 and 3 */
    if (fabs(vals[1] - vals[0]) <= threshold * scale
        && fabs(vals[2] - vals[1]) <= threshold * scale
        && fabs(vals[3] - vals[2]) <= threshold * scale) {
        /* All 4 agree: select average of middle two */
        *result = (vals[1] + vals[2]) / 2.0;
        return VOTING_RESULT_OK;
    }

    /* If three of four agree (exclude the outlier) */
    /* Check: exclude vals[0] */
    if (fabs(vals[2] - vals[1]) <= threshold * scale
        && fabs(vals[3] - vals[2]) <= threshold * scale) {
        *result = (vals[1] + vals[2]) / 2.0;
        return VOTING_RESULT_OK;
    }
    /* Check: exclude vals[3] */
    if (fabs(vals[1] - vals[0]) <= threshold * scale
        && fabs(vals[2] - vals[1]) <= threshold * scale) {
        *result = (vals[1] + vals[2]) / 2.0;
        return VOTING_RESULT_OK;
    }

    *result = (vals[1] + vals[2]) / 2.0;
    return VOTING_RESULT_DISCREPANCY;
}

/* ============================================================================
 * L5: Median Voting
 * ============================================================================
 * Knowledge: Median voting selects the middle value from a sorted set
 * of inputs. It is robust to outliers because the median is unaffected
 * by extreme values. Used when the fault direction (high or low) is
 * unknown or when inputs come from diverse sensor types.
 */

static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

voting_result_t voting_median(const double inputs[], const bool valid[],
                              size_t n, double *result)
{
    if (!inputs || !valid || !result || n == 0)
        return VOTING_RESULT_INSUFFICIENT_INPUTS;

    /* Collect valid inputs */
    double valid_vals[VOTING_MAX_INPUTS];
    size_t count = 0;
    for (size_t i = 0; i < n && i < VOTING_MAX_INPUTS; i++) {
        if (valid[i]) {
            valid_vals[count++] = inputs[i];
        }
    }

    if (count == 0) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    /* Sort and select median */
    qsort(valid_vals, count, sizeof(double), compare_double);
    *result = valid_vals[count / 2];
    return VOTING_RESULT_OK;
}

/* ============================================================================
 * L5: Weighted Voting
 * ============================================================================
 * Knowledge: Weighted voting assigns different weights to each input,
 * reflecting confidence in the source (e.g., higher weight for
 * calibrated sensors). The output is the weighted average of inputs
 * that agree within the discrepancy threshold.
 *
 * Weighted average: output = sum(w_i * v_i) / sum(w_i)
 *   for all i where |v_i - median| <= threshold * scale
 */

voting_result_t voting_weighted(voter_t *voter)
{
    if (!voter) return VOTING_RESULT_INSUFFICIENT_INPUTS;
    if (voter->n_inputs < 1) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    /* First, find median to use as reference */
    double valid_vals[VOTING_MAX_INPUTS];
    bool all_valid[VOTING_MAX_INPUTS];
    size_t count = 0;

    for (uint8_t i = 0; i < voter->n_inputs && i < VOTING_MAX_INPUTS; i++) {
        if (voter->inputs[i].valid) {
            valid_vals[count] = voter->inputs[i].value;
            all_valid[count] = true;
            count++;
        }
    }

    if (count < 1) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    double median;
    (void)voting_median(valid_vals, all_valid, count, &median);

    /* Weighted average of inputs within threshold of median */
    double scale = fmax(fabs(median), 1.0);
    double weight_sum = 0.0;
    double value_sum = 0.0;
    uint8_t in_agreement = 0;

    for (uint8_t i = 0; i < voter->n_inputs && i < VOTING_MAX_INPUTS; i++) {
        if (!voter->inputs[i].valid) continue;
        if (fabs(voter->inputs[i].value - median)
            <= voter->discrepancy_threshold * scale) {
            double w = voter->inputs[i].weight;
            value_sum += w * voter->inputs[i].value;
            weight_sum += w;
            in_agreement++;
        }
    }

    if (in_agreement == 0 || weight_sum <= 0.0) {
        voter->selected_value = median;
        voter->result = VOTING_RESULT_DISCREPANCY;
        return VOTING_RESULT_DISCREPANCY;
    }

    voter->selected_value = value_sum / weight_sum;
    voter->majority_count = in_agreement;
    voter->result = VOTING_RESULT_OK;
    return VOTING_RESULT_OK;
}

/* ============================================================================
 * L5: Mid-Value Selection
 * ============================================================================
 */

voting_result_t voting_midvalue(const double inputs[], const bool valid[],
                                size_t n, double *result)
{
    if (!inputs || !valid || !result || n == 0)
        return VOTING_RESULT_INSUFFICIENT_INPUTS;

    double valid_vals[VOTING_MAX_INPUTS];
    size_t count = 0;
    for (size_t i = 0; i < n && i < VOTING_MAX_INPUTS; i++) {
        if (valid[i]) valid_vals[count++] = inputs[i];
    }

    if (count == 0) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    qsort(valid_vals, count, sizeof(double), compare_double);

    if (count == 1) {
        *result = valid_vals[0];
    } else if (count % 2 == 1) {
        *result = valid_vals[count / 2];
    } else {
        *result = (valid_vals[count / 2 - 1] + valid_vals[count / 2]) / 2.0;
    }

    return VOTING_RESULT_OK;
}

/* ============================================================================
 * L5: Average Voting with Deviation Check
 * ============================================================================
 */

voting_result_t voting_average_with_check(const double inputs[],
                                           const bool valid[],
                                           size_t n, double threshold,
                                           double *result)
{
    if (!inputs || !valid || !result || n == 0)
        return VOTING_RESULT_INSUFFICIENT_INPUTS;

    /* First compute median as reference */
    double median;
    voting_result_t mr = voting_median(inputs, valid, n, &median);
    if (mr != VOTING_RESULT_OK) return mr;

    /* Average inputs that agree with median within threshold */
    double scale = fmax(fabs(median), 1.0);
    double sum = 0.0;
    size_t count = 0;

    for (size_t i = 0; i < n; i++) {
        if (!valid[i]) continue;
        if (fabs(inputs[i] - median) <= threshold * scale) {
            sum += inputs[i];
            count++;
        }
    }

    if (count == 0) {
        *result = median;
        return VOTING_RESULT_DISCREPANCY;
    }

    *result = sum / (double)count;
    return VOTING_RESULT_OK;
}

/* ============================================================================
 * L5: High-Select and Low-Select Voting
 * ============================================================================
 */

voting_result_t voting_high_select(const double inputs[], const bool valid[],
                                   size_t n, double *result)
{
    if (!inputs || !valid || !result || n == 0)
        return VOTING_RESULT_INSUFFICIENT_INPUTS;

    double max_val = -1e300;
    bool found = false;

    for (size_t i = 0; i < n; i++) {
        if (valid[i]) {
            if (inputs[i] > max_val) max_val = inputs[i];
            found = true;
        }
    }

    if (!found) return VOTING_RESULT_INSUFFICIENT_INPUTS;
    *result = max_val;
    return VOTING_RESULT_OK;
}

voting_result_t voting_low_select(const double inputs[], const bool valid[],
                                  size_t n, double *result)
{
    if (!inputs || !valid || !result || n == 0)
        return VOTING_RESULT_INSUFFICIENT_INPUTS;

    double min_val = 1e300;
    bool found = false;

    for (size_t i = 0; i < n; i++) {
        if (valid[i]) {
            if (inputs[i] < min_val) min_val = inputs[i];
            found = true;
        }
    }

    if (!found) return VOTING_RESULT_INSUFFICIENT_INPUTS;
    *result = min_val;
    return VOTING_RESULT_OK;
}

/* ============================================================================
 * L5: Majority Check and Discrepancy
 * ============================================================================
 */

bool voting_has_majority(const double inputs[], const bool valid[],
                         size_t n, double threshold)
{
    if (!inputs || !valid || n < 2) return false;

    for (size_t i = 0; i < n; i++) {
        if (!valid[i]) continue;
        double scale = fmax(fabs(inputs[i]), 1.0);
        size_t agree = 1;
        for (size_t j = 0; j < n; j++) {
            if (i == j || !valid[j]) continue;
            if (fabs(inputs[i] - inputs[j]) <= threshold * scale) {
                agree++;
            }
        }
        if (agree > n / 2) return true;
    }
    return false;
}

double voting_discrepancy(double a, double b)
{
    double diff = fabs(a - b);
    double scale = fmax(fmax(fabs(a), fabs(b)), 1e-10);
    return diff / scale;
}

/* ============================================================================
 * L5: Byzantine-Resilient Voting
 * ============================================================================
 * Knowledge: Byzantine faults are arbitrary, potentially malicious
 * failures where a component can produce any output, including
 * different outputs to different observers (Lamport et al., 1982).
 *
 * Byzantine-resilient voting requires N >= 3f+1 components to tolerate
 * f Byzantine faults. With N=4, we can tolerate f=1 Byzantine fault.
 *
 * Algorithm:
 *   1. For each valid input, count how many others agree within threshold
 *   2. Select the value with the most agreements
 *   3. If no value has >= N-f agreements, return BYZANTINE_FAULT
 */

voting_result_t voting_byzantine_resilient(const double inputs[],
                                            const bool valid[],
                                            size_t n, double threshold,
                                            double *result)
{
    if (!inputs || !valid || !result || n < 3)
        return VOTING_RESULT_INSUFFICIENT_INPUTS;

    size_t best_idx = 0;
    size_t best_agree = 0;
    bool any_valid = false;

    for (size_t i = 0; i < n; i++) {
        if (!valid[i]) continue;
        any_valid = true;
        double scale = fmax(fabs(inputs[i]), 1.0);
        size_t agree = 1;

        for (size_t j = 0; j < n; j++) {
            if (i == j || !valid[j]) continue;
            if (fabs(inputs[i] - inputs[j]) <= threshold * scale) {
                agree++;
            }
        }

        if (agree > best_agree) {
            best_agree = agree;
            best_idx = i;
        }
    }

    if (!any_valid) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    /* Need at least N-f agreements for f Byzantine faults
     * With f=1, need N-1 agreements */
    size_t f = (n - 1) / 3;  /* Maximum tolerable Byzantine faults */
    if (best_agree < n - f) {
        *result = inputs[best_idx];
        return VOTING_RESULT_BYZANTINE_FAULT;
    }

    *result = inputs[best_idx];
    return VOTING_RESULT_OK;
}

/* ============================================================================
 * L5: Agreement Count and Standard Deviation
 * ============================================================================
 */

uint8_t voting_agreement_count(const double inputs[], const bool valid[],
                               size_t n, double reference, double threshold)
{
    if (!inputs || !valid) return 0;
    double scale = fmax(fabs(reference), 1.0);
    uint8_t count = 0;

    for (size_t i = 0; i < n; i++) {
        if (valid[i]
            && fabs(inputs[i] - reference) <= threshold * scale) {
            count++;
        }
    }
    return count;
}

double voting_input_stddev(const double inputs[], const bool valid[],
                           size_t n)
{
    if (!inputs || !valid || n < 2) return 0.0;

    double sum = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        if (valid[i]) {
            sum += inputs[i];
            count++;
        }
    }

    if (count < 2) return 0.0;

    double mean = sum / (double)count;
    double var_sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (valid[i]) {
            double d = inputs[i] - mean;
            var_sum += d * d;
        }
    }

    return sqrt(var_sum / (double)(count - 1));
}

/* ============================================================================
 * L5: Voting Execute — Main Dispatcher
 * ============================================================================
 */

voting_result_t voting_execute(voter_t *voter)
{
    if (!voter) return VOTING_RESULT_INSUFFICIENT_INPUTS;

    double valid_vals[VOTING_MAX_INPUTS];
    bool valid_flags[VOTING_MAX_INPUTS];

    for (uint8_t i = 0; i < voter->n_inputs && i < VOTING_MAX_INPUTS; i++) {
        valid_vals[i] = voter->inputs[i].value;
        valid_flags[i] = voter->inputs[i].valid;
    }

    switch (voter->algorithm) {
        case VOTING_ALGORITHM_2OO3:
            if (voter->n_inputs >= 3) {
                voting_result_t r = voting_2oo3(
                    voter->inputs[0].value,
                    voter->inputs[1].value,
                    voter->inputs[2].value,
                    voter->discrepancy_threshold,
                    &voter->selected_value);
                voter->result = r;
                return r;
            }
            break;

        case VOTING_ALGORITHM_2OO4:
            if (voter->n_inputs >= 4) {
                voting_result_t r = voting_2oo4(
                    valid_vals, 4,
                    voter->discrepancy_threshold,
                    &voter->selected_value);
                voter->result = r;
                return r;
            }
            break;

        case VOTING_ALGORITHM_MEDIAN: {
            voting_result_t r = voting_median(
                valid_vals, valid_flags,
                voter->n_inputs, &voter->selected_value);
            voter->result = r;
            return r;
        }

        case VOTING_ALGORITHM_WEIGHTED: {
            voting_result_t r = voting_weighted(voter);
            return r;
        }

        case VOTING_ALGORITHM_MIDVALUE: {
            voting_result_t r = voting_midvalue(
                valid_vals, valid_flags,
                voter->n_inputs, &voter->selected_value);
            voter->result = r;
            return r;
        }

        case VOTING_ALGORITHM_AVERAGE: {
            voting_result_t r = voting_average_with_check(
                valid_vals, valid_flags,
                voter->n_inputs, voter->discrepancy_threshold,
                &voter->selected_value);
            voter->result = r;
            return r;
        }

        case VOTING_ALGORITHM_HIGH_SELECT: {
            voting_result_t r = voting_high_select(
                valid_vals, valid_flags,
                voter->n_inputs, &voter->selected_value);
            voter->result = r;
            return r;
        }

        case VOTING_ALGORITHM_LOW_SELECT: {
            voting_result_t r = voting_low_select(
                valid_vals, valid_flags,
                voter->n_inputs, &voter->selected_value);
            voter->result = r;
            return r;
        }

        case VOTING_ALGORITHM_BYZANTINE: {
            voting_result_t r = voting_byzantine_resilient(
                valid_vals, valid_flags,
                voter->n_inputs, voter->discrepancy_threshold,
                &voter->selected_value);
            voter->result = r;
            return r;
        }

        default:
            voter->result = VOTING_RESULT_INSUFFICIENT_INPUTS;
            return VOTING_RESULT_INSUFFICIENT_INPUTS;
    }

    voter->result = VOTING_RESULT_INSUFFICIENT_INPUTS;
    return VOTING_RESULT_INSUFFICIENT_INPUTS;
}
