# Course Tree — mini-dcs-alarm-management-isa182

## Prerequisites
- mini-dcs-system-architecture (DCS fundamentals)
- mini-pid-control-engineering (PID control, deadband)
- mini-plc-iec61131-fundamentals (PLC scan cycles)
- mini-industrial-measurement-actuator (sensor accuracy, noise)

## Dependencies (this module requires)
`
  Industrial Measurement (L1-L2)
         |
  PID Control Engineering (L1-L3)
         |
  DCS System Architecture (L1-L2)
         |
  PLC/SCADA Fundamentals (L2-L3)
         |
  [THIS MODULE] ISA-18.2 Alarm Management
         |
  +--------+--------+--------+
  |        |        |        |
  SIS      HMI      APC      IT/OT Security
  (IEC     (ISA-101)(MPC)    (IEC 62443)
   61508/11)
`

## Knowledge Flow
1. **L1-L2**: Understand alarm types, priorities, and states
2. **L3**: Master alarm database and state machine operations
3. **L4**: Theorems about alarm system properties (determinism, bounds)
4. **L5**: Algorithms for detection, flooding, chattering, KPIs
5. **L6**: End-to-end examples (rationalization, live engine, flood analysis)
6. **L7**: Regulatory compliance (FDA 21 CFR Part 11 audit trails)
7. **L8-L9**: Advanced topics and research frontiers