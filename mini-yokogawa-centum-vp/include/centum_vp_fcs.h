#ifndef CENTUM_VP_FCS_H
#define CENTUM_VP_FCS_H

#include <stdint.h>
#include <stdbool.h>
#include "centum_vp_system.h"

typedef enum {
    FCS_TYPE_KFCS2  = 0,
    FCS_TYPE_KFCS   = 1,
    FCS_TYPE_FFCS   = 2,
    FCS_TYPE_LFCS   = 3,
    FCS_TYPE_SFCS   = 4,
    FCS_TYPE_KFCS2S = 5
} centum_fcs_type_t;

typedef enum {
    FCS_CPU_SINGLE         = 0,
    FCS_CPU_DUAL_STANDBY   = 1,
    FCS_CPU_DUAL_ACTIVE    = 2
} centum_fcs_cpu_mode_t;

typedef enum {
    FCS_IOBUS_RIO      = 0,
    FCS_IOBUS_FIO      = 1,
    FCS_IOBUS_NIO      = 2,
    FCS_IOBUS_FF_H1    = 3,
    FCS_IOBUS_PROFIBUS = 4
} centum_fcs_iobus_type_t;

typedef enum {
    IO_MOD_AAI141  = 0,
    IO_MOD_AAI143  = 1,
    IO_MOD_AAI841  = 2,
    IO_MOD_AAI543  = 3,
    IO_MOD_AAI835  = 4,
    IO_MOD_ADV151  = 5,
    IO_MOD_ADV551  = 6,
    IO_MOD_ADV859  = 7,
    IO_MOD_ADR541  = 8,
    IO_MOD_APC846  = 9,
    IO_MOD_AAR181  = 10,
    IO_MOD_AAR145  = 11,
    IO_MOD_AMC80   = 12,
    IO_MOD_ALR111  = 13,
    IO_MOD_ALF111  = 14,
    IO_MOD_ALP121  = 15
} centum_io_module_type_t;

typedef enum {
    SIG_TYPE_AI_4_20MA  = 0,
    SIG_TYPE_AI_1_5V    = 1,
    SIG_TYPE_AI_TC      = 2,
    SIG_TYPE_AI_RTD     = 3,
    SIG_TYPE_AI_PULSE   = 4,
    SIG_TYPE_AO_4_20MA  = 5,
    SIG_TYPE_DI_24V     = 6,
    SIG_TYPE_DI_120VAC  = 7,
    SIG_TYPE_DI_NAMUR   = 8,
    SIG_TYPE_DO_24V     = 9,
    SIG_TYPE_DO_RELAY   = 10,
    SIG_TYPE_DO_120VAC  = 11
} centum_signal_type_t;

typedef struct {
    double      eu_low;
    double      eu_high;
    double      raw_low;
    double      raw_high;
    int16_t     adc_low;
    int16_t     adc_high;
    char        eu_unit[8];
    uint8_t     filter_time_ms;
} centum_signal_range_t;

typedef struct {
    uint8_t     node_address;
    uint8_t     slot_count;
    bool        dual_power;
    bool        dual_bus_if;
    uint16_t    io_point_count;
    bool        live_list[32];
} centum_nio_node_t;

typedef struct {
    centum_io_module_type_t  type;
    uint8_t     slot_number;
    uint8_t     channel_count;
    uint32_t    serial_number;
    bool        healthy;
    bool        channel_fault[32];
    uint16_t    raw_values[32];
    double      eu_values[32];
    centum_signal_range_t ch_range[32];
} centum_io_module_t;

typedef struct {
    uint16_t        fcs_id;
    centum_fcs_type_t type;
    centum_fcs_cpu_mode_t cpu_mode;
    uint32_t        scan_cycle_us;
    uint32_t        io_scan_cycle_us;
    uint16_t        function_block_count;
    uint16_t        nio_node_count;
    centum_nio_node_t nio_nodes[16];
    uint16_t        io_module_count;
    centum_io_module_t io_modules[64];
    uint32_t        memory_pool_kb;
    double          cpu_load;
    double          memory_usage;
    bool            online;
    bool            redundancy_healthy;
    time_t          last_download_time;
} centum_fcs_config_t;

typedef enum {
    FB_PID       = 0,
    FB_IND       = 1,
    FB_MLD       = 2,
    FB_RATIO     = 3,
    FB_SEL       = 4,
    FB_FOUT      = 5,
    FB_VELLIM    = 6,
    FB_SPLRG     = 7,
    FB_PI_BLEND  = 8,
    FB_CALCU     = 9,
    FB_LC64      = 10,
    FB_SEBOL     = 11,
    FB_ST16      = 12,
    FB_SIO       = 13,
    FB_FF_AI     = 14,
    FB_FF_PID    = 15,
    FB_PB        = 16
} centum_fb_type_t;

typedef enum {
    FB_EXEC_ORDER_NORMAL = 0,
    FB_EXEC_ORDER_HIGH   = 1,
    FB_EXEC_ORDER_MEDIUM = 2,
    FB_EXEC_ORDER_LOW    = 3
} centum_fb_exec_order_t;

typedef struct {
    uint32_t    total_fb_count;
    uint32_t    fb_counts_by_type[20];
    uint32_t    executions_per_second;
    double      avg_exec_time_us;
    double      max_exec_time_us;
    double      min_exec_time_us;
    uint32_t    overrun_count;
    uint32_t    scan_cycle_count;
} centum_fb_exec_stats_t;

void centum_fcs_config_init(centum_fcs_config_t *fcs, uint16_t fcs_id);
bool centum_fcs_add_nio_node(centum_fcs_config_t *fcs, const centum_nio_node_t *node);
bool centum_fcs_add_io_module(centum_fcs_config_t *fcs, const centum_io_module_t *module);
bool centum_fcs_remove_io_module(centum_fcs_config_t *fcs, uint8_t node_addr, uint8_t slot);
double centum_signal_convert_raw_to_eu(int16_t raw_value, const centum_signal_range_t *range);
int16_t centum_signal_convert_eu_to_raw(double eu_value, const centum_signal_range_t *range);
uint16_t centum_fcs_total_io_points(const centum_fcs_config_t *fcs);
bool centum_fcs_validate_configuration(const centum_fcs_config_t *fcs);
const char *centum_fcs_type_to_string(centum_fcs_type_t type);
const char *centum_io_module_type_to_string(centum_io_module_type_t type);
centum_io_module_type_t centum_io_module_from_string(const char *model_str);
void centum_fcs_compute_exec_stats(centum_fcs_config_t *fcs, centum_fb_exec_stats_t *stats);

#endif