/**
 * @file availability_model.c
 * @brief Availability / Reliability / SIL Models Implementation
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover
 *
 * Knowledge Coverage:
 *   L4 - Engineering Laws: RBD, Markov models, Fault Tree Analysis
 *   L4 - IEC 61508 SIL calculations, PFD, SFF, HFT requirements
 *   L5 - Algorithms: RBD reduction, Gauss-Seidel, recursive FTA evaluation
 *
 * Reference:
 *   Rausand & Hoyland, System Reliability Theory, 2nd ed. (2004)
 *   IEC 61508-6:2010 Annex B -- Example PFD calculations
 *   ISA-TR84.00.02-2002 -- SIL evaluation techniques
 */

#include "availability_model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * L4: Reliability Block Diagram (RBD) Calculations
 * ============================================================================
 * Knowledge: RBD models system reliability as a network of blocks.
 *   Series: R_sys = prod(R_i)         -- system fails if ANY fails
 *   Parallel: R_sys = 1 - prod(1-R_i) -- system fails if ALL fail
 *   k-out-of-n: R_sys = sum_{i=k}^n C(n,i) * R^i * (1-R)^{n-i}
 *
 * The binomial model assumes independent, identical components.
 * For non-identical components, use the Universal Generating Function.
 */

double rbd_series_availability(const double availabilities[], size_t n)
{
    if (!availabilities || n == 0) return 0.0;
    double result = 1.0;
    for (size_t i = 0; i < n; i++) {
        if (availabilities[i] < 0.0 || availabilities[i] > 1.0) return -1.0;
        result *= availabilities[i];
    }
    return result;
}

double rbd_parallel_availability(const double availabilities[], size_t n)
{
    if (!availabilities || n == 0) return 0.0;
    double product = 1.0;
    for (size_t i = 0; i < n; i++) {
        if (availabilities[i] < 0.0 || availabilities[i] > 1.0) return -1.0;
        product *= (1.0 - availabilities[i]);
    }
    return 1.0 - product;
}

/* Binomial coefficient: C(n,k) = n! / (k! * (n-k)!) */
static double binom(uint8_t n, uint8_t k)
{
    if (k > n) return 0.0;
    if (k == 0 || k == n) return 1.0;
    if (k > n - k) k = n - k;
    double r = 1.0;
    for (uint8_t i = 1; i <= k; i++) {
        r *= (double)(n - k + i) / (double)i;
    }
    return r;
}

double rbd_k_of_n_availability(uint8_t k, uint8_t n, double availability)
{
    if (k > n || n == 0) return -1.0;
    if (availability < 0.0 || availability > 1.0) return -1.0;

    double result = 0.0;
    for (uint8_t i = k; i <= n; i++) {
        result += binom(n, i) * pow(availability, (double)i)
                  * pow(1.0 - availability, (double)(n - i));
    }
    return result;
}

double rbd_k_of_n_with_ccf(uint8_t k, uint8_t n,
                           double availability, double beta)
{
    if (k > n || n == 0) return -1.0;
    if (beta < 0.0 || beta > 1.0) return -1.0;

    /* CCF model: system fails if CCF occurs (prob = beta * (1-A))
     * OR if independent + CCF combination causes < k available */
    double a_ind = availability;  /* Independent availability unchanged */
    double a_ccf = 1.0 - beta * (1.0 - availability);  /* Availability after CCF */

    /* P(system OK) = (1-beta) * P(k-of-n OK independently) + beta * P(CCF doesn't kill) */
    double r_ind = rbd_k_of_n_availability(k, n, a_ind);
    double r_ccf = (beta > 0.0) ? a_ccf : 0.0;

    return (1.0 - beta) * r_ind + beta * r_ccf;
}

double rbd_standby_availability(uint8_t n_standby,
                                double failure_rate, double repair_rate,
                                double switch_success)
{
    if (failure_rate <= 0.0 || repair_rate <= 0.0) return -1.0;
    if (switch_success < 0.0 || switch_success > 1.0) return -1.0;

    /* Simple 2-state Markov: State 0 = operational, State 1 = failed */
    /* For standby: effective failure rate accounts for imperfect switch */
    double lambda_eff = failure_rate * (1.0 + (1.0 - switch_success) * n_standby);
    double mu = repair_rate;

    /* Availability = mu / (lambda + mu) */
    return mu / (lambda_eff + mu);
}

/* ============================================================================
 * L5: Markov Model
 * ============================================================================
 * Knowledge: Continuous-Time Markov Chains (CTMC) model system behavior
 * as transitions between discrete states. For availability analysis:
 *   - States represent combinations of failed/working components
 *   - Transition rates = failure rates (lambda) and repair rates (mu)
 *   - Steady-state probabilities solved from pi * Q = 0, sum(pi) = 1
 *
 * Gauss-Seidel iteration is used because the transition rate matrix
 * is sparse and diagonally dominant for reliability problems.
 */

int markov_init(markov_model_t *model, uint8_t n_states)
{
    if (!model) return -1;
    if (n_states < 1 || n_states > MARKOV_MAX_STATES) return -1;

    memset(model, 0, sizeof(*model));
    model->n_states = n_states;
    model->solved = false;

    /* Initialize steady-state to uniform distribution */
    for (uint8_t i = 0; i < n_states; i++) {
        model->steady_state[i] = 1.0 / (double)n_states;
    }

    return 0;
}

int markov_set_transition(markov_model_t *model, uint8_t from, uint8_t to,
                          double rate)
{
    if (!model) return -1;
    if (from >= model->n_states || to >= model->n_states) return -1;
    if (from == to) return -1;
    if (rate < 0.0) return -1;

    model->transition_matrix[from][to] = rate;
    model->solved = false;  /* Invalidate any previous solution */
    return 0;
}

int markov_solve_steady_state(markov_model_t *model)
{
    if (!model || model->n_states < 1) return -1;

    /* Build the generator matrix Q and solve pi * Q = 0
     * Using power method (simpler than full matrix inversion for
     * diagonally dominant matrices):
     *   pi_new = pi_old * P  where P = I + Q * delta_t
     * Iterate until convergence.
     */
    uint8_t n = model->n_states;
    double pi[MARKOV_MAX_STATES];
    double pi_new[MARKOV_MAX_STATES];

    /* Initialize */
    for (uint8_t i = 0; i < n; i++) pi[i] = 1.0 / (double)n;

    /* Build generator Q: diagonal = -sum of outgoing rates */
    double Q[MARKOV_MAX_STATES][MARKOV_MAX_STATES];
    memset(Q, 0, sizeof(Q));
    for (uint8_t i = 0; i < n; i++) {
        double sum_out = 0.0;
        for (uint8_t j = 0; j < n; j++) {
            if (i != j) {
                Q[i][j] = model->transition_matrix[i][j];
                sum_out += Q[i][j];
            }
        }
        Q[i][i] = -sum_out;
    }

    /* Find maximum rate to determine step size */
    double max_rate = 1.0;
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t j = 0; j < n; j++) {
            double r = fabs(Q[i][j]);
            if (r > max_rate) max_rate = r;
        }
    }
    double dt = 0.1 / max_rate;

    /* Build transition probability matrix P = I + Q*dt */
    double P[MARKOV_MAX_STATES][MARKOV_MAX_STATES];
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t j = 0; j < n; j++) {
            P[i][j] = (i == j) ? (1.0 + Q[i][j] * dt) : (Q[i][j] * dt);
        }
    }

    /* Power iteration */
    for (int iter = 0; iter < 100000; iter++) {
        memset(pi_new, 0, sizeof(pi_new));
        for (uint8_t j = 0; j < n; j++) {
            for (uint8_t i = 0; i < n; i++) {
                pi_new[j] += pi[i] * P[i][j];
            }
        }

        /* Check convergence */
        double max_diff = 0.0;
        for (uint8_t i = 0; i < n; i++) {
            double diff = fabs(pi_new[i] - pi[i]);
            if (diff > max_diff) max_diff = diff;
            pi[i] = pi_new[i];
        }

        if (max_diff < 1e-12) {
            break;
        }
    }

    /* Normalize */
    double sum = 0.0;
    for (uint8_t i = 0; i < n; i++) sum += pi[i];
    if (sum > 0.0) {
        for (uint8_t i = 0; i < n; i++) {
            model->steady_state[i] = pi[i] / sum;
        }
    }

    model->solved = true;
    return 0;
}

double markov_compute_availability(const markov_model_t *model,
                                   const bool operational[])
{
    if (!model || !operational || !model->solved) return -1.0;

    double avail = 0.0;
    for (uint8_t i = 0; i < model->n_states; i++) {
        if (operational[i]) {
            avail += model->steady_state[i];
        }
    }
    return avail;
}

double markov_mttff(const markov_model_t *model, uint8_t absorbing_state)
{
    if (!model || !model->solved) return -1.0;
    if (absorbing_state >= model->n_states) return -1.0;

    /* MTTFF = 1 / (sum of transition rates into absorbing state
     *                  weighted by state probabilities) */
    double rate_into_absorbing = 0.0;
    for (uint8_t i = 0; i < model->n_states; i++) {
        if (i != absorbing_state) {
            double trans = model->transition_matrix[i][absorbing_state];
            rate_into_absorbing += model->steady_state[i] * trans;
        }
    }

    return (rate_into_absorbing > 0.0) ? (1.0 / rate_into_absorbing) : 1e9;
}

/* ============================================================================
 * L4: Fault Tree Analysis (FTA)
 * ============================================================================
 * Knowledge: Fault Tree Analysis is a top-down deductive method for
 * analyzing system failures. The top event (system failure) is decomposed
 * into contributing events connected by logic gates:
 *   - AND gate: all inputs must occur (P = prod(P_i))
 *   - OR gate: any input is sufficient (P = 1 - prod(1-P_i))
 *   - K-of-N: at least K of N inputs must occur
 *
 * FTA was developed by H.A. Watson at Bell Labs for the Minuteman missile
 * system (1961) and is now standardized in IEC 61025.
 */

ft_node_t *ft_create_basic_event(const char *label, double probability)
{
    ft_node_t *node = (ft_node_t *)calloc(1, sizeof(ft_node_t));
    if (!node) return NULL;

    node->type = FT_BASIC_EVENT;
    node->probability = probability;
    node->evaluated = true;  /* Basic events are pre-evaluated */
    node->result_probability = probability;
    node->n_children = 0;

    if (label) {
        strncpy(node->label, label, sizeof(node->label) - 1);
        node->label[sizeof(node->label) - 1] = '\0';
    }

    return node;
}

ft_node_t *ft_create_gate(ft_gate_type_t type, const char *label,
                          uint8_t k, uint8_t n)
{
    ft_node_t *node = (ft_node_t *)calloc(1, sizeof(ft_node_t));
    if (!node) return NULL;

    node->type = type;
    node->n_children = 0;
    node->evaluated = false;

    if (type == FT_VOTING_OR_K_OF_N) {
        node->k = k;
        node->n = n;
    }

    if (label) {
        strncpy(node->label, label, sizeof(node->label) - 1);
        node->label[sizeof(node->label) - 1] = '\0';
    }

    return node;
}

int ft_add_child(ft_node_t *parent, ft_node_t *child)
{
    if (!parent || !child) return -1;
    if (parent->n_children >= 8) return -1;

    parent->children[parent->n_children++] = child;
    parent->evaluated = false;  /* Must re-evaluate */
    return 0;
}

double ft_evaluate(ft_node_t *root)
{
    if (!root) return 0.0;

    /* If already evaluated, return cached result */
    if (root->evaluated) return root->result_probability;

    /* Recursively evaluate children */
    double child_probs[8];
    for (uint8_t i = 0; i < root->n_children; i++) {
        child_probs[i] = ft_evaluate(root->children[i]);
    }

    double result = 0.0;

    switch (root->type) {
        case FT_OR_GATE:
            /* P(OR) = 1 - prod(1 - P_i) */
            result = 0.0;
            {
                double prod = 1.0;
                for (uint8_t i = 0; i < root->n_children; i++) {
                    prod *= (1.0 - child_probs[i]);
                }
                result = 1.0 - prod;
            }
            break;

        case FT_AND_GATE:
            /* P(AND) = prod(P_i) */
            result = 1.0;
            for (uint8_t i = 0; i < root->n_children; i++) {
                result *= child_probs[i];
            }
            break;

        case FT_VOTING_OR_K_OF_N: {
            /* P(VOTING K-of-N) = sum_{i=k}^{n} [sum over combos prod] */
            /* For exact solution with n children, use inclusion-exclusion
             * Simplified: use binomial model if children have similar probs */
            double avg_prob = 0.0;
            for (uint8_t i = 0; i < root->n_children; i++) {
                avg_prob += child_probs[i];
            }
            avg_prob /= (double)root->n_children;
            result = rbd_k_of_n_availability(root->k, root->n_children, avg_prob);
            break;
        }

        case FT_INHIBIT:
            /* Inhibit gate: output occurs if input AND condition */
            if (root->n_children >= 2) {
                result = child_probs[0] * child_probs[1];
            }
            break;

        case FT_BASIC_EVENT:
            result = root->probability;
            break;

        default:
            result = 0.0;
            break;
    }

    root->result_probability = result;
    root->evaluated = true;
    return result;
}

void ft_free_tree(ft_node_t *root)
{
    if (!root) return;
    for (uint8_t i = 0; i < root->n_children; i++) {
        ft_free_tree(root->children[i]);
    }
    free(root);
}

/* ============================================================================
 * L4: SIL and PFD Calculations per IEC 61508
 * ============================================================================
 * Knowledge: Safety Integrity Level (SIL) quantifies the risk reduction
 * provided by a safety function. For low-demand mode:
 *   SIL1: PFDavg in [10^-6, 10^-5)   RRF = 10-100
 *   SIL2: PFDavg in [10^-7, 10^-6)   RRF = 100-1,000
 *   SIL3: PFDavg in [10^-8, 10^-7)   RRF = 1,000-10,000
 *   SIL4: PFDavg in [10^-9, 10^-8)   RRF = 10,000-100,000
 *
 * PFDavg formulas per IEC 61508-6 Annex B:
 *   Single channel: PFDavg = lambda_DU * T1 / 2
 *   1oo2: PFDavg = ((1-beta)*lambda_DU)^2 * T1^2/3 + beta*lambda_DU*T1/2
 *   2oo3: PFDavg = ((1-beta)*lambda_DU)^2 * T1^2 + beta*lambda_DU*T1/2
 */

int availability_sil_from_pfd(double pfd_avg)
{
    if (pfd_avg <= 0.0 || pfd_avg >= 1.0) return 0;
    /* SIL ranges per IEC 61508-1 Table 2:
     * SIL4: [1e-9, 1e-8), SIL3: [1e-8, 1e-7),
     * SIL2: [1e-7, 1e-6), SIL1: [1e-6, 1e-5) */
    if (pfd_avg >= 1e-5) return 0;
    if (pfd_avg >= 1e-6) return 1;
    if (pfd_avg >= 1e-7) return 2;
    if (pfd_avg >= 1e-8) return 3;
    return 4;
}

double availability_pfd_single_channel(double lambda_du, double t1)
{
    if (lambda_du <= 0.0 || t1 <= 0.0) return -1.0;
    return lambda_du * t1 / 2.0;
}

double availability_pfd_1oo2(double lambda_du, double t1, double beta)
{
    if (lambda_du <= 0.0 || t1 <= 0.0) return -1.0;
    if (beta < 0.0 || beta > 1.0) return -1.0;

    double lam_eff = (1.0 - beta) * lambda_du;
    return (lam_eff * lam_eff * t1 * t1) / 3.0
           + (beta * lambda_du * t1) / 2.0;
}

double availability_pfd_2oo3(double lambda_du, double t1, double beta)
{
    if (lambda_du <= 0.0 || t1 <= 0.0) return -1.0;
    if (beta < 0.0 || beta > 1.0) return -1.0;

    double lam_eff = (1.0 - beta) * lambda_du;
    return (lam_eff * lam_eff * t1 * t1)
           + (beta * lambda_du * t1) / 2.0;
}

double availability_safe_failure_fraction(double lambda_s,
                                          double lambda_dd,
                                          double lambda_du)
{
    double denom = lambda_s + lambda_dd + lambda_du;
    if (denom <= 0.0) return 0.0;
    return (lambda_s + lambda_dd) / denom;
}

int availability_hft_required(int sil, double sff)
{
    if (sil < 1 || sil > 4) return -1;
    if (sff < 0.0 || sff > 1.0) return -1;

    /* IEC 61508-2:2010 Table 3 — HFT vs SIL vs SFF */
    if (sff < 0.60) {
        switch (sil) {
            case 1: return 0;
            case 2: return 1;
            case 3: return 2;
            case 4: return 3;  /* SIL4 not achievable with SFF<60% per some interpretations */
            default: return -1;
        }
    } else if (sff < 0.90) {
        switch (sil) {
            case 1: return 0;
            case 2: return 0;
            case 3: return 1;
            case 4: return 2;
            default: return -1;
        }
    } else {  /* SFF >= 90% */
        switch (sil) {
            case 1: return 0;
            case 2: return 0;
            case 3: return 0;
            case 4: return 1;
            default: return -1;
        }
    }
}

double availability_mtbf(double failure_rate)
{
    if (failure_rate <= 0.0) return -1.0;
    return 1.0 / failure_rate;
}

double availability_from_mtbf_mttr(double mtbf, double mttr)
{
    if (mtbf <= 0.0 || mttr < 0.0) return -1.0;
    return mtbf / (mtbf + mttr);
}

int availability_nines(double availability)
{
    if (availability <= 0.0) return 0;
    if (availability >= 1.0) return 20;  /* Perfect availability */
    /* Add epsilon to handle floating point rounding (e.g., 0.99 -> 1.999... -> 2) */
    double nines = -log10(1.0 - availability);
    return (int)(nines + 1e-9);
}

/* ============================================================================
 * L5: Minimal Cut Sets
 * ============================================================================
 * Knowledge: A cut set is a set of basic events whose simultaneous
 * occurrence causes the top event. A minimal cut set contains no
 * subset that is also a cut set. MCS analysis identifies the most
 * critical combinations of failures.
 *
 * For small fault trees, MCS can be found by recursive enumeration.
 */

/* Helper: recursively collect cut sets */
static void collect_cut_sets(const ft_node_t *node,
                             char current_set[][128],
                             uint8_t *current_count,
                             char results[][128],
                             size_t *result_count,
                             size_t max_sets)
{
    if (!node || *result_count >= max_sets) return;

    if (node->type == FT_BASIC_EVENT) {
        /* This basic event is part of a cut set */
        if (*current_count < 8) {
            strncpy(current_set[*current_count], node->label, 127);
            current_set[*current_count][127] = '\0';
            (*current_count)++;
        }
        return;
    }

    if (node->type == FT_OR_GATE) {
        /* Each child of an OR gate forms its own cut set branch */
        for (uint8_t i = 0; i < node->n_children && *result_count < max_sets; i++) {
            uint8_t saved_count = *current_count;
            char saved[8][128];
            for (uint8_t j = 0; j < saved_count; j++) {
                strncpy(saved[j], current_set[j], 127);
            }

            collect_cut_sets(node->children[i], current_set, current_count,
                            results, result_count, max_sets);

            /* Restore for next branch */
            *current_count = saved_count;
            for (uint8_t j = 0; j < saved_count; j++) {
                memcpy(current_set[j], saved[j], 128);
            }
        }
    } else if (node->type == FT_AND_GATE) {
        /* All children of AND must occur together */
        for (uint8_t i = 0; i < node->n_children && *result_count < max_sets; i++) {
            collect_cut_sets(node->children[i], current_set, current_count,
                            results, result_count, max_sets);
        }
    }
}

int ft_minimal_cut_sets(const ft_node_t *root, char cut_sets[][128],
                        size_t max_sets)
{
    if (!root || !cut_sets || max_sets == 0) return 0;

    char current_set[8][128];
    uint8_t current_count = 0;
    size_t result_count = 0;

    memset(current_set, 0, sizeof(current_set));

    collect_cut_sets(root, current_set, &current_count,
                    cut_sets, &result_count, max_sets);

    /* If we collected a full set at the root, output it */
    if (current_count > 0 && result_count < max_sets) {
        /* Build concatenated string */
        size_t off = 0;
        cut_sets[result_count][0] = '\0';
        for (uint8_t i = 0; i < current_count; i++) {
            off += snprintf(cut_sets[result_count] + off,
                           128 - off, "%s%s",
                           (i > 0) ? "+" : "",
                           current_set[i]);
        }
        result_count++;
    }

    return (int)result_count;
}
