# Foundation Fieldbus H1 — Knowledge Graph

## Module: mini-fieldbus-foundation-h1

### L1: Definitions — Complete ✅

| # | Topic | C Definition | Location |
|---|-------|-------------|----------|
| 1 | H1 bit rate (31.25 kbit/s) | `FF_H1_BITRATE_HZ` | `include/ff_h1_physical.h` |
| 2 | Manchester encoding (bit 1→[1,0], bit 0→[0,1]) | `ff_manchester_encode()` | `include/ff_h1_physical.h` |
| 3 | H1 frame structure (Preamble+SD+DLPDU+ED) | `ff_h1_frame_t` | `include/ff_h1_physical.h` |
| 4 | Cable types A/B/C/D (IEC 61158-2) | `ff_cable_type_t`, `ff_cable_spec_t` | `include/ff_h1_physical.h` |
| 5 | Start/End Delimiters (code violation) | `ff_start_delimiter_write()` | `include/ff_h1_physical.h` |
| 6 | CRC-16-CCITT generator polynomial 0x1021 | `ff_crc16_ccitt()` | `include/ff_h1_physical.h` |
| 7 | DL-address ranges (permanent 0x10-0xFB) | `FF_DL_ADDR_PERM_MIN` | `include/ff_h1_datalink.h` |
| 8 | LAS (Link Active Scheduler) | `ff_las_context_t` | `include/ff_h1_datalink.h` |
| 9 | CD Schedule entry | `ff_cd_entry_t`, `ff_cd_schedule_t` | `include/ff_h1_datalink.h` |
| 10 | VCR types (BNU/QUB/QUU) | `ff_vcr_type_t` | `include/ff_h1_datalink.h` |
| 11 | Function Block types (AI/AO/DI/DO/PID/...) | `ff_fb_type_t` | `include/ff_h1_application.h` |
| 12 | MODE_BLK (OOS/MAN/AUTO/CAS/RCAS/ROUT) | `ff_block_mode_t`, `ff_mode_blk_t` | `include/ff_h1_application.h` |
| 13 | Block Error flags | `FF_BLKERR_*` | `include/ff_h1_application.h` |
| 14 | SM states (UNINITIALIZED→OPERATIONAL→FAULT) | `ff_sm_state_t` | `include/ff_h1_system_mgmt.h` |
| 15 | Device Identity (32-byte device ID) | `ff_device_identity_t` | `include/ff_h1_device.h` |
| 16 | ITK versions and features | `ff_itk_version_t` | `include/ff_h1_device.h` |
| 17 | DD (Device Description) variables | `ff_dd_variable_t` | `include/ff_h1_device.h` |
| 18 | CFF capabilities | `ff_cff_capabilities_t` | `include/ff_h1_device.h` |
| 19 | FISCO/FNICO IS types | `ff_is_type_t`, `ff_entity_params_t` | `include/ff_h1_segment.h` |
| 20 | Segment commissioning checklist | `ff_commissioning_checklist_t` | `include/ff_h1_segment.h` |

### L2: Core Concepts — Complete ✅

| # | Concept | Implementation | Location |
|---|---------|---------------|----------|
| 1 | LAS bus arbitration (scheduled + unscheduled) | `ff_las_run_macrocycle()` | `src/ff_h1_datalink.c` |
| 2 | Token passing (round-robin) | `ff_live_list_next_token()` | `src/ff_h1_datalink.c` |
| 3 | Live List management | `ff_live_list_add/find/remove()` | `src/ff_h1_datalink.c` |
| 4 | Link Master election (priority-based) | `ff_lm_election_compare()` | `src/ff_h1_datalink.c` |
| 5 | MODE_BLK state machine (mode shedding) | `ff_mode_transition_allowed()`, `ff_mode_determine_actual()` | `src/ff_h1_application.c` |
| 6 | Function Block execution engine | `ff_fb_execute()` | `src/ff_h1_application.c` |
| 7 | Link Object validation | `ff_link_validate()` | `src/ff_h1_application.c` |
| 8 | Find Tag Query service | `ff_sm_find_tag_match()` | `src/ff_h1_system_mgmt.c` |
| 9 | Set Address protocol | `ff_sm_process_set_address()` | `src/ff_h1_system_mgmt.c` |
| 10 | FMS Read/Write services | `ff_fms_read/write_parameter()` | `src/ff_h1_application.c` |
| 11 | Object Dictionary lookup | `ff_od_lookup()` | `src/ff_h1_application.c` |
| 12 | ITK compliance checking | `ff_itk_check_compliance()` | `src/ff_h1_device.c` |

### L3: Engineering Structures — Complete ✅

| # | Structure | Implementation | Location |
|---|-----------|---------------|----------|
| 1 | Manchester bit-level encoding/decoding | `ff_manchester_encode/decode()` | `src/ff_h1_physical.c` |
| 2 | CRC-16 table-driven computation | `crc16_init_table()`, `ff_crc16_ccitt()` | `src/ff_h1_physical.c` |
| 3 | H1 frame assembly (octet-level) | `ff_h1_frame_assemble()` | `src/ff_h1_physical.c` |
| 4 | DL-PDU structure (FC + addresses + FCS) | `ff_dl_pdu_t` | `include/ff_h1_datalink.h` |
| 5 | FMS PDU header (service + invoke_id + OD) | `ff_fms_header_t` | `include/ff_h1_application.h` |
| 6 | SM agent state machine transitions | `ff_sm_init/start/set_operational()` | `src/ff_h1_system_mgmt.c` |
| 7 | Time Distribution message processing | `ff_sm_process_td()` | `src/ff_h1_system_mgmt.c` |
| 8 | NM statistics counters | `ff_nm_statistics_t` | `src/ff_h1_system_mgmt.c` |
| 9 | SMIB read interface | `ff_smib_read()` | `src/ff_h1_system_mgmt.c` |

### L4: Engineering Laws/Standards (Formal) — Complete ✅

| # | Theorem/Property | Formalization | Location |
|---|-----------------|--------------|----------|
| 1 | Manchester encoding bit-level injectivity | `manchester_encode_bit_injective` | `src/ff_h1_formal.lean` |
| 2 | Manchester byte encoding length = 16 | `manchester_encode_byte_length` | `src/ff_h1_formal.lean` |
| 3 | CRC-16 polynomial nonzero | `crc16_polynomial_nonzero` | `src/ff_h1_formal.lean` |
| 4 | Per-addr range count = 236 | `permanent_address_count_eq_236` | `src/ff_h1_formal.lean` |
| 5 | OOS always reachable from any mode | `oos_always_reachable` | `src/ff_h1_formal.lean` |
| 6 | MAN→CAS transition not directly allowed | `man_to_cas_invalid` | `src/ff_h1_formal.lean` |
| 7 | Schedule determinism (empty) | `empty_schedule_ordered` | `src/ff_h1_formal.lean` |
| 8 | Singleton schedule ordered | `singleton_schedule_ordered` | `src/ff_h1_formal.lean` |

### L5: Algorithms/Methods — Complete ✅

| # | Algorithm | Implementation | Location |
|---|-----------|---------------|----------|
| 1 | LAS macrocycle CD scheduling | `ff_las_run_macrocycle()` | `src/ff_h1_datalink.c` |
| 2 | LAS CD utilization computation | `ff_las_cd_utilization()` | `src/ff_h1_datalink.c` |
| 3 | Link Master hold-off computation | `ff_lm_holdoff_ms()` | `src/ff_h1_datalink.c` |
| 4 | PID ISA standard form (velocity algorithm) | `ff_fb_pid_algorithm()` | `src/ff_h1_application.c` |
| 5 | AI signal processing chain | `ff_fb_ai_algorithm()` | `src/ff_h1_application.c` |
| 6 | Ratio block algorithm | `ff_fb_ratio_algorithm()` | `src/ff_h1_application.c` |

### L6: Canonical Problems — Complete ✅

| # | Problem | Example | Location |
|---|---------|---------|----------|
| 1 | H1 segment DC power budget design | `example_segment_design.c` | `examples/` |
| 2 | LAS CD schedule design & macrocycle simulation | `example_las_schedule.c` | `examples/` |
| 3 | Device commissioning sequence | `example_device_commission.c` | `examples/` |
| 4 | Segment health diagnostics | `ff_segment_health_evaluate()` | `src/ff_h1_segment.c` |
| 5 | Maximum trunk length estimation | `ff_max_trunk_length()` | `src/ff_h1_segment.c` |
| 6 | Spur topology validation | `ff_segment_validate_spurs()` | `src/ff_h1_segment.c` |

### L7: Industrial Applications — Partial+ ✅

| # | Application | Implementation | Location |
|---|------------|---------------|----------|
| 1 | Emerson/Rosemount device commissioning | `example_device_commission.c` | `examples/` |
| 2 | Middle East gas plant segment design | `example_segment_design.c` | `examples/` |
| 3 | Flow control loop LAS scheduling | `example_las_schedule.c` | `examples/` |

### L8: Advanced Topics — Partial+ ✅

| # | Topic | Implementation | Location |
|---|-------|---------------|----------|
| 1 | FISCO intrinsic safety compatibility | `ff_fisco_verify_compatibility()` | `src/ff_h1_segment.c` |
| 2 | Temperature-compensated cable resistance | `ff_cable_loop_resistance()` | `src/ff_h1_segment.c` |

### L9: Research Frontiers — Partial ✅

| # | Topic | Status |
|---|-------|--------|
| 1 | APL (Advanced Physical Layer) — Ethernet to field | Documented in `docs/` |
| 2 | FDI (Field Device Integration) — DD evolution | ITK 7.0 feature flags present |
| 3 | NOA (NAMUR Open Architecture) | Documented reference |

---

## Module Status: COMPLETE ✅

- **L1**: Complete (20 definitions)
- **L2**: Complete (12 core concepts)
- **L3**: Complete (9 engineering structures)
- **L4**: Complete (8 formal theorems in Lean 4)
- **L5**: Complete (6 algorithms)
- **L6**: Complete (6 canonical problems)
- **L7**: Partial+ (3 industrial applications)
- **L8**: Partial+ (2 advanced topics)
- **L9**: Partial (documented frontiers)

**include/ + src/ line count**: ≥ 4400 lines