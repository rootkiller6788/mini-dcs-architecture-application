/**
 * @file    ecs700_system_core.h
 * @brief   SUPCON ECS-700 DCS System Core Architecture Definitions
 *
 * ECS-700 (Engineering Control System-700) is SUPCON's large-scale
 * distributed control system platform for process industries.
 *
 * Architecture Overview:
 *   Management Network (MNet)   — ERP/MES integration layer
 *   System Network (SCnet)      — redundant industrial Ethernet backbone
 *   Process Control Network (SBUS) — field-level redundant bus
 *   Control Station (CS)        — main controller executing control logic
 *   Engineering Station (ES)    — system configuration and programming
 *   Operator Station (OS)       — process visualization and operation HMI
 *   Historian Station (HS)      — long-term data archiving and analysis
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L3 Engineering Structures
 *
 * Reference:
 *   - IEC 61131-3: Programming languages for industrial control
 *   - IEC 61508: Functional safety of E/E/PE safety-related systems
 *   - GB/T 36293-2018: DCS technical specifications (China National Standard)
 *   - Astrom & Hagglund (1995), PID Controllers: Theory, Design, and Tuning
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#ifndef ECS700_SYSTEM_CORE_H
#define ECS700_SYSTEM_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* ============================================================================
 * L1: Core Definitions — System Constants and Enumerations
 * ============================================================================
 */

/** Maximum number of control stations in a single ECS-700 domain */
#define ECS700_MAX_CONTROL_STATIONS     64

/** Maximum number of operator stations per system */
#define ECS700_MAX_OPERATOR_STATIONS    32

/** Maximum number of engineering stations */
#define ECS700_MAX_ENGINEERING_STATIONS 8

/** Maximum number of I/O modules per control station */
#define ECS700_MAX_IO_MODULES_PER_CS    128

/** Maximum points per I/O module (mixed analog/digital) */
#define ECS700_MAX_POINTS_PER_MODULE    64

/** Maximum system domains (logical partitions) */
#define ECS700_MAX_DOMAINS              16

/** SCnet redundant network ports */
#define ECS700_SCNET_PORT_A             0
#define ECS700_SCNET_PORT_B             1

/** SBUS redundant fieldbus ports */
#define ECS700_SBUS_PORT_A              0
#define ECS700_SBUS_PORT_B              1

/** SCnet standard cycle time in microseconds */
#define ECS700_SCNET_CYCLE_US           100000UL  /* 100 ms */

/** Control station basic scan period (fast loop) in microseconds */
#define ECS700_CS_FAST_SCAN_US          50000UL   /* 50 ms */

/** Control station standard scan period in microseconds */
#define ECS700_CS_STANDARD_SCAN_US      200000UL  /* 200 ms */

/** Control station slow scan period in microseconds */
#define ECS700_CS_SLOW_SCAN_US          1000000UL /* 1 s */

/** Redundancy failover target time in microseconds (IEC 62439-3 HSR) */
#define ECS700_FAILOVER_TARGET_US       20000UL   /* 20 ms */

/** Maximum process points tag length */
#define ECS700_TAG_LEN_MAX              32

/** Maximum alarm message length */
#define ECS700_ALARM_MSG_LEN_MAX        256

/**
 * @brief DCS node types in ECS-700 system architecture
 *
 * Each node type has a distinct role in the distributed architecture.
 * The system assigns a unique node ID to every device on the SCnet.
 */
typedef enum {
    ECS700_NODE_CONTROL_STATION   = 0x01,  /**< Main process controller */
    ECS700_NODE_OPERATOR_STATION  = 0x02,  /**< Human-machine interface */
    ECS700_NODE_ENGINEERING_STATION = 0x04, /**< Engineering/config station */
    ECS700_NODE_HISTORIAN_STATION = 0x08,  /**< Data historian/archive */
    ECS700_NODE_GATEWAY           = 0x10,  /**< Communication gateway */
    ECS700_NODE_TIME_SERVER       = 0x20,  /**< GPS/NTP time synchronization */
    ECS700_NODE_SAFETY_CONTROLLER = 0x40,  /**< SIS safety controller */
    ECS700_NODE_IOSERVER          = 0x80   /**< I/O communication server */
} ecs700_node_type_t;

/**
 * @brief Control station operational states
 *
 * Models the lifecycle of a control station from power-on through
 * active operation, including redundancy transitions.
 */
typedef enum {
    ECS700_CS_STATE_OFFLINE       = 0,  /**< Powered off or disconnected */
    ECS700_CS_STATE_INITIALIZING  = 1,  /**< Booting, self-test in progress */
    ECS700_CS_STATE_LOADING       = 2,  /**< Loading configuration from ES */
    ECS700_CS_STATE_STANDBY       = 3,  /**< Ready, waiting for activation */
    ECS700_CS_STATE_PRIMARY       = 4,  /**< Active, executing control logic */
    ECS700_CS_STATE_SECONDARY     = 5,  /**< Hot standby, sync in progress */
    ECS700_CS_STATE_MAINTENANCE   = 6,  /**< Maintenance mode, outputs held */
    ECS700_CS_STATE_FAULT         = 7   /**< Fault detected, fail-safe state */
} ecs700_cs_state_t;

/**
 * @brief Redundancy modes supported by ECS-700
 *
 * ECS-700 supports multiple redundancy strategies:
 *   1:1 Hot Standby — primary/secondary pair with real-time sync
 *   N:1 Cold Standby — one backup for N controllers (cost-optimized)
 */
typedef enum {
    ECS700_REDUNDANCY_NONE        = 0,  /**< No redundancy (single controller) */
    ECS700_REDUNDANCY_1V1_HOT     = 1,  /**< 1:1 hot standby (default) */
    ECS700_REDUNDANCY_NV1_COLD    = 2,  /**< N:1 cold standby */
    ECS700_REDUNDANCY_2V2_PARALLEL = 3  /**< 2-out-of-2 parallel voting */
} ecs700_redundancy_mode_t;

/**
 * @brief I/O signal types processed by ECS-700
 */
typedef enum {
    ECS700_SIGNAL_AI_4_20MA       = 0,  /**< Analog Input, 4-20 mA */
    ECS700_SIGNAL_AI_0_10V        = 1,  /**< Analog Input, 0-10 V */
    ECS700_SIGNAL_AI_TC           = 2,  /**< Analog Input, Thermocouple */
    ECS700_SIGNAL_AI_RTD          = 3,  /**< Analog Input, RTD (Pt100/Cu50) */
    ECS700_SIGNAL_AO_4_20MA       = 4,  /**< Analog Output, 4-20 mA */
    ECS700_SIGNAL_AO_0_10V        = 5,  /**< Analog Output, 0-10 V */
    ECS700_SIGNAL_DI_24V          = 6,  /**< Digital Input, 24 VDC */
    ECS700_SIGNAL_DI_220V         = 7,  /**< Digital Input, 220 VAC */
    ECS700_SIGNAL_DO_RELAY        = 8,  /**< Digital Output, Relay */
    ECS700_SIGNAL_DO_SSR          = 9,  /**< Digital Output, Solid State */
    ECS700_SIGNAL_PI_PULSE        = 10, /**< Pulse Input for flow totalization */
    ECS700_SIGNAL_HART            = 11, /**< HART smart instrument signal */
    ECS700_SIGNAL_FOUNDATION_FF   = 12, /**< Foundation Fieldbus H1 */
    ECS700_SIGNAL_PROFIBUS_DP     = 13  /**< PROFIBUS DP device */
} ecs700_signal_type_t;

/* ============================================================================
 * L1: Core Data Structures — System Architecture Models
 * ============================================================================
 */

/**
 * @brief Engineering unit range for analog signals
 *
 * Defines the conversion between raw ADC/DAC counts (0-65535 for 16-bit)
 * and engineering units (e.g., 0-100% level, 0-250°C temperature).
 *
 * Knowledge Point: Sensor Range/Scaling — the mapping between physical
 * input and digital representation is fundamental to all DCS measurements.
 * Implements: y = eu_lo + (raw - raw_lo) * (eu_hi - eu_lo) / (raw_hi - raw_lo)
 */
typedef struct {
    double raw_lo;          /**< Raw minimum (e.g., 0 for 4 mA) */
    double raw_hi;          /**< Raw maximum (e.g., 65535 for 20 mA) */
    double eu_lo;           /**< Engineering unit minimum */
    double eu_hi;           /**< Engineering unit maximum */
    char   eu_label[16];    /**< Engineering unit label (e.g., "°C", "kPa") */
    uint8_t decimal_places; /**< Display precision for HMI */
} ecs700_eu_range_t;

/**
 * @brief Process point (tag) — the fundamental data entity in ECS-700
 *
 * Every measurement, setpoint, output, and calculated value in the DCS
 * is represented as a process point. The real-time database is a collection
 * of process points indexed by unique tag names.
 *
 * Knowledge Point: DCS Real-Time Database — the tag-centric data model
 * that underpins all control, display, alarm, and historian functions.
 * Conceptually equivalent to OPC UA Variable Node.
 */
typedef struct {
    char      tag[ECS700_TAG_LEN_MAX];   /**< Unique tag name */
    char      description[128];           /**< Human-readable description */
    ecs700_signal_type_t signal_type;     /**< I/O signal type */
    ecs700_eu_range_t eu_range;           /**< Engineering unit range */
    double    pv;                         /**< Process variable (current value) */
    double    pv_raw;                     /**< Raw ADC value (unscaled) */
    double    pv_filtered;               /**< Filtered process variable */
    double    pv_rate;                   /**< Rate of change (per second) */
    uint64_t  pv_timestamp;             /**< μs timestamp of last PV update */
    double    setpoint;                  /**< Target setpoint */
    double    output;                    /**< Controller output (OP) */
    double    output_raw;                /**< Raw DAC output value */
    uint8_t   alarm_state;               /**< Current alarm state bitmask */
    bool      scan_enabled;              /**< Whether point is in scan */
    bool      output_enabled;            /**< Whether output is active */
    uint16_t  control_station_id;        /**< Owning control station */
    uint16_t  io_module_id;             /**< I/O module index */
    uint8_t   io_channel;               /**< I/O channel within module */
    uint32_t  scan_period_us;           /**< Assigned scan period */
    uint64_t  last_scan_time;           /**< μs timestamp of last scan */
    double    signal_filter_tc;          /**< Filter time constant (seconds) */
} ecs700_process_point_t;

/**
 * @brief Domain configuration — logical partition within ECS-700 system
 *
 * A domain groups related control stations that share the same process area.
 * Domains enable: independent configuration management, separate alarm
 * suppression, and zone-based security isolation.
 *
 * Knowledge Point: DCS Domain/Zoning — logical partitioning for large plants
 * required by IEC 62443 zone-and-conduit security model.
 */
typedef struct {
    uint8_t   domain_id;                                /**< Domain identifier (1-16) */
    char      domain_name[64];                          /**< Domain name (e.g., "Reaction Area") */
    uint8_t   num_control_stations;                     /**< Control stations in domain */
    uint16_t  control_station_ids[ECS700_MAX_CONTROL_STATIONS]; /**< CS node IDs */
    uint8_t   num_io_modules;                           /**< Total I/O modules */
    uint32_t  total_process_points;                      /**< Total tag count */
    uint32_t  scan_period_fast_us;                       /**< Domain fast scan period */
    uint32_t  scan_period_normal_us;                     /**< Domain normal scan period */
    uint32_t  scan_period_slow_us;                       /**< Domain slow scan period */
    bool      domain_security_enabled;                   /**< IEC 62443 zone security */
    uint8_t   security_level;                            /**< Security level 0-4 */
} ecs700_domain_config_t;

/**
 * @brief System-level ECS-700 configuration
 *
 * Top-level structure representing the complete DCS system.
 * Contains all domains, network topology, and global parameters.
 *
 * Knowledge Point: DCS System Architecture — hierarchical decomposition
 * from system → domain → control station → I/O module → channel.
 */
typedef struct {
    char      system_name[64];                           /**< Plant/system identifier */
    uint8_t   num_domains;                               /**< Active domains */
    ecs700_domain_config_t domains[ECS700_MAX_DOMAINS];  /**< Domain configurations */
    uint16_t  total_control_stations;                    /**< Total CS count */
    uint16_t  total_operator_stations;                   /**< Total OS count */
    uint32_t  total_process_points;                       /**< Total tag count */
    uint32_t  global_scan_period_us;                      /**< Global scan period */
    uint64_t  system_start_time;                         /**< System start timestamp μs */
    uint64_t  current_scan_cycle;                        /**< Incremental scan cycle counter */
    bool      scnet_redundancy_enabled;                   /**< Redundant SCnet */
    bool      time_sync_enabled;                         /**< NTP/GPS time sync */
    uint32_t  time_sync_interval_s;                       /**< Time sync interval */
    uint8_t   failover_retry_max;                         /**< Max failover retries */
    uint32_t  heartbeat_interval_us;                     /**< Node heartbeat interval */
} ecs700_system_config_t;

/* ============================================================================
 * L2: Core Concepts — System-Level Operations
 * ============================================================================
 */

/**
 * @brief Initialize ECS-700 system configuration
 *
 * Sets up default values for a new system including scan periods,
 * network parameters, and time synchronization settings.
 *
 * Knowledge Point: DCS System Initialization — the cold-start sequence
 * that establishes the system infrastructure before any control runs.
 *
 * @param config    Pointer to system configuration to initialize
 * @param sys_name  Plant/system identifier string
 */
void ecs700_system_init(ecs700_system_config_t *config, const char *sys_name);

/**
 * @brief Register a new domain in the system
 *
 * Knowledge Point: Domain Registration — assigning control stations
 * to logical partitions for zone-based management.
 *
 * @param config      System configuration
 * @param domain_name Name of the new domain
 * @return Domain ID (1-based) on success, 0 on failure
 */
uint8_t ecs700_domain_register(ecs700_system_config_t *config,
                                const char *domain_name);

/**
 * @brief Add a control station to an existing domain
 *
 * Knowledge Point: Node Assignment — the hierarchical relationship
 * between domains and their control stations.
 *
 * @param config    System configuration
 * @param domain_id Target domain
 * @param cs_id     Control station node ID
 * @return true on success, false if domain full or invalid
 */
bool ecs700_domain_add_cs(ecs700_system_config_t *config,
                           uint8_t domain_id, uint16_t cs_id);

/**
 * @brief Get the total number of configured process points in a domain
 *
 * @param config    System configuration
 * @param domain_id Domain to query
 * @return Number of points, or 0 if domain invalid
 */
uint32_t ecs700_domain_point_count(const ecs700_system_config_t *config,
                                    uint8_t domain_id);

/**
 * @brief Compute the maximum allowable system load factor
 *
 * The load factor is the ratio of actual scan execution time to the
 * configured scan period. ECS-700 design guideline: load factor ≤ 60%
 * for stable operation with margin for communication overhead.
 *
 * Knowledge Point: DCS Load Factor — critical capacity planning metric.
 * L = (T_exec / T_scan) * 100%. Must stay below threshold to prevent
 * scan overruns that cause control degradation.
 *
 * @param total_exec_time_us  Total execution time per scan (μs)
 * @param scan_period_us      Configured scan period (μs)
 * @return Load factor as percentage (0.0 - 100.0)
 */
double ecs700_compute_load_factor(double total_exec_time_us,
                                   double scan_period_us);

/**
 * @brief Estimate system communication bandwidth utilization
 *
 * Computes the SCnet bandwidth usage based on point count, scan rate,
 * and packet overhead. ECS-700 SCnet uses 100 Mbps (Fast Ethernet) or
 * 1 Gbps (Gigabit Ethernet) redundant ring.
 *
 * Knowledge Point: DCS Network Sizing — the deterministic communication
 * model. B = N_points * bytes_per_point * scan_rate + overhead.
 * Must include protocol overhead (TCP/IP + SCnet application layer).
 *
 * @param num_points          Total process points on network
 * @param scan_period_us      Scan period in microseconds
 * @param bytes_per_point     Data size per point (typical 32 bytes)
 * @return Bandwidth usage in bits per second
 */
double ecs700_estimate_network_bandwidth(uint32_t num_points,
                                          uint32_t scan_period_us,
                                          uint32_t bytes_per_point);

/**
 * @brief Validate system configuration consistency
 *
 * Checks for: duplicate node IDs, domain capacity violations,
 * invalid scan period ranges, and redundancy mismatches.
 *
 * Knowledge Point: Configuration Validation — pre-startup sanity checks
 * required by IEC 61508 for safety-related systems (offline verification).
 *
 * @param config  System configuration to validate
 * @return 0 if valid, non-zero error code otherwise
 */
int ecs700_validate_config(const ecs700_system_config_t *config);

/* ============================================================================
 * L3: Engineering Structures — System Data Models
 * ============================================================================
 */

/**
 * @brief System health status structure
 *
 * Centralized diagnostic data for the entire DCS system.
 * Aggregated from all nodes via SCnet health telegrams.
 *
 * Knowledge Point: System Health Monitoring — required by IEC 61508
 * diagnostic coverage requirements (DC ≥ 90% for SIL 2).
 */
typedef struct {
    uint8_t   active_domains;                            /**< Domains online */
    uint16_t  primary_controllers;                       /**< Controllers in PRIMARY state */
    uint16_t  standby_controllers;                       /**< Controllers in STANDBY state */
    uint16_t  fault_controllers;                         /**< Controllers in FAULT state */
    uint16_t  total_alarms_active;                       /**< Current active alarms */
    uint16_t  total_alarms_unacked;                      /**< Unacknowledged alarms */
    double    average_cpu_load;                          /**< System average CPU % */
    double    scnet_a_utilization;                       /**< SCnet port A utilization % */
    double    scnet_b_utilization;                       /**< SCnet port B utilization % */
    uint32_t  total_packet_errors;                       /**< Cumulative network errors */
    uint32_t  failover_events;                           /**< Redundancy switchovers */
    uint64_t  uptime_seconds;                            /**< System uptime */
    bool      time_sync_valid;                           /**< Time synchronization status */
    double    average_scan_jitter_us;                     /**< Average scan time jitter */
} ecs700_system_health_t;

/**
 * @brief Collect system-wide health diagnostics
 *
 * Knowledge Point: Diagnostics Aggregation — the hierarchical
 * collection of health data from nodes → domains → system.
 *
 * @param config  System configuration
 * @param health  Output health report structure
 */
void ecs700_collect_health(const ecs700_system_config_t *config,
                            ecs700_system_health_t *health);

/* ============================================================================
 * L4: Engineering Laws — Conversion and Scale Functions
 * ============================================================================
 */

/**
 * @brief Convert raw ADC value to engineering units
 *
 * Implements linear interpolation:
 *   eu = eu_lo + (raw - raw_lo) * (eu_hi - eu_lo) / (raw_hi - raw_lo)
 *
 * Handles edge cases: raw value outside [raw_lo, raw_hi] is clamped.
 * Division by zero is prevented (raw_lo == raw_hi returns eu_lo).
 *
 * Knowledge Point: Signal Linearization — the first step in any
 * measurement chain. Real DCS also supports sqrt extraction (flow),
 * polynomial, and thermocouple lookup tables.
 *
 * @param raw_raw  Raw ADC count
 * @param range    Engineering unit range definition
 * @return Value in engineering units
 */
double ecs700_raw_to_eu(uint16_t raw_raw, const ecs700_eu_range_t *range);

/**
 * @brief Convert engineering units to raw DAC value
 *
 * Inverse of raw_to_eu:
 *   raw = raw_lo + (eu - eu_lo) * (raw_hi - raw_lo) / (eu_hi - eu_lo)
 *
 * @param eu     Value in engineering units
 * @param range  Engineering unit range definition
 * @return Raw DAC count (clamped to [raw_lo, raw_hi])
 */
uint16_t ecs700_eu_to_raw(double eu, const ecs700_eu_range_t *range);

/**
 * @brief Apply first-order low-pass filter to process variable
 *
 * Filter equation: y[k] = α * x[k] + (1 - α) * y[k-1]
 * where α = Ts / (Ts + Tc), Ts = sample time, Tc = filter time constant.
 *
 * Knowledge Point: Signal Filtering — DCS noise reduction using
 * exponential moving average. Critical for noisy flow measurements.
 *
 * @param pv_new          New raw process value
 * @param pv_filtered_prev Previous filtered value
 * @param sample_time_s    Sample time in seconds
 * @param filter_tc_s      Filter time constant in seconds
 * @return Filtered value
 */
double ecs700_apply_signal_filter(double pv_new, double pv_filtered_prev,
                                   double sample_time_s, double filter_tc_s);

/**
 * @brief Compute rate of change with deadband
 *
 * Rate = (PV[k] - PV[k-1]) / Δt, with a minimum deadband to suppress
 * noise-induced false rate alarms.
 *
 * Knowledge Point: Rate-of-Change Detection — essential for process
 * upset detection and predictive alarm generation.
 *
 * @param pv_current  Current process value
 * @param pv_previous Previous process value
 * @param delta_t_s   Time delta in seconds
 * @param deadband    Minimum change to consider (engineering units)
 * @return Rate of change per second, or 0.0 if below deadband
 */
double ecs700_compute_pv_rate(double pv_current, double pv_previous,
                               double delta_t_s, double deadband);

#ifdef __cplusplus
}
#endif

#endif /* ECS700_SYSTEM_CORE_H */
