# Course Tree — SUPCON ECS-700 DCS

## Prerequisite Knowledge Dependencies

```
ECS-700 DCS Architecture
│
├── Prerequisites (must be learned first)
│   ├── mini-pid-control-engineering (PID fundamentals)
│   │   ├── PID structure (P, PI, PD, PID)
│   │   ├── Anti-windup and bumpless transfer
│   │   ├── Ziegler-Nichols tuning
│   │   └── PID discretization (positional/velocity forms)
│   │
│   ├── mini-advanced-pid-tuning
│   │   └── Relay auto-tuning (Astrom-Hagglund)
│   │
│   ├── mini-feedforward-cascade-ratio
│   │   ├── Cascade control structure
│   │   └── Feedforward compensation
│   │
│   ├── mini-plc-iec61131-fundamentals
│   │   └── IEC 61131-3 SFC language
│   │
│   ├── mini-industrial-measurement-actuator
│   │   ├── 4-20 mA transmitters
│   │   ├── Thermocouples and CJC
│   │   ├── RTD measurement
│   │   └── NAMUR NE43 fault detection
│   │
│   └── mini-industrial-communication-protocol
│       ├── MODBUS TCP
│       ├── OPC UA concepts
│       └── CRC error detection
│
├── Current Module (mini-supcon-ecs700-dcs)
│   ├── System Architecture (domains, nodes, redundancy)
│   ├── Control Station Execution (PID, cascade, SFC)
│   ├── Redundancy Management (failover, health scoring)
│   ├── Communication (SCnet, PTP, MODBUS, OPC UA)
│   └── I/O Subsystem (signal processing, fault detection)
│
└── Dependent Modules (can be learned after)
    ├── mini-dcs-system-architecture (general DCS principles)
    ├── mini-honeywell-experion-pks (competitor comparison)
    ├── mini-yokogawa-centum-vp (competitor comparison)
    ├── mini-emerson-deltav-system (competitor comparison)
    ├── mini-scada-hmi-engineering (operator station)
    ├── mini-safety-instrumented-system (SIS integration)
    └── mini-industrial-real-time-database (historian)
```

## Learning Path

1. **Foundation**: PID control fundamentals (how PID works)
2. **Advanced PID**: Tuning methods, cascade, feedforward
3. **PLC/IEC 61131**: Sequential control with SFC
4. **Measurements**: How field instruments connect to DCS
5. **Communication**: Industrial protocols
6. **DCS Integration** (this module): Putting it all together in ECS-700
7. **Comparison**: Other DCS platforms
8. **Advanced**: HMI, safety, historian

## Knowledge Transfer Map

| Concept | Learned In | Applied In (This Module) |
|---------|-----------|--------------------------|
| PID algorithm | mini-pid-structure | ecs700_pid_execute() |
| Anti-windup | mini-anti-windup | Conditional integration in PID |
| Bumpless transfer | mini-anti-windup | ecs700_pid_mode_transition() |
| Cascade control | mini-feedforward-cascade | ecs700_cascade_execute() |
| SFC | mini-plc-iec61131 | ecs700_sfc_execute() |
| 4-20 mA | mini-measurement | ecs700_io_process_input() |
| NAMUR NE43 | mini-smart-instrument | ecs700_io_detect_open_wire() |
| MODBUS TCP | mini-communication | ecs700_modbus_mapping |
| OPC UA | mini-communication | ecs700_opcua_node |
| IEC 61508 | mini-functional-safety | ecs700_compute_pfd_avg() |
