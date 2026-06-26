/**
 * @file experion_system.h
 * @brief Honeywell Experion PKS — Core System Architecture Definitions
 *
 * Covers L1 (Definitions): Experion PKS node types, domain topology, server
 * roles, station classifications, system-wide time synchronization.
 *
 * Reference: Honeywell Experion PKS System Architecture Specification (EP-DCS-ARCH)
 * Course Alignment: RWTH Aachen Industrial Control Systems, CMU 24-677 Adv Ctrl Systems
 * Standards: ISA-95 Level 3/4 integration, ISA-88 batch hierarchy
 *
 * All definitions follow the Experion PKS R500+ naming conventions.
 */

#ifndef EXPERION_SYSTEM_H
#define EXPERION_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1 — Core Type Definitions
 * ========================================================================== */

/** Experion PKS node types (topology enumeration).
 *  Each node represents a distinct functional role in the DCS architecture. */
typedef enum {
    EXN_NODE_UNKNOWN         = 0x00,
    EXN_NODE_ESVT            = 0x01,
    EXN_NODE_ESVT_REDUNDANT  = 0x02,
    EXN_NODE_EST             = 0x04,
    EXN_NODE_FLEX_STATION    = 0x08,
    EXN_NODE_C300            = 0x10,
    EXN_NODE_C300_REDUNDANT  = 0x20,
    EXN_NODE_UCN_GATEWAY     = 0x40,
    EXN_NODE_OPTIC_MUX       = 0x80,
    EXN_NODE_SAFETY_MGR      = 0x100,
    EXN_NODE_ACE             = 0x200,
    EXN_NODE_RTUS            = 0x400,
    EXN_NODE_DSA             = 0x800
} ExperionNodeType;

#define EXN_MAX_DOMAIN_NODES    256
#define EXN_MAX_ASSET_PER_DOMAIN 40000
#define EXN_MAX_ALARM_PER_DOMAIN 100000
#define EXN_FTE_NETWORK_COUNT   4
#define EXN_TAG_NAME_MAX_LEN    24
#define EXN_DESCRIPTOR_MAX_LEN  40
#define EXN_ENG_UNITS_MAX_LEN   10

typedef struct {
    uint32_t    domain_id;
    char        domain_name[64];
    uint32_t    primary_esvt_id;
    uint32_t    backup_esvt_id;
    uint32_t    nodes_online;
    uint32_t    nodes_total;
    uint64_t    asset_count;
    time_t      domain_start_time;
    bool        fte_enabled;
    uint8_t     fte_slot_a;
    uint8_t     fte_slot_b;
} ExperionDomain;

typedef enum {
    EDB_UNKNOWN        = 0,
    EDB_ERDB           = 1,
    EDB_EMDB           = 2,
    EDB_EHDB           = 3,
    EDB_EADB           = 4,
    EDB_ESDB           = 5,
    EDB_EEVDB          = 6
} ExperionDatabaseType;

typedef struct {
    uint32_t    max_analog_points;
    uint32_t    max_digital_points;
    uint32_t    max_accum_points;
    uint32_t    max_regulatory_cv;
    uint32_t    max_sequence_modules;
    uint32_t    max_device_ctrl;
    uint32_t    max_logic_blocks;
    uint32_t    max_custom_blocks;
} ExperionServerCapacity;

typedef enum {
    XMODE_INITIALIZING     = 0,
    XMODE_RUN              = 1,
    XMODE_HOLD             = 2,
    XMODE_SHUTDOWN         = 3,
    XMODE_FAILOVER         = 4,
    XMODE_EMERGENCY_STOP   = 5,
    XMODE_MAINTENANCE      = 6,
    XMODE_SIMULATION       = 7
} ExperionSystemMode;

typedef enum {
    XQUAL_GOOD              = 0x00,
    XQUAL_GOOD_CASCADE      = 0x01,
    XQUAL_UNCERTAIN         = 0x40,
    XQUAL_UNCERTAIN_SUBST   = 0x44,
    XQUAL_BAD               = 0xC0,
    XQUAL_BAD_CONFIG        = 0xC4,
    XQUAL_BAD_COMM          = 0xC8
} ExperionPointQuality;

typedef struct {
    uint8_t     priority1;
    uint8_t     clock_class;
    uint8_t     clock_accuracy;
    uint16_t    offset_scaled_log_var;
    uint8_t     priority2;
    uint64_t    clock_identity;
    uint16_t    steps_removed;
    uint32_t    announce_receipt_timeout;
} ExperionPTPClockQuality;

typedef struct {
    bool        sntp_enabled;
    bool        ptp_enabled;
    bool        gps_reference;
    char        ntp_server[128];
    uint32_t    ntp_poll_interval_s;
    uint32_t    ptp_sync_interval_s;
    uint32_t    max_clock_drift_us;
    ExperionPTPClockQuality best_master;
} ExperionTimeSyncConfig;

typedef struct {
    uint32_t                system_id;
    char                    system_name[64];
    ExperionDomain          domain;
    ExperionSystemMode      mode;
    ExperionTimeSyncConfig  time_sync;
    ExperionServerCapacity  capacity;
    uint32_t                scan_period_ms;
    uint32_t                node_registry[EXN_MAX_DOMAIN_NODES];
    uint32_t                node_types[EXN_MAX_DOMAIN_NODES];
    uint32_t                active_alarm_count;
    uint64_t                total_scan_cycles;
    bool                    redundancy_active;
    bool                    safety_integrated;
    uint32_t                uptime_hours;
} ExperionSystem;

typedef struct {
    double      eu_0_percent;
    double      eu_100_percent;
    char        eu_label[EXN_ENG_UNITS_MAX_LEN];
    double      signal_lo;
    double      signal_hi;
    int32_t     decimal_places;
} ExperionEURange;

/* API */
int  experion_system_init(ExperionSystem *sys, const char *name, uint32_t id);
int  experion_system_register_node(ExperionSystem *sys, uint32_t node_id,
                                   ExperionNodeType node_type);
int  experion_system_activate(ExperionSystem *sys);
int  experion_system_set_mode(ExperionSystem *sys, ExperionSystemMode mode);
int  experion_system_get_point_count(const ExperionSystem *sys,
                                     ExperionServerCapacity *capacity);
bool experion_system_health_check(const ExperionSystem *sys);
int64_t experion_clock_offset_ns(int64_t t1, int64_t t2, int64_t t3, int64_t t4);
int  experion_system_shutdown(ExperionSystem *sys);

#ifdef __cplusplus
}
#endif

#endif /* EXPERION_SYSTEM_H */
