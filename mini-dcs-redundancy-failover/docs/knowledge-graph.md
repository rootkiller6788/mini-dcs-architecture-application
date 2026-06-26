# Knowledge Graph — mini-dcs-redundancy-failover

## L1: Definitions (Complete)

| # | Term | Definition | C Type | Lean Type |
|---|------|-----------|--------|-----------|
| 1 | Redundancy Architecture | M-out-of-N (MooN) voting scheme | `redundancy_architecture_t` | — |
| 2 | 1oo2 | One-out-of-two, dual modular redundancy | `REDUNDANCY_1OO2` | — |
| 3 | 2oo3 / TMR | Two-out-of-three, triple modular redundancy | `REDUNDANCY_2OO3`, `REDUNDANCY_TMR` | — |
| 4 | 2oo4 | Two-out-of-four, quad modular redundancy | `REDUNDANCY_2OO4` | — |
| 5 | Hot/Warm/Cold Standby | Standby redundancy with different sync levels | `REDUNDANCY_HOT_STANDBY`, etc. | — |
| 6 | Module Role | Primary / Secondary / Standby / Offline | `module_role_t` | — |
| 7 | Module Health | Healthy / Degraded / Faulty / Offline / Fail-Safe | `module_health_t` | — |
| 8 | Failover State | Normal / Degraded / Failing Over / Failed Over / Recovery | `failover_state_t` | — |
| 9 | Heartbeat | Periodic liveness signal with sequence number | `heartbeat_msg_t` | `HeartbeatSeq` |
| 10 | MTBF / MTTR | Mean Time Between Failures / Mean Time To Repair | — | — |
| 11 | Availability | A = MTBF / (MTBF + MTTR) | — | `availabilityFromMTBF` |
| 12 | SIL | Safety Integrity Level (1-4 per IEC 61508) | — | — |
| 13 | PFDavg | Average Probability of Failure on Demand | — | — |
| 14 | SFF | Safe Failure Fraction | — | `safeFailureFraction` |
| 15 | Diagnostic Coverage | DC = lambda_DD / (lambda_DD + lambda_DU) | — | `diagnosticCoverage` |
| 16 | CCF | Common Cause Failure, beta-factor model | `ccf_model_t` | — |
| 17 | Diversity | HW/SW/Vendor diversity to reduce CCF | `diversity_type_t` | — |

## L2: Core Concepts (Complete)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Redundancy Group Management | `redundancy_group_t`, `redundancy_group_init` |
| 2 | Health Status Evaluation | `redundancy_module_set_health`, `redundancy_group_healthy_count` |
| 3 | Degraded Mode Operation | `redundancy_group_is_degraded`, `redundancy_group_fault_tolerance` |
| 4 | Failover Execution | `failover_execute`, `failover_execute_failback` |
| 5 | Bumpless Transfer | `failover_bumpless_possible` |
| 6 | Split-Brain Detection | `failover_detect_split_brain` |
| 7 | Quorum Management | `failover_quorum_check` |
| 8 | State Consistency | `state_sync_verify_consistency` |
| 9 | Diagnostic Coverage | `diag_coverage_factor`, `diag_coverage_class` |
| 10 | Fault Reaction Time | `diag_fault_reaction_time_ms` |
| 11 | Vote/Signal Selection | `voting_execute` (2oo3, median, weighted, high/low select) |
| 12 | Byzantine Fault Tolerance | `voting_byzantine_resilient` |

## L3: Engineering Structures (Complete)

| # | Structure | File |
|---|-----------|------|
| 1 | Redundancy Group Configuration | `redundancy_core.h` — `redundancy_group_t` |
| 2 | Module Descriptor with Health/Role | `redundancy_core.h` — `redundancy_module_t` |
| 3 | Heartbeat Message Protocol | `failover_engine.h` — `heartbeat_msg_t` |
| 4 | Failover Event Log (Circular Buffer) | `failover_engine.h` — `failover_event_t` |
| 5 | Voter State Machine | `voting_mechanism.h` — `voter_t` |
| 6 | Memory Region Tracking for Sync | `state_sync.h` — `sync_region_t` |
| 7 | Version Vector for Causal Ordering | `state_sync.h` — `version_vector_t` |
| 8 | Diagnostic Fault Record | `diagnostic_monitor.h` — `diag_fault_record_t` |
| 9 | Trend Data Point for Time Series | `diagnostic_monitor.h` — `diag_trend_point_t` |
| 10 | CCF Model Parameters | `redundancy_core.h` — `ccf_model_t` |

## L4: Engineering Laws (Complete)

| # | Law/Standard | Implementation |
|---|-------------|---------------|
| 1 | Series RBD: R_sys = prod(R_i) | `rbd_series_availability` |
| 2 | Parallel RBD: R_sys = 1 - prod(1-R_i) | `rbd_parallel_availability` |
| 3 | k-out-of-n: binomial expansion | `rbd_k_of_n_availability` |
| 4 | IEC 61508 SIL PFD: 1oo2 formula | `availability_pfd_1oo2` |
| 5 | IEC 61508 SIL PFD: 2oo3 formula | `availability_pfd_2oo3` |
| 6 | IEC 61508 SIL classification | `availability_sil_from_pfd` |
| 7 | IEC 61508 HFT vs SIL vs SFF | `availability_hft_required` |
| 8 | Availability from MTBF/MTTR | `availability_from_mtbf_mttr` |
| 9 | von Neumann TMR Theorem | `redundancy_reliability_factor` |
| 10 | Availability nines calculation | `availability_nines` |

## L5: Algorithms/Methods (Complete)

| # | Algorithm | Implementation |
|---|-----------|---------------|
| 1 | 2oo3 Majority Voting with Discrepancy | `voting_2oo3` |
| 2 | 2oo4 Quad-Redundant Mid-Value Selection | `voting_2oo4` |
| 3 | Median Voting (Robust to Outliers) | `voting_median` |
| 4 | Weighted Voting | `voting_weighted` |
| 5 | High-Select / Low-Select Voting | `voting_high_select`, `voting_low_select` |
| 6 | Byzantine-Resilient Voting (3f+1) | `voting_byzantine_resilient` |
| 7 | Bully Leader Election (Garcia-Molina) | `failover_bully_election` |
| 8 | Priority-Based Primary Election | `failover_elect_primary` |
| 9 | CRC-32 (IEEE 802.3 polynomial) | `diag_crc32`, `state_sync_compute_checksum` |
| 10 | Markov Steady-State Solver (Power Iteration) | `markov_solve_steady_state` |
| 11 | Fault Tree Recursive Evaluation | `ft_evaluate` |
| 12 | Gauss-Seidel for Markov Chains | Within `markov_solve_steady_state` |
| 13 | Version Vector Operations | `state_sync_version_vector_*` |
| 14 | Delta Compression Sync | `state_sync_delta_transfer` |
| 15 | Linear Regression Trend Analysis | `diag_trend_slope` |
| 16 | Z-Score Anomaly Detection | `diag_detect_anomalies` |
| 17 | Built-In Self-Test (BIST) | `diag_run_bist` |
| 18 | Exponential Fault Detection Probability | `diag_fault_detection_probability` |

## L6: Canonical Problems (Complete)

| # | Problem | Example File |
|---|---------|-------------|
| 1 | Controller Redundancy Failover Simulation | `examples/example_controller_failover.c` |
| 2 | Triple Modular Redundancy Sensor Voting | `examples/example_tmr_voting.c` |
| 3 | HIPPS Availability & SIL Calculation | `examples/example_availability_calc.c` |

## L7: Industrial Applications (Partial+)

| # | Application | Coverage |
|---|------------|----------|
| 1 | Honeywell Experion C300 Redundancy | Referenced in architecture docs |
| 2 | ABB 800xA State Sync Methods | Referenced in state_sync docs |
| 3 | Emerson DeltaV Redundancy | Peer-to-peer sync reference |
| 4 | Yokogawa CENTUM VP Pair-and-Spare | Cross-referenced in redundancy patterns |
| 5 | IEC 61508/61511 SIS Design | Full PFD/SIL calculations implemented |

## L8: Advanced Topics (Partial+)

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Byzantine Fault Tolerance | `voting_byzantine_resilient` with N >= 3f+1 |
| 2 | Markov Availability Models | `markov_model_t` with steady-state solver |
| 3 | Fault Tree Analysis | `ft_node_t` with AND/OR/K-of-N gates |
| 4 | Multi-Method State Sync | Full/Incremental/Checksum/Delta sync methods |
| 5 | Statistical Anomaly Detection | Z-score anomaly detection in trend data |
| 6 | Formal Verification (Lean 4) | `redundancy_formal.lean` with 10 theorems |

## L9: Industry Frontiers (Partial)

| # | Topic | Coverage |
|---|-------|---------|
| 1 | Autonomous Failover | Documented concept of zero-touch failover |
| 2 | IT/OT Convergence | Architecture supports dual-network and cybersecurity |
| 3 | Digital Twin for Redundancy Testing | Formal models enable simulation-based verification |
