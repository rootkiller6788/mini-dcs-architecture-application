# Knowledge Graph — mini-dcs-alarm-management-isa182

## L1: Definitions
| # | Concept | Type | File |
|---|---------|------|------|
| 1 | Alarm Priority (CRITICAL/HIGH/MEDIUM/LOW) | typedef enum | alarm_management_types.h |
| 2 | Alarm Type (HIGH/LOW/HI_HI/LO_LO/DEV/ROC/BAD/SYS) | typedef enum | alarm_management_types.h |
| 3 | Alarm State Machine (5 states) | typedef enum | alarm_management_types.h |
| 4 | ISA-18.2 Lifecycle (9 phases A-I) | typedef enum | alarm_management_types.h |
| 5 | Alarm Class (ALARM/ALERT/PROMPT/NO_ALARM) | typedef enum | alarm_management_types.h |
| 6 | Consequence Severity (4 levels) | typedef enum | alarm_management_types.h |
| 7 | Response Urgency (4 levels) | typedef enum | alarm_management_types.h |
| 8 | Alarm Configuration (per-alarm settings) | typedef struct | alarm_management_types.h |
| 9 | Alarm Event Record | typedef struct | alarm_management_types.h |
| 10 | Shelving Record | typedef struct | alarm_management_types.h |
| 11 | Rationalization Record | typedef struct | alarm_management_types.h |
| 12 | KPI Counters | typedef struct | alarm_management_types.h |
| 13 | Operator Response Record | typedef struct | alarm_management_types.h |
| 14 | Chattering Detector | typedef struct | alarm_management_types.h |
| 15 | Flood Detector | typedef struct | alarm_management_types.h |
| 16 | Master Alarm Database (MAD) | typedef struct | alarm_management_types.h |
| 17 | ISA-101 HMI Color Codes | function | alarm_management_types.c |
| 18 | Audit Entry Structure | typedef struct | alarm_audit_trail.h |
| 19 | MOC Record Structure | typedef struct | alarm_audit_trail.h |
| 20 | Frequent Alarm Entry | typedef struct | alarm_management_types.h |

## L2: Core Concepts
| # | Concept | Implementation |
|---|---------|----------------|
| 1 | Priority Assignment Matrix (4x4) | isa18_assign_priority_matrix() |
| 2 | Alarm Justification (4 criteria) | isa18_check_alarm_justified() |
| 3 | Alarm Shelving (timed suppression) | isa18_shelve_alarm() |
| 4 | Plant State-Based Suppression | isa18_suppression_by_plant_state() |
| 5 | Shelve Approval Requirements | isa18_check_shelve_approval_required() |
| 6 | Deadband/Hysteresis Application | isa18_apply_deadband() |
| 7 | Alarm State Utility Functions | isa18_alarm_state_is_active/rtn/acknowledged |
| 8 | Activation Eligibility Rules | isa18_alarm_can_activate() |
| 9 | Operator Acknowledgment | isa18_engine_acknowledge() |
| 10 | Manual Unshelving | isa18_unshelve_alarm() |
| 11 | Alarm Shelving Workflow | Full workflow in shelving_suppression.c |

## L3: Engineering Structures
| # | Structure | Implementation |
|---|-----------|----------------|
| 1 | Master Alarm Database (MAD) | mad_init/add/find/remove/validate |
| 2 | 5-State Alarm State Machine | isa18_alarm_state_transition() |
| 3 | Event Generation | isa18_engine_generate_event() |
| 4 | Annunciator Active List | isa18_engine_get_active_list() |
| 5 | Rationalization Record Lifecycle | init/set_outcome/team_add/apply |
| 6 | Alarm Config Lifecycle | init/set_high/set_low/copy/validate |
| 7 | On/Off Delay Timer | isa18_compute_on/off_delay_expiry() |
| 8 | Priority Numeric Conversion | isa18_priority_to_numeric() |
| 9 | Audit Trail Ring Buffer | audit_init/log_event/query_by_* |
| 10 | Shelving Record Management | shelve/unshelve/auto_unshelve_expired |
| 11 | KPI Counter Initialization | isa18_kpi_init() |

## L4: Engineering Standards / Theorems
| # | Theorem / Standard | Source |
|---|-------------------|--------|
| 1 | Priority Matrix Totality | priority_matrix_total (Lean) |
| 2 | Deterministic State Transitions | alarm_state_transition_total (Lean) |
| 3 | Suppressed Alarms Do Not Activate | suppressed_alarm_does_not_activate (Lean) |
| 4 | Shelved Alarms Do Not Activate | shelved_alarm_does_not_activate (Lean) |
| 5 | Cleared Returns to Normal | cleared_returns_to_normal (Lean) |
| 6 | Shelving Duration Bounded | validShelveDuration (Lean) |
| 7 | Health Score Bounded [0,100] | health_score_bounded (Lean) |
| 8 | Flood Counter Monotonic | flood_counter_monotonic (Lean) |
| 9 | Max Safe Response Time Formula | isa18_calc_max_safe_response_time() |
| 10 | Response Time Safety Margin | isa18_calc_response_time_margin() |
| 11 | 2oo3 Discrepancy Detection | isa18_check_discrepancy_alarm() |
| 12 | Audit Hash Chain Integrity | isa18_audit_verify_chain() |
| 13 | Regulatory Compliance Check | isa18_audit_compliance_check() |

## L5: Algorithms / Methods
| # | Algorithm | Implementation |
|---|-----------|----------------|
| 1 | Full Alarm Engine Scan Cycle | isa18_engine_scan() |
| 2 | Chattering Detection (threshold) | isa18_kpi_detect_chattering() |
| 3 | Flood Detection (rolling window) | isa18_flood_detector_update() |
| 4 | Flood End Detection | isa18_flood_detector_check() |
| 5 | Alarms Per Day Calculation | isa18_kpi_calc_alarms_per_day() |
| 6 | Peak Rate (sliding window) | isa18_kpi_calc_peak_rate() |
| 7 | Priority-Based Alarm Sorting | isa18_engine_priority_sort() |
| 8 | Top-N Frequent Alarms | isa18_kpi_top_n_frequent() |
| 9 | Nuisance Alarm Detection | isa18_kpi_detect_nuisance_alarm() |
| 10 | EEMUA 191 Benchmark Assessment | isa18_kpi_assess_eemua_benchmark() |
| 11 | Composite Health Score | isa18_kpi_overall_health_score() |
| 12 | Standing Alarm Detection | isa18_kpi_count_standing_alarms() |
| 13 | Rolling 24-Hour Window | isa18_kpi_rolling_window_update() |
| 14 | Average Response Time | isa18_kpi_calc_avg_response_time() |
| 15 | Stale Alarm Detection | isa18_engine_count_stale_alarms() |
| 16 | KPI Report Generation | isa18_kpi_generate_report() |

## L6: Canonical Problems
| # | Problem | Example |
|---|---------|---------|
| 1 | Alarm Rationalization Workshop | example_alarm_rationalization.c |
| 2 | Live Alarm Engine Simulation (30 steps) | example_alarm_engine_live.c |
| 3 | Alarm Flood and KPI Analysis | example_alarm_flood_kpi.c |

## L7: Industrial Applications
| # | Application | Implementation |
|---|-------------|----------------|
| 1 | FDA 21 CFR Part 11 Audit Trail | isa18_audit_log_event() |
| 2 | ISA-101 HMI Color Coding | isa18_priority_color_code() |
| 3 | CSV Export for Regulatory | isa18_audit_export_csv() |
| 4 | Management of Change (MOC) | isa18_audit_moc_record() |
| 5 | Regulatory Compliance Report | isa18_audit_generate_regulatory_report() |
| 6 | Operator Shift Summary | isa18_audit_operator_shift_summary() |

## L8: Advanced Topics
| # | Topic | Status |
|---|-------|--------|
| 1 | Formal Verification of State Machine | Partial (Lean theorems) |
| 2 | Tamper-Evident Hash Chains | Partial (audit chain) |

## L9: Research Frontiers
| # | Topic | Status |
|---|-------|--------|
| 1 | IT/OT Convergence in Alarm Management | Documented |
| 2 | Autonomous Alarm Rationalization (AI) | Documented |