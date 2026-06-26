/**
 * @file dcs_types.h
 * @brief Core type definitions for DCS (Distributed Control System) architecture.
 *
 * Knowledge Level: L1 Definitions + L3 Engineering Structures
 *
 * References:
 *   - ISA-95: Enterprise-Control System Integration (Levels 0-4)
 *   - ISA-88: Batch Control standard
 *   - IEC 61508: Functional Safety of E/E/PE Systems
 *   - IEC 61511: Functional Safety for Process Industry
 *   - Honeywell Experion PKS Architecture Manual
 *   - Yokogawa CENTUM VP System Overview
 *
 * This header defines all fundamental data structures used across the DCS
 * architecture module: hierarchy levels, node types, signal definitions,
 * redundancy modes, communication link types, and software architecture.
 */

#ifndef DCS_TYPES_H
#define DCS_TYPES_H

#include <stdint.h>
#include <stddef.h>

/*===========================================================================
 * L1: ISA-95 Hierarchy Levels
 *===========================================================================*/

/**
 * @brief ISA-95 functional hierarchy levels for DCS.
 *
 * Level 0 (Field):      Sensors, actuators, transmitters - physical process interface.
 * Level 1 (Control):    PLCs, DCS controllers, safety controllers - basic control.
 * Level 2 (Supervisory): SCADA, operator stations, alarm management - process monitoring.
 * Level 3 (Plant/MES):  Manufacturing execution, batch management, historian.
 * Level 4 (Enterprise): ERP, business planning, logistics.
 */
typedef enum {
    DCS_LEVEL_0_FIELD       = 0,
    DCS_LEVEL_1_CONTROL     = 1,
    DCS_LEVEL_2_SUPERVISORY = 2,
    DCS_LEVEL_3_PLANT_MES   = 3,
    DCS_LEVEL_4_ENTERPRISE  = 4
} dcs_hierarchy_level_t;

/*===========================================================================
 * L1: Core Definitions - Node Types
 *===========================================================================*/

/**
 * @brief DCS node type enumeration.
 *
 * Each node type represents a distinct functional role in the distributed
 * control system architecture. The classification follows major DCS vendor
 * conventions (Honeywell Experion, Yokogawa CENTUM, Emerson DeltaV).
 */
typedef enum {
    DCS_NODE_CONTROLLER          = 0,  /* Process controller (CP) */
    DCS_NODE_IO_SUBSYSTEM        = 1,  /* Remote I/O rack */
    DCS_NODE_OPERATOR_STATION    = 2,  /* HMI client (HIS/OWS) */
    DCS_NODE_ENGINEERING_STATION = 3,  /* Engineering workstation */
    DCS_NODE_APPLICATION_STATION = 4,  /* Advanced control / OPC server */
    DCS_NODE_HISTORIAN           = 5,  /* Process data historian */
    DCS_NODE_ALARM_SERVER        = 6,  /* Dedicated alarm management */
    DCS_NODE_DOMAIN_CONTROLLER   = 7,  /* System domain controller */
    DCS_NODE_COMM_GATEWAY        = 8,  /* Protocol gateway / converter */
    DCS_NODE_SAFETY_CONTROLLER   = 9,  /* Safety logic solver */
    DCS_NODE_BATCH_SERVER        = 10, /* Batch execution server */
    DCS_NODE_ASSET_MANAGER       = 11  /* Asset management station */
} dcs_node_type_t;

/**
 * @brief DCS node state.
 */
typedef enum {
    DCS_NODE_STATE_OFFLINE     = 0,
    DCS_NODE_STATE_INITIALIZING = 1,
    DCS_NODE_STATE_STANDBY     = 2,
    DCS_NODE_STATE_ACTIVE      = 3,
    DCS_NODE_STATE_FAULT       = 4,
    DCS_NODE_STATE_MAINTENANCE = 5
} dcs_node_state_t;

/*===========================================================================
 * L1: Core Definitions - Redundancy Modes
 *===========================================================================*/

/**
 * @brief Redundancy architecture type.
 *
 * 1oo1  (1-out-of-1):  Simplex, no redundancy.
 * 1oo2  (1-out-of-2):  Dual redundant, either can run the process.
 * 2oo2  (2-out-of-2):  Both must agree (voting for safety).
 * 2oo3  (2-out-of-3):  Triple modular redundancy, 2 of 3 vote.
 * 1oo2D (1-out-of-2D): Dual with diagnostics (IEC 61508).
 */
typedef enum {
    DCS_REDUNDANCY_1OO1  = 0,
    DCS_REDUNDANCY_1OO2  = 1,
    DCS_REDUNDANCY_2OO2  = 2,
    DCS_REDUNDANCY_2OO3  = 3,
    DCS_REDUNDANCY_1OO2D = 4
} dcs_redundancy_arch_t;

/**
 * @brief Redundancy operating mode.
 */
typedef enum {
    DCS_REDUNDANCY_MODE_HOT_STANDBY   = 0,  /* Standby executes in parallel, switchover < 1 scan */
    DCS_REDUNDANCY_MODE_WARM_STANDBY  = 1,  /* Standby has database but not executing */
    DCS_REDUNDANCY_MODE_COLD_STANDBY  = 2,  /* Standby powered off, needs full init */
    DCS_REDUNDANCY_MODE_ACTIVE_ACTIVE = 3   /* Both actively processing (load sharing) */
} dcs_redundancy_mode_t;

/*===========================================================================
 * L1: Core Definitions - Signal / I/O Types
 *===========================================================================*/

/**
 * @brief Analog/digital signal types in DCS I/O subsystems.
 */
typedef enum {
    DCS_SIG_AI_4_20MA   = 0,  /* Analog Input 4-20 mA */
    DCS_SIG_AO_4_20MA   = 1,  /* Analog Output 4-20 mA */
    DCS_SIG_AI_0_10V    = 2,  /* Analog Input 0-10 V */
    DCS_SIG_AO_0_10V    = 3,  /* Analog Output 0-10 V */
    DCS_SIG_DI_24VDC    = 4,  /* Digital Input 24V DC */
    DCS_SIG_DO_24VDC    = 5,  /* Digital Output 24V DC */
    DCS_SIG_DI_120VAC   = 6,  /* Digital Input 120V AC */
    DCS_SIG_DO_120VAC   = 7,  /* Digital Output 120V AC */
    DCS_SIG_PULSE_IN    = 8,  /* Pulse/frequency input */
    DCS_SIG_PULSE_OUT   = 9,  /* Pulse/frequency output */
    DCS_SIG_RTD_PT100   = 10, /* RTD Pt100 temperature */
    DCS_SIG_TC_K        = 11, /* Thermocouple Type K */
    DCS_SIG_TC_J        = 12, /* Thermocouple Type J */
    DCS_SIG_HART        = 13, /* HART smart transmitter */
    DCS_SIG_FIELDBUS_H1 = 14, /* Foundation Fieldbus H1 */
    DCS_SIG_PROFIBUS_PA = 15  /* PROFIBUS PA */
} dcs_signal_type_t;

/**
 * @brief Signal quality flag (as per OPC DA quality).
 */
typedef enum {
    DCS_QUALITY_GOOD            = 0xC0,  /* OPC: Good quality */
    DCS_QUALITY_GOOD_LOCAL_OVRD = 0xD8,  /* Good, local override */
    DCS_QUALITY_UNCERTAIN       = 0x40,  /* Uncertain (sensor fault possible) */
    DCS_QUALITY_BAD             = 0x00,  /* Bad (hardware failure) */
    DCS_QUALITY_BAD_CONFIG      = 0x04,  /* Bad, configuration error */
    DCS_QUALITY_BAD_OFFLINE     = 0x08,  /* Bad, device offline */
    DCS_QUALITY_LAST_KNOWN      = 0x14   /* Last known value */
} dcs_signal_quality_t;

/*===========================================================================
 * L1: Core Definitions - Communication Link Types
 *===========================================================================*/

/**
 * @brief Communication link type in DCS backbone.
 */
typedef enum {
    DCS_LINK_ETHERNET_TCPIP  = 0,
    DCS_LINK_ETHERNET_IP     = 1,
    DCS_LINK_PROFIBUS_DP     = 2,
    DCS_LINK_FOUNDATION_H1   = 3,
    DCS_LINK_MODBUS_TCP      = 4,
    DCS_LINK_MODBUS_RTU      = 5,
    DCS_LINK_HART            = 6,
    DCS_LINK_OPC_UA          = 7,
    DCS_LINK_PROPRIETARY     = 8
} dcs_link_type_t;

/*===========================================================================
 * L2: Core Concepts - DCS Controller Configuration
 *===========================================================================*/

/**
 * @brief DCS controller node configuration.
 *
 * A controller node is the core processing unit at Level 1 (Control).
 * It executes the control strategy (PID loops, sequential logic, interlocking)
 * and manages I/O communication.
 *
 * Key parameters:
 *   - scan_period_ms: cyclic execution interval (typical: 50ms-1000ms)
 *   - max_loops: maximum PID loops this controller can execute
 *   - max_tags: maximum process tags in controller database
 *   - node_id: unique identifier within the DCS domain
 *   - redundancy: whether this controller has a redundant partner
 */
typedef struct {
    uint32_t             node_id;
    dcs_node_type_t      node_type;
    dcs_node_state_t     state;
    dcs_hierarchy_level_t level;
    double               scan_period_ms;
    uint32_t             max_loops;
    uint32_t             max_tags;
    uint32_t             max_io_points;
    double               cpu_load_pct;
    double               memory_used_mb;
    double               memory_total_mb;
    int                  redundancy_enabled;
    dcs_redundancy_mode_t redundancy_mode;
} dcs_controller_config_t;

/*===========================================================================
 * L2: Core Concepts - I/O Point Definition
 *===========================================================================*/

/**
 * @brief Individual I/O point configuration.
 *
 * Maps a physical field signal to the DCS tag database.
 * Each I/O point has a hardware address (rack/slot/channel),
 * signal type, engineering range, and alarm limits.
 */
typedef struct {
    uint32_t           io_point_id;
    uint32_t           rack_num;
    uint32_t           slot_num;
    uint32_t           channel_num;
    dcs_signal_type_t  signal_type;
    double             raw_min;        /* Raw ADC/DAC minimum */
    double             raw_max;        /* Raw ADC/DAC maximum */
    double             eu_min;         /* Engineering unit minimum */
    double             eu_max;         /* Engineering unit maximum */
    double             eu_value;       /* Current engineering unit value */
    double             raw_value;      /* Current raw value */
    dcs_signal_quality_t quality;
    int                enabled;
    char               tag_name[64];
    char               eu_units[16];   /* e.g., "kPa", "degC", "kg/h" */
    double             hihi_limit;     /* High-high alarm limit */
    double             hi_limit;       /* High alarm limit */
    double             lo_limit;       /* Low alarm limit */
    double             lolo_limit;     /* Low-low alarm limit */
    double             rate_limit;     /* Rate-of-change alarm limit */
} dcs_io_point_t;

/*===========================================================================
 * L3: Engineering Structures - Software Architecture Components
 *===========================================================================*/

/**
 * @brief DCS software component categorization.
 *
 * Represents the standard DCS software stack, from real-time
 * control execution to enterprise integration.
 */
typedef enum {
    DCS_SW_RTDB       = 0,  /* Real-time database */
    DCS_SW_CTRL_ENG   = 1,  /* Control engine (PID, sequence, interlock) */
    DCS_SW_HMI_ENG    = 2,  /* HMI engine (graphics, trends) */
    DCS_SW_ALARM_MGR  = 3,  /* Alarm management server */
    DCS_SW_HISTORIAN  = 4,  /* Process data historian */
    DCS_SW_EVENT_MGR  = 5,  /* Event/sequence-of-events (SOE) manager */
    DCS_SW_BATCH_EXEC = 6,  /* Batch execution engine (ISA-88) */
    DCS_SW_OPC_SERVER = 7,  /* OPC server (DA, HDA, A&E) */
    DCS_SW_CONFIG_MGR = 8,  /* Configuration management */
    DCS_SW_DIAGNOSTIC = 9,  /* System diagnostic monitor */
    DCS_SW_SECURITY   = 10  /* Security server */
} dcs_software_component_t;

/*===========================================================================
 * L3: Engineering Structures - Network Topology
 *===========================================================================*/

/**
 * @brief Communication network topology for DCS backbone.
 */
typedef enum {
    DCS_TOPOLOGY_BUS        = 0,  /* Single bus (obsolete for new systems) */
    DCS_TOPOLOGY_STAR       = 1,  /* Star topology via switches */
    DCS_TOPOLOGY_RING       = 2,  /* Ring topology with redundancy */
    DCS_TOPOLOGY_DUAL_RING  = 3,  /* Dual counter-rotating ring */
    DCS_TOPOLOGY_MESH       = 4,  /* Full mesh (highest redundancy) */
    DCS_TOPOLOGY_TREE       = 5,  /* Hierarchical tree */
    DCS_TOPOLOGY_DUAL_STAR  = 6   /* Dual-star for critical systems */
} dcs_network_topology_t;

/*===========================================================================
 * L3: Engineering Structures - DCS System Configuration
 *===========================================================================*/

/**
 * @brief Complete DCS system configuration.
 *
 * Top-level structure that aggregates all DCS subsystems into
 * a coherent system model. Used for system sizing, architecture
 * verification, and performance analysis.
 */
typedef struct {
    /* System identification */
    char     system_name[64];
    char     vendor[32];      /* e.g., "Honeywell", "Yokogawa", "Emerson" */
    char     model[32];       /* e.g., "Experion PKS", "CENTUM VP" */

    /* Hierarchy counts */
    uint32_t num_controller_nodes;
    uint32_t num_operator_stations;
    uint32_t num_engineering_stations;
    uint32_t num_io_subsystems;
    uint32_t num_application_stations;

    /* I/O capacity */
    uint32_t total_ai_points;
    uint32_t total_ao_points;
    uint32_t total_di_points;
    uint32_t total_do_points;
    uint32_t total_hart_devices;
    uint32_t total_fieldbus_devices;

    /* Network */
    dcs_network_topology_t backbone_topology;
    double                 backbone_speed_mbps;
    int                    backbone_redundant;

    /* Redundancy */
    int controller_redundancy;
    int network_redundancy;
    int power_redundancy;
    int server_redundancy;

    /* Operational parameters */
    double target_availability_pct;
    double controller_scan_ms;
    double max_network_load_pct;
    uint32_t num_process_areas;
} dcs_system_config_t;

/*===========================================================================
 * L2: Core Concepts - Time Synchronization
 *===========================================================================*/

/**
 * @brief Time synchronization quality for DCS nodes.
 *
 * Precision Time Protocol (IEEE 1588) is the standard for DCS
 * sub-millisecond synchronization. NTP is acceptable for Level 2+.
 *
 * Clock class definitions per IEEE 1588-2008:
 *   Class 6:  locked to primary reference, accuracy < 25ns
 *   Class 7:  holdover within spec for prior lock
 *   Class 248: default (free-running)
 */
typedef enum {
    DCS_TIME_SRC_GPS        = 0,
    DCS_TIME_SRC_PTP_MASTER = 1,
    DCS_TIME_SRC_PTP_SLAVE  = 2,
    DCS_TIME_SRC_NTP        = 3,
    DCS_TIME_SRC_FREE_RUN   = 4
} dcs_time_source_t;

typedef struct {
    dcs_time_source_t source;
    int               clock_class;     /* IEEE 1588 clock class */
    double            offset_ns;       /* Offset from master (ns) */
    double            drift_ppm;       /* Clock drift (parts per million) */
    int               is_synchronized;
    uint64_t          last_sync_timestamp;
} dcs_time_quality_t;

/*===========================================================================
 * L4: Engineering Standards - ISA-88 Control Activity Model
 *===========================================================================*/

/**
 * @brief ISA-88 procedural control model hierarchy.
 *
 * Procedure        : Highest level (e.g., "Make Product X")
 *   Unit Procedure : Sequence of operations on a single unit
 *     Operation    : A major processing activity (e.g., "Charge")
 *       Phase      : Smallest element of procedural control
 */
typedef enum {
    ISA88_PROCEDURE        = 0,
    ISA88_UNIT_PROCEDURE   = 1,
    ISA88_OPERATION        = 2,
    ISA88_PHASE            = 3
} isa88_procedural_level_t;

/**
 * @brief ISA-88 phase state machine.
 */
typedef enum {
    ISA88_PHASE_IDLE        = 0,
    ISA88_PHASE_RUNNING     = 1,
    ISA88_PHASE_COMPLETE    = 2,
    ISA88_PHASE_HELD        = 3,
    ISA88_PHASE_STOPPED     = 4,
    ISA88_PHASE_ABORTED     = 5,
    ISA88_PHASE_RESTARTING  = 6
} isa88_phase_state_t;

/*===========================================================================
 * L4: Engineering Standards - IEC 61508 SIL Levels
 *===========================================================================*/

/**
 * @brief Safety Integrity Level per IEC 61508.
 *
 * SIL is defined by the average probability of failure on demand (PFDavg)
 * for low-demand mode, or probability of dangerous failure per hour (PFH)
 * for high-demand/continuous mode.
 *
 * SIL  Low-demand PFDavg      High-demand PFH
 * 1    >= 10^-2 to < 10^-1    >= 10^-6 to < 10^-5
 * 2    >= 10^-3 to < 10^-2    >= 10^-7 to < 10^-6
 * 3    >= 10^-4 to < 10^-3    >= 10^-8 to < 10^-7
 * 4    >= 10^-5 to < 10^-4    >= 10^-9 to < 10^-8
 */
typedef enum {
    DCS_SIL_NONE = 0,
    DCS_SIL_1    = 1,
    DCS_SIL_2    = 2,
    DCS_SIL_3    = 3,
    DCS_SIL_4    = 4
} dcs_sil_level_t;

/**
 * @brief Safety Instrumented Function (SIF) definition.
 *
 * A SIF is a safety function implemented by a Safety Instrumented System (SIS)
 * to achieve or maintain a safe state of the process with respect to a
 * specific hazardous event.
 */
typedef struct {
    uint32_t         sif_id;
    char             sif_name[64];
    char             hazard_description[128];
    dcs_sil_level_t  target_sil;
    dcs_sil_level_t  achieved_sil;
    double           required_rrf;    /* Required Risk Reduction Factor */
    double           achieved_rrf;    /* Achieved Risk Reduction Factor */
    double           pfd_avg;         /* Average Probability of Failure on Demand */
    double           proof_test_interval_hours;
    dcs_redundancy_arch_t sensor_arch;   /* Sensor voting architecture */
    dcs_redundancy_arch_t logic_arch;    /* Logic solver voting */
    dcs_redundancy_arch_t actuator_arch; /* Final element voting */
    int              is_verified;
} dcs_sif_definition_t;

#endif /* DCS_TYPES_H */
