# Course Tree — DCS System Architecture

## Prerequisite Dependency Tree

```
L0: C Programming, Basic Control Theory
│
├── L1: Definitions
│   └── ISA-95 levels, DCS node types, signal types, SIL levels
│
├── L2: Core Concepts
│   ├── Prerequisites: L1
│   └── ISA-95 mapping, architecture verification, redundancy, alarm states
│
├── L3: Engineering Structures
│   ├── Prerequisites: L1, L2
│   └── Topology analysis, controller loading, scan cycle, database config
│
├── L4: Engineering Standards
│   ├── Prerequisites: L1, L2
│   └── IEC 61508 PFD, ISA-18.2 alarms, ISA-88 batch
│       └── Requires: probability theory, reliability engineering
│
├── L5: Algorithms/Methods
│   ├── Prerequisites: L3, L4
│   └── Bumpless transfer, flood detection, token passing, scheduling
│       └── Requires: control theory, queuing theory
│
├── L6: Canonical Problems
│   ├── Prerequisites: L3-L5
│   └── System sizing, SIF design, batch control
│
├── L7: Industrial Applications
│   ├── Prerequisites: L1-L6
│   └── Vendor-specific models (Honeywell, Yokogawa, Emerson)
│
├── L8: Advanced Topics
│   ├── Prerequisites: L5, L7
│   └── Rate monotonic scheduling, network convergence
│
└── L9: Industry Frontiers
    ├── Prerequisites: L1-L8
    └── Autonomous operations, digital twin (documented)
```

## External Prerequisites (from other modules)
- **0. mini-industrial-measurement-actuator** → Signal types, I/O definitions (L1)
- **1. mini-pid-control-engineering** → PID loop configuration (L3)
- **4. mini-plc-iec61131-fundamentals** → Scan cycle model (L3)
- **9. mini-industrial-communication-protocol** → Network protocols (L3, L6)
- **10. mini-safety-instrumented-system** → SIL, SIF concepts (L4, L6)
- **15. mini-batch-process-control-isa88** → ISA-88 detailed models (L4)
