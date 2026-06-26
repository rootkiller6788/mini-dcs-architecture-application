# Course Tree — mini-dcs-redundancy-failover

## Prerequisite Dependencies

```
mini-dcs-redundancy-failover
  ├── mini-industrial-measurement-actuator (L1: sensor/actuator definitions)
  ├── mini-pid-control-engineering (L1-L2: PID control, bumpless transfer)
  ├── mini-advanced-pid-tuning (L5: anti-windup, gain scheduling)
  ├── mini-feedforward-cascade-ratio (L2: cascade structure)
  ├── mini-plc-iec61131-fundamentals (L3: scan cycle, I/O refresh)
  ├── mini-industrial-communication-protocol (L3: heartbeat protocols, CRC)
  └── mini-safety-instrumented-system (L4: SIL, PFD, SFF, IEC 61508)
```

## Knowledge Flow

### L1 → L2
Definitions of redundancy architectures → Core concepts of health monitoring and failover

### L2 → L3
Failover concepts → Heartbeat protocol engineering structures

### L3 → L4
Structures → Engineering laws: RBD, Markov, SIL formulas

### L4 → L5
Laws → Algorithms: voting, election, CRC, sync

### L5 → L6
Algorithms → Canonical problems: failover simulation, sensor voting, SIL calculation

### L6 → L7
Problems → Industrial applications: Honeywell, ABB, Emerson, Yokogawa references

### L7 → L8
Applications → Advanced: Byzantine tolerance, Markov models, anomaly detection

### L8 → L9
Advanced → Frontiers: autonomous failover, digital twin, IT/OT convergence

## Research Frontiers (L9)

1. **Autonomous Operations** — Self-healing DCS with AI-driven failover decisions
2. **Digital Twin** — Virtual replica for pre-deployment redundancy testing
3. **IT/OT Convergence** — Cloud-integrated redundancy across sites
4. **5G Wireless** — Ultra-reliable low-latency wireless redundancy
5. **Zero-Trust Security** — Byzantine-resilient architectures for OT cybersecurity
