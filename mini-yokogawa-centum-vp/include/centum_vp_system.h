#ifndef CENTUM_VP_SYSTEM_H
#define CENTUM_VP_SYSTEM_H
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define CENTUM_VP_MAX_STATIONS       256U
#define CENTUM_VP_MAX_DOMAINS         16U
#define CENTUM_VP_MAX_HIS             64U
#define CENTUM_VP_MAX_FCS             64U
#define CENTUM_VP_MAX_FB_PER_FCS    20000U
#define CENTUM_VP_MAX_IO_PER_FCS   100000U
#define CENTUM_VP_MAX_TAGS         500000U
#define CENTUM_VP_VNET_BANDWIDTH_GBPS  1.0
#define CENTUM_VP_SCAN_CYCLE_FAST      50U
#define CENTUM_VP_SCAN_CYCLE_MEDIUM   100U
#define CENTUM_VP_SCAN_CYCLE_NORMAL   200U
#define CENTUM_VP_SCAN_CYCLE_SLOW      500U
#define CENTUM_VP_SCAN_CYCLE_VERY_SLOW 1000U
#define CENTUM_VP_SCAN_CYCLE_BATCH    2000U

typedef enum {
    CENTUM_STATION_HIS     = 0,
    CENTUM_STATION_FCS     = 1,
    CENTUM_STATION_ENG     = 2,
    CENTUM_STATION_SENG    = 3,
    CENTUM_STATION_BCV     = 4,
    CENTUM_STATION_CGW     = 5,
    CENTUM_STATION_SFC     = 6,
    CENTUM_STATION_APCS    = 7,
    CENTUM_STATION_PRINTER = 8,
    CENTUM_STATION_LHS     = 9,
    CENTUM_STATION_EXAOPC  = 10
} centum_station_type_t;

typedef enum {
    CENTUM_STAT_POWEROFF   = 0,
    CENTUM_STAT_INITIAL    = 1,
    CENTUM_STAT_STANDBY    = 2,
    CENTUM_STAT_LOADING    = 3,
    CENTUM_STAT_RUNNING    = 4,
    CENTUM_STAT_FAIL       = 5,
    CENTUM_STAT_MAINT      = 6,
    CENTUM_STAT_SIMULATE   = 7
} centum_station_status_t;

typedef enum {
    CENTUM_MODE_OFFLINE    = 0,
    CENTUM_MODE_ONLINE     = 1,
    CENTUM_MODE_DEBUG      = 2,
    CENTUM_MODE_TEST       = 3,
    CENTUM_MODE_EMERGENCY  = 4
} centum_system_mode_t;

typedef struct {
    uint8_t     domain_id;
    char        domain_name[32];
    uint16_t    station_count;
    uint16_t    station_ids[CENTUM_VP_MAX_STATIONS];
    uint32_t    domain_ip_subnet;
    uint32_t    domain_ip_mask;
    bool        inter_domain_link;
    time_t      creation_time;
} centum_domain_config_t;

typedef struct {
    char            project_name[64];
    char            project_description[128];
    uint8_t         project_version_major;
    uint8_t         project_version_minor;
    uint8_t         domain_count;
    centum_domain_config_t domains[CENTUM_VP_MAX_DOMAINS];
    centum_system_mode_t   system_mode;
    bool            redundancy_enabled;
    bool            safety_system_integrated;
    uint32_t        total_tag_count;
    time_t          last_engineering_build;
    time_t          last_online_download;
} centum_system_config_t;

typedef struct {
    uint8_t     domain_number;
    uint8_t     station_number;
    uint32_t    ip_address;
    uint16_t    vnet_port;
    char        hostname[64];
} centum_vnet_addr_t;

typedef struct {
    uint16_t                station_id;
    centum_station_type_t   type;
    centum_station_status_t status;
    centum_vnet_addr_t      vnet_addr;
    char                    station_name[32];
    char                    station_comment[64];
    uint32_t                memory_mb;
    double                  cpu_load_percent;
    uint32_t                uptime_seconds;
    uint32_t                error_count;
    time_t                  last_status_change;
    uint16_t                engineering_unit_id;
    bool                    license_valid;
    time_t                  license_expiry;
} centum_station_t;

typedef enum {
    DBSEG_COMMON       = 0,
    DBSEG_FCS_CONFIG   = 1,
    DBSEG_IO_MODULE    = 2,
    DBSEG_FUNCTION_BLK = 3,
    DBSEG_SEQUENCE     = 4,
    DBSEG_GRAPHIC      = 5,
    DBSEG_ALARM        = 6,
    DBSEG_TREND        = 7,
    DBSEG_REPORT       = 8,
    DBSEG_SECURITY     = 9,
    DBSEG_OPC_MAP      = 10,
    DBSEG_BATCH        = 11,
    DBSEG_SAFETY       = 12
} centum_db_segment_type_t;

typedef enum {
    DBCHECK_OK              = 0,
    DBCHECK_WARNING         = 1,
    DBCHECK_ERROR_TAG_DUP   = 2,
    DBCHECK_ERROR_WIRE_OPEN = 3,
    DBCHECK_ERROR_MISSING   = 4,
    DBCHECK_ERROR_TYPE_MIS  = 5,
    DBCHECK_ERROR_LOOP      = 6
} centum_db_check_result_t;

typedef enum {
    LICENSE_HIS_BASE      = 0x0001,
    LICENSE_HIS_8MONITOR  = 0x0002,
    LICENSE_HIS_OPC       = 0x0004,
    LICENSE_FCS_1000PT    = 0x0010,
    LICENSE_FCS_5000PT    = 0x0011,
    LICENSE_FCS_10000PT   = 0x0012,
    LICENSE_FCS_20000PT   = 0x0013,
    LICENSE_BATCH_MGMT    = 0x0020,
    LICENSE_APCS_MPC      = 0x0030,
    LICENSE_SAFETY_SIL3   = 0x0040,
    LICENSE_OPC_UA        = 0x0050,
    LICENSE_EXAQUANTUM    = 0x0060,
    LICENSE_REDUNDANCY    = 0x0070,
    LICENSE_REMOTE_ENG    = 0x0080
} centum_license_feature_t;

typedef struct {
    char        license_key[64];
    uint32_t    feature_mask;
    uint32_t    max_tags;
    uint8_t     licensed_fcs_count;
    uint8_t     licensed_his_count;
    time_t      issue_date;
    time_t      expiry_date;
    char        licensed_to[128];
    bool        valid;
} centum_license_t;

void centum_system_config_init(centum_system_config_t *config);
bool centum_system_add_station(centum_system_config_t *config, const centum_station_t *station, uint8_t domain_id);
bool centum_system_remove_station(centum_system_config_t *config, uint16_t station_id);
bool centum_system_set_station_status(centum_system_config_t *config, uint16_t station_id, centum_station_status_t new_status);
uint32_t centum_system_total_io_count(const centum_system_config_t *config);
uint16_t centum_system_find_station_by_tag(const centum_system_config_t *config, const char *station_tag);
centum_db_check_result_t centum_system_validate(const centum_system_config_t *config);
bool centum_system_license_check(const centum_system_config_t *config, const centum_license_t *license);
uint32_t centum_vnet_calc_ip_address(uint8_t domain, uint8_t station);
const char *centum_station_type_to_string(centum_station_type_t type);
const char *centum_station_status_to_string(centum_station_status_t status);

#endif
