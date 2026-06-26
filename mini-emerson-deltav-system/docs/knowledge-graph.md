# Knowledge Graph -- Emerson DeltaV DCS

## L1 -- Definitions (Complete)

| # | Concept | C Type / Enum | Location |
|---|---------|---------------|----------|
| 1 | Node Types (9) | `delta_v_node_type_t` | `delta_v_system.h` |
| 2 | Node Status (8) | `delta_v_node_status_t` | `delta_v_system.h` |
| 3 | System Modes (5) | `delta_v_system_mode_t` | `delta_v_system.h` |
| 4 | Database Segments (13) | `delta_v_db_segment_t` | `delta_v_system.h` |
| 5 | DB Check Results (9) | `delta_v_db_check_t` | `delta_v_system.h` |
| 6 | License Features (18) | `delta_v_license_feature_t` | `delta_v_system.h` |
| 7 | Controller Types (7) | `delta_v_controller_type_t` | `delta_v_controller.h` |
| 8 | CHARM Signal Types (13) | `delta_v_charm_signal_type_t` | `delta_v_controller.h` |
| 9 | I/O Bus Types (6) | `delta_v_io_bus_type_t` | `delta_v_controller.h` |
| 10 | Controller Modes (5) | `delta_v_controller_mode_t` | `delta_v_controller.h` |
| 11 | PID Forms (3) | `delta_v_pid_form_t` | `delta_v_control.h` |
| 12 | PID Modes (8) | `delta_v_pid_mode_t` | `delta_v_control.h` |
| 13 | PID Action (2) | `delta_v_pid_action_t` | `delta_v_control.h` |
| 14 | Selector Types (6) | `delta_v_selector_type_t` | `delta_v_control.h` |
| 15 | Sequence Step Types (7) | `delta_v_seq_step_type_t` | `delta_v_control.h` |
| 16 | MPC Algorithms (3) | `delta_v_mpc_algorithm_t` | `delta_v_control.h` |
| 17 | Fuzzy Set Shapes (4) | `delta_v_fuzzy_shape_t` | `delta_v_control.h` |
| 18 | Control Module Types (13) | `delta_v_cmod_type_t` | `delta_v_control.h` |
| 19 | ACN Message Types (15) | `delta_v_acn_msg_type_t` | `delta_v_communication.h` |
| 20 | ACN Priority (5) | `delta_v_acn_priority_t` | `delta_v_communication.h` |
| 21 | OPC Data Types (12) | `delta_v_opc_data_type_t` | `delta_v_communication.h` |
| 22 | OPC Quality Codes (8) | `delta_v_opc_quality_t` | `delta_v_communication.h` |
| 23 | Modbus Function Codes (8) | `delta_v_modbus_func_t` | `delta_v_communication.h` |
| 24 | FF H1 Device Classes (3) | `delta_v_ff_h1_device_class_t` | `delta_v_communication.h` |
| 25 | Redundancy Roles (6) | `delta_v_redundancy_role_t` | `delta_v_redundancy.h` |
| 26 | Failover Types (4) | `delta_v_failover_type_t` | `delta_v_redundancy.h` |
| 27 | Redundancy Pair Types (5) | `delta_v_redundancy_pair_type_t` | `delta_v_redundancy.h` |
| 28 | Sync States (6) | `delta_v_sync_state_t` | `delta_v_redundancy.h` |
| 29 | Batch States (12) | `delta_v_batch_state_t` | `delta_v_batch.h` |
| 30 | Batch Commands (8) | `delta_v_batch_command_t` | `delta_v_batch.h` |
| 31 | ISA-88 Entity Levels (4) | `isa88_entity_level_t` | `delta_v_batch.h` |

## L2 -- Core Concepts (Complete)

| # | Concept | Implementation |
|---|---------|----------------|
| 1 | DCS Hierarchy | `delta_v_system_config_t` + area/node model |
| 2 | Node State Machine | `delta_v_is_valid_status_transition()` |
| 3 | CHARMs Electronic Marshalling | Carrier/CHARM/Channel config |
| 4 | I/O Signal Conversion | `delta_v_signal_convert_raw_to_eu/eu_to_raw()` |
| 5 | PID Bumpless Transfer | `delta_v_pid_block_bumpless_transfer()` |
| 6 | PID Anti-Windup | `delta_v_pid_block_anti_windup()` |
| 7 | PID Mode Transitions | `delta_v_pid_block_set_mode()` |
| 8 | Split-Range Control | `delta_v_split_range_calculate()` |
| 9 | Ratio Control | `delta_v_ratio_block_calculate()` |
| 10 | 1:1 Controller Redundancy | `delta_v_redundancy_*()` |
| 11 | Batch State Machine | `delta_v_batch_issue_command()` |
| 12 | ISA-88 Procedural Model | `delta_v_recipe_*()` |
| 13 | 2oo3 Voting | `delta_v_2oo3_vote()` |
| 14 | Lead-Lag Compensation | `delta_v_lead_lag_calculate()` |
