# Knowledge Graph — Yokogawa CENTUM VP DCS

## L1 — Definitions (Complete)

| # | Concept | C Type / Enum | Lean Type | Location |
|---|---------|---------------|-----------|----------|
| 1 | Station Types (HIS/FCS/ENG/SENG/BCV/CGW/SFC/APCS) | `centum_station_type_t` | — | `centum_vp_system.h` |
| 2 | Station Status (POWEROFF→RUNNING→FAIL) | `centum_station_status_t` | — | `centum_vp_system.h` |
| 3 | System Modes (OFFLINE/ONLINE/DEBUG/TEST/EMERGENCY) | `centum_system_mode_t` | — | `centum_vp_system.h` |
| 4 | FCS Hardware Types (KFCS2/KFCS/FFCS/LFCS/SFCS) | `centum_fcs_type_t` | — | `centum_vp_fcs.h` |
| 5 | CPU Redundancy Modes (SINGLE/DUAL_STANDBY/DUAL_ACTIVE) | `centum_fcs_cpu_mode_t` | — | `centum_vp_fcs.h` |
| 6 | I/O Bus Types (RIO/FIO/NIO/FF_H1/PROFIBUS) | `centum_fcs_iobus_type_t` | — | `centum_vp_fcs.h` |
| 7 | I/O Module Types (16 Yokogawa N-IO models) | `centum_io_module_type_t` | — | `centum_vp_fcs.h` |
| 8 | Signal Types (4-20mA/1-5V/TC/RTD/Pulse/DI/DO) | `centum_signal_type_t` | — | `centum_vp_fcs.h` |
| 9 | Signal Range Configuration | `centum_signal_range_t` | — | `centum_vp_fcs.h` |
| 10 | PID Algorithms (Velocity/Positional/I-PD/PI-D) | `centum_pid_algorithm_t` | — | `centum_vp_control.h` |
| 11 | PID Modes (MAN/AUT/CAS/PRCAS/IMAN/ROUT/RCAS) | `centum_pid_mode_t` | — | `centum_vp_control.h` |
| 12 | PID Action (Direct/Reverse) | `centum_pid_action_t` | — | `centum_vp_control.h` |
| 13 | Sequence Types (Rule/Table/ST16/SEBOL) | `centum_sequence_type_t` | — | `centum_vp_control.h` |
| 14 | Sequence States (IDLE→RUNNING→COMPLETED) | `centum_sequence_state_t` | — | `centum_vp_control.h` |
| 15 | LC64 Logic Elements (AND/OR/NOT/XOR/SR/RS/TON/TOF) | `centum_lc64_element_t` | — | `centum_vp_control.h` |
| 16 | Vnet/IP Message Types (10 message codes) | `vnet_message_type_t` | — | `centum_vp_communication.h` |
| 17 | OPC Data Types (INT16→STRING) | `opc_data_type_t` | — | `centum_vp_communication.h` |
| 18 | OPC Quality Codes (GOOD/BAD/UNCERTAIN) | `opc_quality_t` | — | `centum_vp_communication.h` |
| 19 | Modbus Function Codes (8 standard functions) | `modbus_function_code_t` | — | `centum_vp_communication.h` |
| 20 | FF H1 Device Classes (Link Master/Basic/Bridge) | `ff_h1_device_class_t` | — | `centum_vp_communication.h` |
| 21 | Redundancy Roles (Primary/Standby/Offline/Syncing) | `centum_redundancy_role_t` | — | `centum_vp_redundancy.h` |
| 22 | Failover Types (Manual/Auto/Scheduled/Fault) | `centum_failover_type_t` | — | `centum_vp_redundancy.h` |
| 23 | Batch States (ISA-88 state model, 11 states) | `centum_batch_state_t` | `BatchState` | `centum_vp_batch.h` + `.lean` |
| 24 | ISA-88 Entity Levels (Procedure/Unit/Op/Phase) | `isa88_entity_level_t` | — | `centum_vp_batch.h` |
| 25 | Station Status as inductive type | — | `StationStatus` | `centum_vp_formal.lean` |
| 26 | FCS Configuration with invariants | — | `FCSConfig` | `centum_vp_formal.lean` |
| 27 | PID Block with valid-limits invariant | — | `PIDBlock` | `centum_vp_formal.lean` |
| 28 | Redundancy Pair with health invariant | — | `RedundancyPair` | `centum_vp_formal.lean` |

## L2 — Core Concepts (Complete)

| # | Concept | Implementation | Location |
|---|---------|----------------|----------|
| 1 | DCS Hierarchy (HIS→FCS→RIO/NIO) | `centum_system_config_t` + domain model | `centum_vp_system.h/.c` |
| 2 | Station Lifecycle | `centum_system_set_station_status()` | `centum_vp_system.c` |
| 3 | Control Scan Cycle | `centum_fcs_config_t.scan_cycle_us` | `centum_vp_fcs.h/.c` |
| 4 | I/O Signal Conversion (4-20mA ↔ EU) | `centum_signal_convert_raw_to_eu/eu_to_raw()` | `centum_vp_fcs.c` |
| 5 | PID Bumpless Transfer | `centum_pid_block_bumpless_transfer()` | `centum_vp_control.c` |
| 6 | PID Anti-Windup (Clamping) | `centum_pid_block_anti_windup_clamp()` | `centum_vp_control.c` |
| 7 | PID Mode Transitions (MAN↔AUT↔CAS) | `centum_pid_block_set_mode()` | `centum_vp_control.c` |
| 8 | Interlock Logic (LC64) | `centum_lc64_execute()` | `centum_vp_control.c` |
| 9 | Signal Selector | `centum_selector_block_evaluate()` | `centum_vp_control.c` |
| 10 | Split-Range Control | `centum_split_range_calculate()` | `centum_vp_control.c` |
| 11 | Ratio Control | `centum_ratio_block_calculate()` | `centum_vp_control.c` |
| 12 | Pair-and-Spare Redundancy | `centum_redundancy_*()` | `centum_vp_redundancy.h/.c` |
| 13 | Batch Command State Machine | `centum_batch_command()` | `centum_vp_batch.c` |
| 14 | ISA-88 Procedural Model | `centum_batch_*_procedure/up/op/phase()` | `centum_vp_batch.c` |

## L3 — Engineering Structures (Complete)

| # | Structure | Implementation | Location |
|---|-----------|----------------|----------|
| 1 | Project Database Segments (13 types) | `centum_db_segment_type_t` | `centum_vp_system.h` |
| 2 | Domain/Station Configuration | `centum_domain_config_t`, `centum_station_t` | `centum_vp_system.h/.c` |
| 3 | Vnet/IP Network Addressing (172.16.x.y) | `centum_vnet_calc_ip_address()` | `centum_vp_system.c` |
| 4 | N-IO Node Topology | `centum_nio_node_t` | `centum_vp_fcs.h/.c` |
| 5 | I/O Module Slot Management | `centum_fcs_add/remove_io_module()` | `centum_vp_fcs.c` |
| 6 | Sequence Step Definition | `centum_sequence_step_t` | `centum_vp_control.h/.c` |
| 7 | LC64 Logic Element Configuration | `centum_lc64_block_t` | `centum_vp_control.h/.c` |
| 8 | Vnet/IP Packet Framing | `vnet_packet_header_t` | `centum_vp_communication.h/.c` |
| 9 | Modbus RTU Frame Construction | `modbus_build_request_frame()` | `centum_vp_communication.c` |
| 10 | Modbus Response Parsing | `modbus_parse_response_frame()` | `centum_vp_communication.c` |
| 11 | FF H1 LAS Schedule | `ff_h1_schedule_rebuild()` | `centum_vp_communication.c` |
| 12 | CGW (Comm Gateway) Configuration | `centum_cgw_config_t` | `centum_vp_communication.h/.c` |
| 13 | Redundancy Pair Configuration | `centum_redundancy_pair_t` | `centum_vp_redundancy.h/.c` |
| 14 | Failover Event Logging (circular buffer) | `centum_failover_log_t` | `centum_vp_redundancy.h/.c` |
| 15 | Recipe/Formula Definition | `centum_batch_recipe_t` | `centum_vp_batch.h/.c` |

## L4 — Engineering Laws (Complete)

| # | Law/Standard | Implementation | Location |
|---|-------------|----------------|----------|
| 1 | 4-20mA Linearity (affine map) | `signal_conversion_linear` theorem | `centum_vp_formal.lean` |
| 2 | Scan Cycle Total Order | `centum_scan_monotone` theorem | `centum_vp_formal.lean` |
| 3 | Pair Redundancy Availability (1oo2) | `pair_redundancy_availability` theorem | `centum_vp_formal.lean` |
| 4 | PID Bumpless Property (zero error→zero ΔMV) | `pid_velocity_bumpless` theorem | `centum_vp_formal.lean` |
| 5 | Vnet/IP Addressing Injectivity | `vnet_addressing_injective` theorem | `centum_vp_formal.lean` |
| 6 | Batch State Transition Determinism | `batch_state_transitions_are_deterministic` | `centum_vp_formal.lean` |
| 7 | CRC-16 Single-Bit Error Detection | `crc16_detects_single_bit` theorem | `centum_vp_formal.lean` |
| 8 | Redundancy Pair Health Invariant | `redundancy_pair_health_invariant` theorem | `centum_vp_formal.lean` |
| 9 | Station ID Validity | `station_id_zero_invalid` theorem | `centum_vp_formal.lean` |
| 10 | MTBF/MTTR Availability (IEC 61508) | `centum_redundancy_calculate_availability()` | `centum_vp_redundancy.c` |

## L5 — Algorithms (Complete)

| # | Algorithm | Implementation | Location |
|---|-----------|----------------|----------|
| 1 | PID Velocity Algorithm | `centum_pid_block_execute()` | `centum_vp_control.c` |
| 2 | PID Positional Algorithm | `centum_pid_block_execute()` (alt path) | `centum_vp_control.c` |
| 3 | Anti-Windup Clamping | `centum_pid_block_anti_windup_clamp()` | `centum_vp_control.c` |
| 4 | Bumpless Transfer | `centum_pid_block_bumpless_transfer()` | `centum_vp_control.c` |
| 5 | Alarm Evaluation (DV/VH/VL/VP) | `centum_pid_block_handle_alarms()` | `centum_vp_control.c` |
| 6 | SEBOL Sequence Execution | `centum_sequence_execute()` | `centum_vp_control.c` |
| 7 | LC64 Boolean Logic Evaluation | `centum_lc64_execute()` | `centum_vp_control.c` |
| 8 | Signal Selection (High/Low/Mid/Avg) | `centum_selector_block_evaluate()` | `centum_vp_control.c` |
| 9 | Split-Range Calculation | `centum_split_range_calculate()` | `centum_vp_control.c` |
| 10 | Ratio Block Calculation | `centum_ratio_block_calculate()` | `centum_vp_control.c` |
| 11 | CRC-16 CCITT | `vnet_calculate_crc16()` | `centum_vp_communication.c` |
| 12 | Modbus CRC-16 | `modbus_crc16()` | `centum_vp_communication.c` |
| 13 | FF H1 Macrocycle Scheduling | `ff_h1_schedule_rebuild()` | `centum_vp_communication.c` |
| 14 | Failover Execution | `centum_redundancy_perform_failover()` | `centum_vp_redundancy.c` |
| 15 | Switchover Time Estimation | `centum_redundancy_switchover_time_estimate()` | `centum_vp_redundancy.c` |
| 16 | Recipe Linear Scaling | `centum_batch_scale_recipe()` | `centum_vp_batch.c` |
| 17 | Batch Sequential Engine | `centum_batch_execute()` | `centum_vp_batch.c` |
| 18 | Equipment Availability Arbitration | `centum_batch_check_equipment_availability()` | `centum_vp_batch.c` |

## L6 — Canonical Problems (Complete)

| # | Problem | Example | Location |
|---|---------|---------|----------|
| 1 | Temperature PID Control | Reactor heating with bumpless transfer | `examples/example_pid_control_loop.c` |
| 2 | DCS Redundancy Failover | Pair-and-Spare CPU failover scenario | `examples/example_redundancy_failover.c` |
| 3 | Batch Reactor Process | ISA-88 recipe, scale, execute, EBR | `examples/example_batch_reactor.c` |

## L7 — Industrial Applications (Partial+)

| # | Application | Implementation | Location |
|---|------------|----------------|----------|
| 1 | CENTUM VP R6 System Architecture | `centum_system_config_t` | `centum_vp_system.h/.c` |
| 2 | Yokogawa KFCS2 Controller | `centum_fcs_config_t` | `centum_vp_fcs.h/.c` |
| 3 | N-IO Modular I/O System | `centum_nio_node_t`, `centum_io_module_t` | `centum_vp_fcs.h/.c` |
| 4 | Exaopc OPC Server Integration | `opc_item_value_t`, CGW OPC groups | `centum_vp_communication.h/.c` |
| 5 | Foundation Fieldbus H1 (ALF111) | `ff_h1_segment_t` | `centum_vp_communication.h/.c` |
| 6 | Electronic Batch Record (21 CFR 11) | `centum_batch_generate_report()` | `centum_vp_batch.c` |

## L8 — Advanced Topics (Partial+)

| # | Topic | Implementation | Location |
|---|-------|----------------|----------|
| 1 | Deterministic Control Network (Vnet/IP) | `vnet_packet_header_t`, CRC, stats | `centum_vp_communication.h/.c` |
| 2 | Reliability Engineering (MTBF/MTTR) | `centum_redundancy_mtbf/mttr()` | `centum_vp_redundancy.c` |
| 3 | System Availability Analysis | `centum_redundancy_calculate_availability()` | `centum_vp_redundancy.c` |
| 4 | FB Execution Statistics & Overrun Detection | `centum_fcs_compute_exec_stats()` | `centum_vp_fcs.c` |
| 5 | Formal Verification (Lean 4) | 10 theorems in `centum_vp_formal.lean` | `src/centum_vp_formal.lean` |

## L9 — Industry Frontiers (Partial)

| # | Topic | Documentation |
|---|-------|--------------|
| 1 | IT/OT Convergence (OPC UA, Exaquantum) | `centum_vp_communication.h` |
| 2 | Autonomous Operations (ISA-88 procedural automation) | `centum_vp_batch.h/.c` |
| 3 | Digital Twin (CENTUM VP simulation mode) | `centum_station_status_t` SIMULATE state |