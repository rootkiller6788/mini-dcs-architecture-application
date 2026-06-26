#ifndef DELTA_V_SYSTEM_H
#define DELTA_V_SYSTEM_H
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define DELTAV_MAX_WORKSTATIONS         60U
#define DELTAV_MAX_CONTROLLERS         120U
#define DELTAV_MAX_CHARMS_PER_CONT    96U
#define DELTAV_MAX_IO_PER_CONT        256U
#define DELTAV_MAX_MODULES_PER_CTRL   30000U
#define DELTAV_MAX_TAGS               750000U
#define DELTAV_MAX_AREAS              24U
#define DELTAV_MAX_UNITS_PER_AREA     50U
#define DELTAV_MAX_ALARMS_PER_SEC      50U
#define DELTAV_MAX_TREND_SAMPLES     100000U
#define DELTAV_REDUN_NET_BANDWIDTH_GBPS  1.0
#define DELTAV_PRIMARY_NET_PORT       18500U
#define DELTAV_SECONDARY_NET_PORT     18501U

#define DELTAV_SCAN_FAST_25MS           25U
#define DELTAV_SCAN_FAST_50MS           50U
#define DELTAV_SCAN_NORMAL_100MS       100U
#define DELTAV_SCAN_NORMAL_250MS       250U
#define DELTAV_SCAN_SLOW_500MS         500U
#define DELTAV_SCAN_SLOW_1S           1000U
#define DELTAV_SCAN_SLOW_2S           2000U
#define DELTAV_SCAN_BATCH_5S          5000U

typedef enum {
    DELTAV_NODE_PROFESSIONAL_PLUS = 0,
    DELTAV_NODE_PROFESSIONAL      = 1,
    DELTAV_NODE_OPERATOR          = 2,
    DELTAV_NODE_APPLICATION       = 3,
    DELTAV_NODE_BASE              = 4,
    DELTAV_NODE_REMOTE            = 5,
    DELTAV_NODE_CONTROLLER        = 6,
    DELTAV_NODE_SIS_CONTROLLER    = 7,
    DELTAV_NODE_CHARMS_GATEWAY    = 8
} delta_v_node_type_t;

typedef enum {
    DELTAV_STAT_OFF              = 0,
    DELTAV_STAT_BOOTING          = 1,
    DELTAV_STAT_INITIALIZING     = 2,
    DELTAV_STAT_STANDBY          = 3,
    DELTAV_STAT_ACTIVE           = 4,
    DELTAV_STAT_DEGRADED         = 5,
    DELTAV_STAT_FAILED           = 6,
    DELTAV_STAT_SIMULATE         = 7
} delta_v_node_status_t;

typedef enum {
    DELTAV_MODE_SETUP             = 0,
    DELTAV_MODE_DOWNLOAD_PARTIAL  = 1,
    DELTAV_MODE_DOWNLOAD_FULL     = 2,
    DELTAV_MODE_ONLINE            = 3,
    DELTAV_MODE_EMERGENCY         = 4
} delta_v_system_mode_t;

typedef struct {
    uint8_t     area_id;
    char        area_name[32];
    char        area_description[128];
    uint16_t    module_count;
    uint16_t    alarm_priority_default;
    bool        operator_access_full;
    bool        maintenance_access;
    bool        batch_active;
    uint16_t    master_workstation_id;
    time_t      area_created;
    double      alarm_hysteresis_percent;
} delta_v_area_t;
typedef enum {
    DELTAV_DBSEG_SYSTEM       = 0,
    DELTAV_DBSEG_CONTROLLERS  = 1,
    DELTAV_DBSEG_CHARMS_IO    = 2,
    DELTAV_DBSEG_CONTROL_MOD  = 3,
    DELTAV_DBSEG_ALARM        = 4,
    DELTAV_DBSEG_HISTORY      = 5,
    DELTAV_DBSEG_SECURITY     = 6,
    DELTAV_DBSEG_BATCH        = 7,
    DELTAV_DBSEG_GRAPHICS     = 8,
    DELTAV_DBSEG_EVENT        = 9,
    DELTAV_DBSEG_SIS          = 10,
    DELTAV_DBSEG_OPC          = 11,
    DELTAV_DBSEG_AMS          = 12
} delta_v_db_segment_t;

typedef enum {
    DELTAV_DB_OK                 = 0,
    DELTAV_DB_WARNING            = 1,
    DELTAV_DB_ERROR_TAG_DUP      = 2,
    DELTAV_DB_ERROR_WIRE_OPEN    = 3,
    DELTAV_DB_ERROR_MISSING_REF  = 4,
    DELTAV_DB_ERROR_TYPE_MISMATCH = 5,
    DELTAV_DB_ERROR_LOOP_DETECTED = 6,
    DELTAV_DB_ERROR_SIS_VIOLATION = 7,
    DELTAV_DB_ERROR_LICENSE       = 8
} delta_v_db_check_t;

typedef struct {
    uint32_t    subnet_address;
    uint32_t    subnet_mask;
    uint8_t     node_prefix;
    bool        redundancy_enabled;
    uint16_t    primary_port;
    uint16_t    secondary_port;
    uint32_t    bandwidth_bps;
    double      latency_max_ms;
    double      jitter_max_us;
    bool        spanning_tree_enabled;
    bool        igmp_snooping;
} delta_v_network_topology_t;

typedef enum {
    DELTAV_LIC_PROPLUS_BASE       = 0x0001,
    DELTAV_LIC_WORKSTATION        = 0x0002,
    DELTAV_LIC_CONTROLLER_250Dst  = 0x0010,
    DELTAV_LIC_CONTROLLER_500Dst  = 0x0011,
    DELTAV_LIC_CONTROLLER_1000Dst = 0x0012,
    DELTAV_LIC_CONTROLLER_2000Dst = 0x0013,
    DELTAV_LIC_BATCH_ENG          = 0x0020,
    DELTAV_LIC_BATCH_EXEC         = 0x0021,
    DELTAV_LIC_MPC_PREDICT        = 0x0030,
    DELTAV_LIC_MPC_PREDICTPRO     = 0x0031,
    DELTAV_LIC_SIS_SIL2           = 0x0040,
    DELTAV_LIC_SIS_SIL3           = 0x0041,
    DELTAV_LIC_AMS_DEVICEMGR      = 0x0050,
    DELTAV_LIC_REDUNDANCY         = 0x0060,
    DELTAV_LIC_OPC_SERVER         = 0x0070,
    DELTAV_LIC_CHARMS             = 0x0080,
    DELTAV_LIC_DELTAV_LIVE        = 0x0090,
    DELTAV_LIC_NEURAL             = 0x00A0
} delta_v_license_feature_t;

typedef struct {
    char        license_key[64];
    uint64_t    feature_mask;
    uint32_t    max_dsts;
    uint32_t    max_controllers;
    uint16_t    max_workstations;
    uint16_t    max_batch_units;
    time_t      issued_date;
    time_t      expiry_date;
    char        licensed_to[128];
    bool        valid;
    uint32_t    dongle_serial;
} delta_v_license_t;

typedef struct {
    char            system_name[64];
    char            system_description[256];
    uint8_t         major_version;
    uint8_t         minor_version;
    uint8_t         service_pack;
    delta_v_system_mode_t system_mode;
    uint8_t         area_count;
    delta_v_area_t  areas[DELTAV_MAX_AREAS];
    uint16_t        workstation_count;
    uint16_t        controller_count;
    delta_v_network_topology_t network;
    delta_v_db_check_t   last_db_check;
    uint32_t        total_tag_count;
    uint32_t        total_alarm_count;
    bool            redundancy_enabled;
    bool            sis_integrated;
    bool            batch_installed;
    bool            ams_installed;
    bool            opc_ua_enabled;
    time_t          last_full_download;
    time_t          last_engineering_build;
    char            proplus_hostname[64];
} delta_v_system_config_t;

typedef struct {
    uint16_t                node_id;
    delta_v_node_type_t     type;
    delta_v_node_status_t   status;
    char                    hostname[64];
    char                    description[128];
    uint32_t                ip_address;
    uint32_t                ip_address_sec;
    uint16_t                control_port;
    uint32_t                memory_mb;
    double                  cpu_load_percent;
    uint32_t                uptime_seconds;
    uint32_t                error_count;
    uint32_t                comm_error_count;
    time_t                  last_status_change;
    uint8_t                 area_id;
    bool                    license_valid;
    time_t                  license_expiry;
    uint8_t                 firmware_version[4];
    bool                    is_redundant_pair;
    uint16_t                partner_node_id;
} delta_v_node_t;

void delta_v_system_init(delta_v_system_config_t *config);
bool delta_v_system_add_area(delta_v_system_config_t *config, const delta_v_area_t *area);
bool delta_v_system_remove_area(delta_v_system_config_t *config, uint8_t area_id);
const delta_v_area_t *delta_v_system_find_area(const delta_v_system_config_t *config, uint8_t area_id);
bool delta_v_system_register_node(delta_v_system_config_t *config, const delta_v_node_t *node);
bool delta_v_system_decommission_node(delta_v_system_config_t *config, uint16_t node_id);
bool delta_v_system_set_node_status(delta_v_system_config_t *config, uint16_t node_id, delta_v_node_status_t new_status);
delta_v_db_check_t delta_v_system_validate(const delta_v_system_config_t *config);
bool delta_v_system_license_check(const delta_v_system_config_t *config, const delta_v_license_t *license);
uint32_t delta_v_calc_primary_ip(const delta_v_network_topology_t *net, uint8_t node_index);
uint32_t delta_v_calc_secondary_ip(const delta_v_network_topology_t *net, uint8_t node_index);
bool delta_v_is_valid_status_transition(delta_v_node_status_t current, delta_v_node_status_t next);
uint32_t delta_v_system_total_dst_count(const delta_v_system_config_t *config);
uint16_t delta_v_system_active_controller_count(const delta_v_system_config_t *config);
bool delta_v_system_redundancy_health_check(const delta_v_system_config_t *config);
const char *delta_v_node_type_to_string(delta_v_node_type_t type);
const char *delta_v_node_status_to_string(delta_v_node_status_t status);
const char *delta_v_system_mode_to_string(delta_v_system_mode_t mode);
const char *delta_v_db_check_to_string(delta_v_db_check_t check);

#endif
