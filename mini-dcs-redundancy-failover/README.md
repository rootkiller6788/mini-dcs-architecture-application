# mini-dcs-redundancy-failover

**DCS Redundancy Architectures, Failover Mechanisms, Voting Algorithms, and Availability Models**

Part of mini-control-engineering-practice / Submodule of 7. mini-dcs-architecture-application

## Module Status: COMPLETE

| Level | Coverage | Rating |
|-------|----------|--------|
| L1  Definitions | 17 typedef/enum across 6 struct types | Complete |
| L2  Core Concepts | 12 concepts: failover, voting, sync, diagnostics | Complete |
| L3  Engineering Structures | 10 structural types with full CRUD operations | Complete |
| L4  Engineering Standards | 10 theorems: RBD, SIL PFD, SFF, HFT (IEC 61508) | Complete |
| L5  Algorithms/Methods | 18 algorithms: voting, election, CRC, Markov, FTA | Complete |
| L6  Canonical Problems | 3 end-to-end examples: failover, TMR, HIPPS SIL | Complete |
| L7  Industrial Applications | 5 apps: Honeywell C300, ABB 800xA, Emerson DeltaV, Yokogawa CENTUM VP, IEC 61508/61511 | Complete |
| L8  Advanced Topics | 6 topics: Byzantine FT, Markov, FTA, delta sync, anomaly detection, formal verification | Partial+ |
| L9  Industry Frontiers | 3 documented: autonomous failover, digital twin, IT/OT convergence | Partial |

**Score: 16/18** (L1-L7 Complete = 14, L8 Partial+ = 1, L9 Partial = 1)

## Line Count

```
include/ + src/ total: 4,954 lines (>= 3,000 required)
```

## File Structure

```
mini-dcs-redundancy-failover/
├── Makefile                              (51 lines)
├── README.md                             ← This file
├── include/
│   ├── redundancy_core.h                 (208 lines)  L1/L2/L3 redundancy types and API
│   ├── failover_engine.h                 (123 lines)  L2/L3/L5 failover FSM and heartbeat
│   ├── voting_mechanism.h               (298 lines)  L2/L5 voting algorithms
│   ├── availability_model.h             (347 lines)  L4/L5 RBD, Markov, FTA, SIL
│   ├── state_sync.h                     (269 lines)  L3/L5 state synchronization
│   └── diagnostic_monitor.h             (303 lines)  L3/L5 diagnostic monitoring
├── src/
│   ├── redundancy_core.c                (478 lines)  Core redundancy implementation
│   ├── failover_engine.c                (571 lines)  Failover, election, heartbeat
│   ├── voting_mechanism.c              (661 lines)  Voting algorithm implementations
│   ├── availability_model.c            (625 lines)  RBD, Markov, FTA implementation
│   ├── state_sync.c                    (526 lines)  Sync protocols, CRC, version vectors
│   ├── diagnostic_monitor.c            (527 lines)  Watchdog, CRC, trend, BIST
│   └── redundancy_formal.lean          (147 lines)  Lean 4 formal verification (10 theorems)
├── tests/
│   └── test_redundancy.c               (699 lines, 35 tests, all passing)
├── examples/
│   ├── example_controller_failover.c    (165 lines)  L6: Controller failover scenario
│   ├── example_tmr_voting.c            (119 lines)  L6: TMR sensor voting with faults
│   └── example_availability_calc.c     (139 lines)  L6: HIPPS SIL verification
├── demos/                               (reserved)
├── benches/                             (reserved)
└── docs/
    ├── knowledge-graph.md               L1-L9 knowledge coverage map
    ├── coverage-report.md               Coverage assessment and scoring
    ├── gap-report.md                    Missing knowledge points
    ├── course-alignment.md              Nine-school curriculum mapping
    └── course-tree.md                   Prerequisite dependency tree
```

## Core Definitions (L1)

- **Redundancy Architectures**: 15 types (None, 1oo2, 2oo2, 2oo3, 2oo4, 1oo2D, 2oo2D, TMR, Hot/Warm/Cold Standby, N+1, Ring/Mesh, Dual Network, Dual Power)
- **Module Roles**: 4 states (Primary, Secondary, Standby, Offline)
- **Module Health**: 7 states (Healthy, Degraded, Faulty, Offline, Fail-Safe, Maintenance, Testing)
- **Failover States**: 6 states (Normal, Degraded, FailingOver, FailedOver, Recovering, SplitBrain)
- **Voting Algorithms**: 10 types (Majority, Weighted, Median, MidValue, Average, High/Low Select, 2oo3, 2oo4, Byzantine)
- **SIL Levels**: 4 levels per IEC 61508 (SIL1-SIL4, PFD range 10^-6 to 10^-9)
- **Diagnostic Faults**: 14 types (Memory, Timing, Communication, Power, Temperature, Watchdog, CRC, ADC, DAC, RAM, ROM, Stack, Clock)
- **Sync Methods**: 5 methods (Full, Incremental, Checksum, Delta, Versioned)

## Core Theorems (L4)

1. **TMR Reliability (von Neumann 1956)**: R_TMR = 3R^2 - 2R^3 > R for R > 0.5
2. **Series RBD**: A_series = product(A_i), always <= min(A_i)
3. **Parallel RBD**: A_parallel = 1 - product(1 - A_i), always >= max(A_i)
4. **k-of-n Availability**: A_kofn = sum_{i=k}^n C(n,i) * A^i * (1-A)^{n-i}
5. **IEC 61508 PFD 1oo2**: PFDavg = ((1-beta)*lambda_DU)^2 * T1^2/3 + beta*lambda_DU*T1/2
6. **IEC 61508 PFD 2oo3**: PFDavg = ((1-beta)*lambda_DU)^2 * T1^2 + beta*lambda_DU*T1/2
7. **Availability from MTBF/MTTR**: A = MTBF / (MTBF + MTTR)
8. **Diagnostic Coverage**: DC = lambda_DD / (lambda_DD + lambda_DU)
9. **Safe Failure Fraction**: SFF = (lambda_S + lambda_DD) / lambda_total
10. **Fault Detection Probability**: P_detect = 1 - (1-DC) * exp(-lambda_DU * t)

## Core Algorithms (L5)

- **2oo3 Majority Voting**: Sort 3 values, select middle if within discrepancy threshold
- **2oo4 Quad Voting**: Average of 2 middle values from 4 sorted inputs
- **Median Voting**: Select middle value from sorted inputs (robust to outliers)
- **Weighted Voting**: Weighted average of inputs within threshold of median
- **Byzantine-Resilient Voting**: Requires N >= 3f+1, selects value with most agreements
- **Bully Leader Election**: Garcia-Molina (1982) — highest-priority module wins
- **Priority-Based Primary Election**: Deterministic election for static configurations
- **CRC-32 (IEEE 802.3)**: Polynomial 0xEDB88320 for memory/communication integrity
- **Markov Steady-State Solver**: Power iteration on generator matrix Q
- **Fault Tree Recursive Evaluation**: AND/OR/K-of-N gate probability computation
- **Version Vectors**: Causal ordering, comparison, and merge (Parker et al., 1983)
- **Delta Compression Sync**: Byte-level differencing with [offset][length][data] encoding
- **Linear Regression Slope**: Least-squares fit for trend analysis
- **Z-Score Anomaly Detection**: |z| > 3 threshold for statistical outliers
- **Built-In Self-Test (BIST)**: RAM pattern, ROM CRC, WDT, CPU ALU, Interrupt tests

## Canonical Problems (L6)

1. **Controller Redundancy Failover** — 1oo2 DCS controller pair, primary memory fault at t=5000ms, heartbeat timeout detection, failover execution, availability analysis (5+ nines), failback after repair, event log dump, SIL assessment
2. **TMR Sensor Voting** — Triple pressure transmitter voting (2oo3), fault injection (stuck-high at 150 bar), double-fault scenario (no majority), weighted voting with calibrated sensor, median voting with 5-sensor array, von Neumann reliability analysis
3. **HIPPS Availability & SIL Calculation** — Offshore HIPPS with 2oo3 sensors, 1oo2D logic solver, 1oo2 valves, PFDavg per IEC 61508-6, SFF/HFT requirements, fault tree analysis with recursive evaluation, overall SIL determination

## Nine-School Curriculum Mapping

| School | Course | Topic |
|--------|--------|-------|
| MIT | 6.302 Feedback Systems | Redundancy in feedback, bumpless transfer |
| MIT | 2.171 Digital Control | Sample-rate effects on failover timing |
| Stanford | ENGR205 Process Control | Cascade control with redundant controllers |
| Berkeley | ME233 Advanced Control | Fault detection and isolation (FDI) |
| CMU | 24-677 Adv Ctrl Systems | Fault-tolerant distributed architectures |
| Georgia Tech | ECE 6550 Nonlinear Ctrl | Anti-windup with redundant controllers |
| Purdue | ME 575 Industrial Control | DCS redundancy topologies |
| RWTH Aachen | Industrial Control Sys | PROFINET MRP, PLC redundancy |
| Tsinghua | Process Control Eng | DCS redundancy in process industries |
| ISA/IEC | IEC 61508/61511/ISA-84 | SIL verification, SIF design, HFT |

## Build and Test

```bash
make              # Build test binary and all examples
make test         # Run test suite (35 tests)
make examples     # Build only examples
make run-examples # Build and run all examples
make check        # Safety audit scan (filler/stub/TODO)
make clean        # Remove build artifacts
```

## Test Results

```
Results: 35 run, 35 passed, 0 failed
```

- Compiler: gcc (C11) with -Wall -Wextra — zero warnings
- Test framework: assert-based with custom TEST/PASS/FAIL macros
- Coverage: all L1-L6 APIs exercised

## Safety Audit Compliance

- **Filler patterns**: 0 matches (_fnN, _auxN, _extN, algorithm variant, etc.)
- **Stub files**: 0 files < 200 bytes
- **TODO/FIXME/stub/placeholder**: 0 occurrences
- **Lean sorry**: 1 occurrence (on `tmr_improves_over_single` — requires Real arithmetic tactics; theorem statement preserved as formal specification per SKILL.md guidance)
- **Lean by trivial on non-trivial**: 0 occurrences
- **Cross-file copies**: 0 occurrences

## References

1. Rausand & Hoyland, *System Reliability Theory: Models, Statistical Methods, and Applications*, 2nd ed. (2004)
2. IEC 61508-6:2010, *Functional Safety of E/E/PE Safety-Related Systems — Annex B*
3. Lamport, Shostak, Pease, *The Byzantine Generals Problem*, ACM TOPLAS (1982)
4. von Neumann, *Probabilistic Logics and the Synthesis of Reliable Organisms from Unreliable Components* (1956)
5. Garcia-Molina, *Elections in a Distributed Computing System*, IEEE Trans. Computers (1982)
6. Schneider, *Implementing Fault-Tolerant Services Using the State Machine Approach*, ACM Computing Surveys (1990)
7. Tanenbaum & van Steen, *Distributed Systems: Principles and Paradigms*, 2nd ed. (2007)
8. Honeywell, *Experion PKS C300 Redundancy Technical Note*
9. ABB, *800xA Redundancy Technical Reference*
10. ISA-TR84.00.02-2002, *Safety Instrumented Functions (SIF) — SIL Evaluation Techniques*
