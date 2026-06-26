#ifndef DELTA_V_CONTROLLER_H
#define DELTA_V_CONTROLLER_H
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * Emerson DeltaV Controller Hardware Definitions
 * Reference: DeltaV M-series and S-series Hardware Documentation
 *
 * Covers M-series (MD, MD Plus, MX) and S-series (SQ, SD, SZ) controllers,
 * CHARMs (CHARacterization Modules) electronic marshalling technology,
 * and the I/O subsystem architecture.
 *
 * Knowledge Points (each typedef/enum = one L1/L3 concept):
 *   delta_v_controller_type_t      -- 7 controller hardware models (L1)
 *   delta_v_charm_signal_type_t    -- 12 CHARM signal types (L1)
 *   delta_v_charm_terminal_t       -- CHARM terminal block config (L3)
 *   delta_v_charm_io_channel_t     -- Single CHARM I/O channel (L3)
 *   delta_v_carrier_t              -- 8-slot CHARM carrier (L3)
 *   delta_v_controller_config_t    -- Controller HW/SW configuration (L3)
 *   delta_v_signal_range_t         -- Signal range (4-20mA etc.) (L1)
 *   delta_v_io_bus_type_t          -- 6 I/O bus types (L1)
 *   delta_v_controller_mode_t      -- Controller operational mode (L1)
 * ============================================================================ */

typedef enum {
    DELTAV_CTRL_MD       = 0,
    DELTAV_CTRL_MD_PLUS  = 1,
    DELTAV_CTRL_MX       = 2,
    DELTAV_CTRL_SQ       = 3,
    DELTAV_CTRL_SD       = 4,
    DELTAV_CTRL_SZ       = 5,
    DELTAV_CTRL_PK       = 6
} delta_v_controller_type_t;

typedef enum {
    DELTAV_SIG_AI_4_20MA_HART = 0,
    DELTAV_SIG_AI_0_20MA      = 1,
    DELTAV_SIG_AI_1_5V        = 2,
    DELTAV_SIG_AI_0_10V       = 3,
    DELTAV_SIG_AI_TC_K        = 4,
    DELTAV_SIG_AI_TC_J        = 5,
    DELTAV_SIG_AI_RTD_PT100   = 6,
    DELTAV_SIG_AI_PULSE       = 7,
    DELTAV_SIG_AO_4_20MA_HART = 8,
    DELTAV_SIG_DI_24VDC       = 9,
    DELTAV_SIG_DI_120VAC      = 10,
    DELTAV_SIG_DO_24VDC       = 11,
    DELTAV_SIG_DO_RELAY       = 12
} delta_v_charm_signal_type_t;

typedef enum {
    DELTAV_IOBUS_LOCAL        = 0,
    DELTAV_IOBUS_CHARMS       = 1,
    DELTAV_IOBUS_PROFIBUS_DP  = 2,
    DELTAV_IOBUS_DEVICENET    = 3,
    DELTAV_IOBUS_AS_I         = 4,
    DELTAV_IOBUS_FOUNDATION_H1 = 5
} delta_v_io_bus_type_t;

typedef enum {
    DELTAV_CTRL_MODE_RUN      = 0,
    DELTAV_CTRL_MODE_PROGRAM  = 1,
    DELTAV_CTRL_MODE_FAILSAFE = 2,
    DELTAV_CTRL_MODE_MAINT    = 3,
    DELTAV_CTRL_MODE_SIMULATE = 4
} delta_v_controller_mode_t;

typedef struct {
    uint8_t                 channel_number;
    delta_v_charm_signal_type_t signal_type;
    char                    tag_name[32];
    char                    description[64];
    double                  raw_min;
    double                  raw_max;
    double                  eu_min;
    double                  eu_max;
    char                    engineering_units[16];
    double                  signal_damping_sec;
    bool                    hart_enabled;
    uint8_t                 hart_polling_addr;
    bool                    channel_active;
    bool                    fault_detected;
    bool                    open_circuit_alarm;
    bool                    short_circuit_alarm;
    uint16_t                update_rate_ms;
} delta_v_charm_io_channel_t;

typedef struct {
    char        charm_model[32];
    delta_v_charm_signal_type_t type;
    uint8_t     channel_count;
    bool        hart_capable;
    bool        redundancy_capable;
    double      accuracy_percent;
    double      response_time_ms;
    uint32_t    firmware_version;
} delta_v_charm_spec_t;

typedef struct {
    delta_v_charm_spec_t     charm_spec;
    delta_v_charm_io_channel_t channels[8];
    uint8_t     slot_position;
    bool        installed;
    bool        healthy;
    bool        communicating;
    uint32_t    error_count;
    time_t      last_error_time;
    char        serial_number[32];
} delta_v_charm_module_t;

typedef struct {
    uint8_t        carrier_id;
    delta_v_charm_module_t slots[8];
    bool           power_healthy;
    bool           communication_healthy;
    uint16_t       temperature_celsius;
    uint8_t        firmware_version_major;
    uint8_t        firmware_version_minor;
    bool           address_configured;
    uint8_t        base_address;
} delta_v_carrier_t;

typedef struct {
    uint8_t        controller_id;
    delta_v_controller_type_t hw_type;
    delta_v_controller_mode_t mode;
    char           controller_name[32];
    char           controller_description[128];
    uint8_t        firmware_version[4];
    uint32_t       memory_kb;
    uint32_t       flash_storage_kb;
    uint32_t       nvram_kb;
    double         cpu_load_percent;
    double         memory_usage_percent;
    uint32_t       uptime_seconds;
    delta_v_io_bus_type_t io_bus;
    uint8_t        carrier_count;
    delta_v_carrier_t carriers[12];
    uint16_t       active_channel_count;
    uint16_t       healthy_channel_count;
    uint32_t       total_dsts;
    bool           redundancy_enabled;
    bool           redundancy_active;
    uint8_t        partner_controller_id;
    uint32_t       scan_cycle_base_us;
    double         scan_overrun_percent;
    uint32_t       comm_error_count;
    uint32_t       crc_error_count;
    time_t         last_config_download;
    bool           license_valid;
    uint32_t       license_dst_limit;
    bool           sis_capable;
    uint8_t        sil_level;
    double         ambient_temp_celsius;
    double         cpu_temp_celsius;
} delta_v_controller_config_t;

void delta_v_controller_config_init(delta_v_controller_config_t *ctrl, delta_v_controller_type_t type, uint8_t id);
bool delta_v_controller_add_carrier(delta_v_controller_config_t *ctrl, const delta_v_carrier_t *carrier);
bool delta_v_controller_remove_carrier(delta_v_controller_config_t *ctrl, uint8_t carrier_id);
bool delta_v_controller_configure_channel(delta_v_controller_config_t *ctrl, uint8_t carrier_id, uint8_t slot, uint8_t channel, const delta_v_charm_io_channel_t *config);
bool delta_v_controller_decommission_channel(delta_v_controller_config_t *ctrl, uint8_t carrier_id, uint8_t slot, uint8_t channel);
double delta_v_signal_convert_raw_to_eu(double raw_value, const delta_v_charm_io_channel_t *channel);
double delta_v_signal_convert_eu_to_raw(double eu_value, const delta_v_charm_io_channel_t *channel);
bool delta_v_signal_is_in_range(double value, const delta_v_charm_io_channel_t *channel);
bool delta_v_controller_health_check(const delta_v_controller_config_t *ctrl);
uint16_t delta_v_controller_count_active_channels(const delta_v_controller_config_t *ctrl);
uint16_t delta_v_controller_count_faulty_channels(const delta_v_controller_config_t *ctrl);
double delta_v_controller_availability_calculate(const delta_v_controller_config_t *ctrl);
double delta_v_controller_mtbf_estimate(const delta_v_controller_config_t *ctrl);
const char *delta_v_controller_type_to_string(delta_v_controller_type_t type);
const char *delta_v_charm_signal_type_to_string(delta_v_charm_signal_type_t type);
const char *delta_v_controller_mode_to_string(delta_v_controller_mode_t mode);
const char *delta_v_io_bus_type_to_string(delta_v_io_bus_type_t type);

#endif
