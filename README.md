# Mini DCS Architecture & Application

A collection of **from-scratch, zero-dependency C implementations** of industrial Distributed Control System (DCS) architectures, fieldbus protocols, alarm management standards, and commercial DCS platform models. Each module maps to MIT, Stanford, CMU, and RWTH Aachen courses, bridging industrial automation theory with runnable C code.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|-----------|--------|-------------|
| [mini-dcs-alarm-management-isa182](mini-dcs-alarm-management-isa182/) | ISA-18.2 alarm lifecycle, deadband detection, alarm rationalization, KPI metrics, audit trail | MIT 6.302, ISA-18.2 / IEC 62682 |
| [mini-dcs-redundancy-failover](mini-dcs-redundancy-failover/) | Controller/network redundancy, failover state machines, availability modeling, diagnostics, state synchronization | MIT 6.302, CMU 24-677, RWTH Aachen |
| [mini-dcs-system-architecture](mini-dcs-system-architecture/) | ISA-95 level mapping, DCS alarm management, controller redundancy, SIS integration (IEC 61508/61511), system configuration database | MIT 2.171, ISA-95, IEC 61508 |
| [mini-emerson-deltav-system](mini-emerson-deltav-system/) | M/S-series controllers, CHARMs electronic marshalling, ISA-88 batch control, ACN communication, 1:1 redundancy | MIT 2.171, Purdue ME575, ISA-88 |
| [mini-fieldbus-foundation-h1](mini-fieldbus-foundation-h1/) | H1 physical layer (IEC 61158-2), LAS scheduling, FBAP/FMS application layer, device interoperability, segment engineering | MIT 2.171, Stanford ENGR205, CMU 24-677 |
| [mini-honeywell-experion-pks](mini-honeywell-experion-pks/) | C300 controller scan/execution, CEE scheduling, control blocks, CAB bulk I/O, HMIWeb display, trend system | MIT 6.302, MIT 2.171, CMU 18-771, CMU 24-677 |
| [mini-supcon-ecs700-dcs](mini-supcon-ecs700-dcs/) | SCnet redundant Ethernet, control station architecture, SBUS I/O subsystem, multi-layer redundancy, ECS-700 system core | MIT 2.171, CMU 24-677 |
| [mini-yokogawa-centum-vp](mini-yokogawa-centum-vp/) | Vnet/IP communication, FCS field control station, PID control blocks, paired redundancy, ISA-88 batch management | MIT 2.171, MIT 6.302, CMU 24-677, ISA-88 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Industry standard alignment** — every module is mapped to real industrial standards (ISA-18.2, ISA-88, ISA-95, IEC 61158, IEC 61508, IEC 61511)
- **Real platform modeling** — four commercial DCS platforms (DeltaV, Experion PKS, ECS-700, CENTUM VP) with actual hardware definitions, scan cycles, and redundancy topologies

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-dcs-alarm-management-isa182
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-dcs-architecture-application/
├── mini-dcs-alarm-management-isa182/   # ISA-18.2 alarm lifecycle and KPI metrics
├── mini-dcs-redundancy-failover/       # Redundancy, failover, and availability models
├── mini-dcs-system-architecture/       # ISA-95 level mapping and DCS core architecture
├── mini-emerson-deltav-system/         # Emerson DeltaV M/S-series controller models
├── mini-fieldbus-foundation-h1/        # Foundation Fieldbus H1 protocol stack (IEC 61158)
├── mini-honeywell-experion-pks/        # Honeywell Experion PKS C300/CEE platform
├── mini-supcon-ecs700-dcs/             # SUPCON ECS-700 SCnet / control station models
└── mini-yokogawa-centum-vp/            # Yokogawa CENTUM VP Vnet/IP / FCS models
```

## License

MIT
