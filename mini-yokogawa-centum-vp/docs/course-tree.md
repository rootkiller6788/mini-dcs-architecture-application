# Course Dependency Tree ? Yokogawa CENTUM VP DCS

## Prerequisites

```
mini-pid-control-engineering (PID fundamentals)
??? mini-pid-structure-p-i-pi-pd-pid
??? mini-anti-windup-bumpless-transfer
??? mini-sampling-rate-discretization

mini-dcs-system-architecture (DCS fundamentals)
??? DCS hierarchy model
??? Redundancy concepts
??? Control network basics

mini-plc-iec61131-fundamentals (PLC/DCS programming)
??? Function block programming
??? Sequential function charts

mini-industrial-communication-protocol (Protocols)
??? Modbus RTU/TCP
??? OPC DA/UA
??? Foundation Fieldbus

mini-batch-process-control-isa88 (Batch control)
??? ISA-88 models and terminology
```

## This Module

```
mini-yokogawa-centum-vp (Yokogawa CENTUM VP DCS)
?
??? centum_vp_system.h/.c      ? System architecture, domains, stations
?   ??? depends on: mini-dcs-system-architecture
?
??? centum_vp_fcs.h/.c         ? Field control station, N-IO, I/O modules
?   ??? depends on: mini-industrial-measurement-actuator
?
??? centum_vp_control.h/.c     ? PID, sequences, LC64, selector, split-range
?   ??? depends on: mini-pid-control-engineering
?
??? centum_vp_communication.h/.c ? Vnet/IP, OPC, Modbus, FF H1
?   ??? depends on: mini-industrial-communication-protocol
?
??? centum_vp_redundancy.h/.c  ? Pair-and-Spare, failover, availability
?   ??? depends on: mini-dcs-redundancy-failover
?
??? centum_vp_batch.h/.c       ? ISA-88 batch, recipe, EBR
?   ??? depends on: mini-batch-process-control-isa88
?
??? centum_vp_formal.lean      ? Lean 4 formal verification
    ??? depends on: Lean 4 core (Nat/Int/inductive types)
```

## Postrequisites

```
mini-yokogawa-centum-vp (this module)
?
??? mini-advanced-process-control-apc
?   ??? MPC integration with CENTUM VP
?
??? mini-safety-instrumented-system
?   ??? ProSafe-RS SFC integration
?
??? mini-scada-hmi-engineering
?   ??? HIS operator interface design
?
??? mini-industrial-ai-control-fusion
    ??? AI-enhanced batch optimization
```
