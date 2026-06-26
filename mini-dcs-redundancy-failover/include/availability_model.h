/**
 * @file availability_model.h
 * @brief Availability, Reliability, and Safety Integrity Models
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover (7. mini-dcs-architecture-application)
 *
 * Knowledge Coverage:
 *   L4 - Engineering Laws: Reliability Block Diagrams (RBD), Markov models,
 *        Fault Tree Analysis (FTA), SIL calculations per IEC 61508
 *   L5 - Algorithms: RBD reduction, Markov steady-state, minimal cut sets
 *
 * Reference:
 *   - Rausand & Hoyland, System Reliability Theory, 2nd ed. (2004)
 *   - IEC 61508-6:2010 Annex B -- Example calculations
 *   - ISA-TR84.00.02-2002 -- SIL evaluation techniques
 */

#ifndef AVAILABILITY_MODEL_H
#define AVAILABILITY_MODEL_H

#include <stdint.h>
#include <stdbool.h>

/* RBD component types */
typedef enum {
    RBD_SERIES    = 0,
    RBD_PARALLEL  = 1,
    RBD_K_OF_N    = 2,
    RBD_STANDBY   = 3,
    RBD_BRIDGE    = 4
} rbd_block_type_t;

typedef struct {
    rbd_block_type_t type;
    double availability;
    double failure_rate;
    double repair_rate;
    uint8_t k;
    uint8_t n;
    double beta_ccf;
    bool imperfect_switch;
    double switch_probability;
} rbd_block_t;

/* Markov model states */
#define MARKOV_MAX_STATES 16

typedef struct {
    uint8_t  n_states;
    double   transition_matrix[MARKOV_MAX_STATES][MARKOV_MAX_STATES];
    double   steady_state[MARKOV_MAX_STATES];
    bool     solved;
    char     state_names[MARKOV_MAX_STATES][32];
} markov_model_t;

/* Fault tree gate types */
typedef enum {
    FT_AND_GATE = 0,
    FT_OR_GATE  = 1,
    FT_VOTING_OR_K_OF_N = 2,
    FT_INHIBIT  = 3,
    FT_BASIC_EVENT = 4,
    FT_UNDEVELOPED_EVENT = 5
} ft_gate_type_t;

/* Fault tree node */
typedef struct ft_node_t {
    ft_gate_type_t type;
    char           label[64];
    double         probability;
    uint8_t        k;  /* for K-of-N gates */
    uint8_t        n;
    struct ft_node_t *children[8];
    uint8_t        n_children;
    bool           evaluated;
    double         result_probability;
} ft_node_t;

/**
 * Compute system availability for a series RBD.
 * A_series = product(A_i) for all i
 * @param availabilities Array of component availabilities
 * @param n              Number of components
 * @return               System availability
 * Complexity: O(n)
 */
double rbd_series_availability(const double availabilities[], size_t n);

/**
 * Compute system availability for a parallel RBD.
 * A_parallel = 1 - product(1 - A_i)
 * @param availabilities Array of component availabilities
 * @param n              Number of components
 * @return               System availability
 * Complexity: O(n)
 */
double rbd_parallel_availability(const double availabilities[], size_t n);

/**
 * Compute k-out-of-n system availability using binomial expansion.
 * A_kofn = sum_{i=k}^{n} C(n,i) * A^i * (1-A)^{n-i}
 * @param k Min required operational components
 * @param n Total components
 * @param availability Component availability (assumed identical)
 * @return System availability
 * Complexity: O(n)
 */
double rbd_k_of_n_availability(uint8_t k, uint8_t n, double availability);

/**
 * Compute k-out-of-n availability with common cause failure.
 * Includes beta-factor model for CCF per IEC 61508.
 * @param k Min required
 * @param n Total
 * @param availability Component availability
 * @param beta Common cause factor [0,1]
 * @return System availability with CCF
 * Complexity: O(n)
 */
double rbd_k_of_n_with_ccf(uint8_t k, uint8_t n,
                           double availability, double beta);

/**
 * Compute standby system availability (1 active + N-1 standby).
 * Uses Markov model for imperfect switch detection.
 * @param n_standby Number of standby units
 * @param failure_rate Active unit failure rate (per hour)
 * @param repair_rate Repair rate (per hour)
 * @param switch_success Probability of successful switchover
 * @return System availability
 * Complexity: O(n^2) -- solves Markov chain
 */
double rbd_standby_availability(uint8_t n_standby,
                                double failure_rate, double repair_rate,
                                double switch_success);

/**
 * Initialize a Markov model for availability analysis.
 * @param model    Markov model to initialize
 * @param n_states Number of system states
 * @return         0 on success, -1 on error
 * Complexity: O(1)
 */
int markov_init(markov_model_t *model, uint8_t n_states);

/**
 * Set a transition rate between two Markov states.
 * @param model Markov model
 * @param from  Source state index
 * @param to    Destination state index
 * @param rate  Transition rate (per hour)
 * @return      0 on success
 * Complexity: O(1)
 */
int markov_set_transition(markov_model_t *model, uint8_t from, uint8_t to,
                          double rate);

/**
 * Solve the Markov model for steady-state probabilities.
 * Uses Gauss-Seidel iteration on pi*Q = 0, sum(pi) = 1.
 * @param model Markov model to solve
 * @return      0 on success, -1 if not converged
 * Complexity: O(S^2 * iterations) where S = n_states
 */
int markov_solve_steady_state(markov_model_t *model);

/**
 * Compute system availability from Markov steady-state solution.
 * Availability = sum of probabilities of operational states.
 * @param model        Markov model with solved steady-state
 * @param operational  Array marking which states are operational
 * @return             System availability
 * Complexity: O(S)
 */
double markov_compute_availability(const markov_model_t *model,
                                   const bool operational[]);

/**
 * Compute mean time to first failure (MTTFF) from Markov model.
 * @param model           Markov model
 * @param absorbing_state Index of the failure state
 * @return                MTTFF in hours
 * Complexity: O(S^3) for matrix solve
 */
double markov_mttff(const markov_model_t *model, uint8_t absorbing_state);

/**
 * Create a fault tree basic event node.
 * @param label Description label
 * @param probability Failure probability
 * @return Allocated node, or NULL on error
 * Complexity: O(1)
 */
ft_node_t *ft_create_basic_event(const char *label, double probability);

/**
 * Create a fault tree gate node (AND/OR/K-of-N).
 * @param type Gate type
 * @param label Description label
 * @param k    Required count (for K-of-N gates)
 * @param n    Total count (for K-of-N gates)
 * @return Allocated node, or NULL on error
 * Complexity: O(1)
 */
ft_node_t *ft_create_gate(ft_gate_type_t type, const char *label,
                          uint8_t k, uint8_t n);

/**
 * Add a child node to a fault tree gate.
 * @param parent Parent gate node
 * @param child  Child node to add
 * @return       0 on success, -1 if parent is full
 * Complexity: O(1)
 */
int ft_add_child(ft_node_t *parent, ft_node_t *child);

/**
 * Evaluate the fault tree to compute top event probability.
 * Recursive evaluation: leaves up to root.
 * @param root Root node of fault tree
 * @return     Top event probability
 * Complexity: O(N) where N = total nodes
 */
double ft_evaluate(ft_node_t *root);

/**
 * Free all nodes in a fault tree.
 * @param root Root node
 * Complexity: O(N)
 */
void ft_free_tree(ft_node_t *root);

/**
 * Compute SIL level from PFDavg per IEC 61508-1 Table 2.
 * SIL1: 1e-6 <= PFD < 1e-5
 * SIL2: 1e-7 <= PFD < 1e-6
 * SIL3: 1e-8 <= PFD < 1e-7
 * SIL4: 1e-9 <= PFD < 1e-8
 * @param pfd_avg Average probability of failure on demand
 * @return        SIL level 1-4, or 0 if < SIL1
 * Complexity: O(1)
 */
int availability_sil_from_pfd(double pfd_avg);

/**
 * Compute PFDavg for a single-channel system.
 * PFDavg = lambda_DU * T1 / 2
 * where lambda_DU = undetected dangerous failure rate
 *       T1 = proof test interval (hours)
 * @param lambda_du Dangerous undetected failure rate (per hour)
 * @param t1        Proof test interval (hours)
 * @return          PFDavg
 * Complexity: O(1)
 */
double availability_pfd_single_channel(double lambda_du, double t1);

/**
 * Compute PFDavg for a 1oo2 architecture per IEC 61508-6.
 * PFDavg = ((1-beta)*lambda_DU)^2 * T1^2 / 3 + beta*lambda_DU*T1/2
 * @param lambda_du Dangerous undetected failure rate
 * @param t1        Proof test interval (hours)
 * @param beta      Common cause factor
 * @return          PFDavg for 1oo2
 * Complexity: O(1)
 */
double availability_pfd_1oo2(double lambda_du, double t1, double beta);

/**
 * Compute PFDavg for a 2oo3 architecture per IEC 61508-6.
 * PFDavg = ((1-beta)*lambda_DU)^2 * T1^2 + beta*lambda_DU*T1/2
 * @param lambda_du Dangerous undetected failure rate
 * @param t1        Proof test interval (hours)
 * @param beta      Common cause factor
 * @return          PFDavg for 2oo3
 * Complexity: O(1)
 */
double availability_pfd_2oo3(double lambda_du, double t1, double beta);

/**
 * Compute the Safe Failure Fraction (SFF) from failure rates.
 * SFF = (lambda_S + lambda_DD) / (lambda_S + lambda_DD + lambda_DU)
 * @param lambda_s  Safe failure rate
 * @param lambda_dd Dangerous detected failure rate
 * @param lambda_du Dangerous undetected failure rate
 * @return          SFF [0, 1]
 * Complexity: O(1)
 */
double availability_safe_failure_fraction(double lambda_s,
                                          double lambda_dd,
                                          double lambda_du);

/**
 * Compute hardware fault tolerance (HFT) requirement for a target SIL.
 * Per IEC 61508-2 Table 3:
 *   SIL1: HFT=0 (SFF<60%), HFT=0 (60-90%), HFT=0 (>90%)
 *   SIL2: HFT=1 (SFF<60%), HFT=0 (60-90%), HFT=0 (>90%)
 *   SIL3: HFT=2 (SFF<60%), HFT=1 (60-90%), HFT=0 (>90%)
 *   SIL4: HFT=3 (SFF<60%), HFT=2 (60-90%), HFT=1 (>90%)
 * @param sil Target SIL level (1-4)
 * @param sff Safe failure fraction [0,1]
 * @return    Required HFT (0-3), or -1 if combination not allowed
 * Complexity: O(1)
 */
int availability_hft_required(int sil, double sff);

/**
 * Compute MTBF from failure rate (exponential distribution).
 * MTBF = 1 / lambda
 * @param failure_rate Lambda in failures per hour
 * @return MTBF in hours
 * Complexity: O(1)
 */
double availability_mtbf(double failure_rate);

/**
 * Compute availability from MTBF and MTTR.
 * Availability = MTBF / (MTBF + MTTR)
 * @param mtbf Mean time between failures (hours)
 * @param mttr Mean time to repair (hours)
 * @return     Availability [0, 1]
 * Complexity: O(1)
 */
double availability_from_mtbf_mttr(double mtbf, double mttr);

/**
 * Compute the number of nines of availability.
 * e.g., 0.99999 = 5 nines
 * @param availability Availability [0, 1)
 * @return Number of nines
 * Complexity: O(1)
 */
int availability_nines(double availability);

/**
 * Compute minimum cut sets from a fault tree for FTA analysis.
 * Returns the number of minimal cut sets found.
 * @param root    Fault tree root node
 * @param cut_sets Output array of cut set strings
 * @param max_sets Maximum cut sets to find
 * @return         Number of minimal cut sets
 * Complexity: O(2^N) worst case for N basic events
 */
int ft_minimal_cut_sets(const ft_node_t *root, char cut_sets[][128],
                        size_t max_sets);

#endif /* AVAILABILITY_MODEL_H */
