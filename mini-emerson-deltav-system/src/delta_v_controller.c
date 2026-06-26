#include "delta_v_controller.h"
#include "delta_v_system.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

void delta_v_controller_config_init(delta_v_controller_config_t *ctrl, delta_v_controller_type_t type, uint8_t id)
{
    if (!ctrl) return;
    memset(ctrl, 0, sizeof(delta_v_controller_config_t));
    ctrl->controller_id = id;
    ctrl->hw_type = type;
    ctrl->mode = DELTAV_CTRL_MODE_PROGRAM;
    ctrl->io_bus = DELTAV_IOBUS_CHARMS;
    ctrl->scan_cycle_base_us = 100000;
    ctrl->sis_capable = false;
    ctrl->sil_level = 0;
    ctrl->ambient_temp_celsius = 25.0;
    ctrl->cpu_temp_celsius = 25.0;
    ctrl->firmware_version[0] = 14; ctrl->firmware_version[1] = 3;
    ctrl->memory_kb = 256000; ctrl->flash_storage_kb = 512000; ctrl->nvram_kb = 32000;
    snprintf(ctrl->controller_name, sizeof(ctrl->controller_name), "CTRL-%02d", id);
}

bool delta_v_controller_add_carrier(delta_v_controller_config_t *ctrl, const delta_v_carrier_t *carrier)
{
    if (!ctrl || !carrier) return false;
    if (ctrl->carrier_count >= 12) return false;
    for (uint8_t i = 0; i < ctrl->carrier_count; i++) {
        if (ctrl->carriers[i].carrier_id == carrier->carrier_id) return false;
    }
    ctrl->carriers[ctrl->carrier_count] = *carrier;
    ctrl->carrier_count++;
    return true;
}

bool delta_v_controller_remove_carrier(delta_v_controller_config_t *ctrl, uint8_t carrier_id)
{
    if (!ctrl) return false;
    for (uint8_t i = 0; i < ctrl->carrier_count; i++) {
        if (ctrl->carriers[i].carrier_id == carrier_id) {
            for (uint8_t j = i; j < ctrl->carrier_count - 1; j++)
                ctrl->carriers[j] = ctrl->carriers[j + 1];
            memset(&ctrl->carriers[ctrl->carrier_count - 1], 0, sizeof(delta_v_carrier_t));
            ctrl->carrier_count--;
            return true;
        }
    }
    return false;
}

bool delta_v_controller_configure_channel(delta_v_controller_config_t *ctrl, uint8_t carrier_id, uint8_t slot, uint8_t channel, const delta_v_charm_io_channel_t *config)
{
    if (!ctrl || !config || slot >= 8 || channel >= 8) return false;
    for (uint8_t c = 0; c < ctrl->carrier_count; c++) {
        if (ctrl->carriers[c].carrier_id == carrier_id) {
            ctrl->carriers[c].slots[slot].channels[channel] = *config;
            ctrl->carriers[c].slots[slot].installed = true;
            ctrl->active_channel_count++;
            ctrl->healthy_channel_count++;
            return true;
        }
    }
    return false;
}

bool delta_v_controller_decommission_channel(delta_v_controller_config_t *ctrl, uint8_t carrier_id, uint8_t slot, uint8_t channel)
{
    if (!ctrl || slot >= 8 || channel >= 8) return false;
    for (uint8_t c = 0; c < ctrl->carrier_count; c++) {
        if (ctrl->carriers[c].carrier_id == carrier_id) {
            memset(&ctrl->carriers[c].slots[slot].channels[channel], 0, sizeof(delta_v_charm_io_channel_t));
            ctrl->active_channel_count--;
            return true;
        }
    }
    return false;
}

double delta_v_signal_convert_raw_to_eu(double raw, const delta_v_charm_io_channel_t *ch)
{
    if (!ch) return 0.0;
    double denom = ch->raw_max - ch->raw_min;
    if (fabs(denom) < 1e-12) return 0.0;
    return ch->eu_min + (raw - ch->raw_min) * (ch->eu_max - ch->eu_min) / denom;
}

double delta_v_signal_convert_eu_to_raw(double eu, const delta_v_charm_io_channel_t *ch)
{
    if (!ch) return 0.0;
    double denom = ch->eu_max - ch->eu_min;
    if (fabs(denom) < 1e-12) return 0.0;
    return ch->raw_min + (eu - ch->eu_min) * (ch->raw_max - ch->raw_min) / denom;
}

bool delta_v_signal_is_in_range(double value, const delta_v_charm_io_channel_t *ch)
{
    if (!ch) return false;
    return (value >= ch->eu_min - 0.01 * fabs(ch->eu_max - ch->eu_min)) &&
           (value <= ch->eu_max + 0.01 * fabs(ch->eu_max - ch->eu_min));
}

bool delta_v_controller_health_check(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return false;
    if (ctrl->mode == DELTAV_CTRL_MODE_FAILSAFE) return false;
    if (ctrl->cpu_temp_celsius > 85.0) return false;
    if (ctrl->scan_overrun_percent > 80.0) return false;
    return ctrl->healthy_channel_count * 100 >= ctrl->active_channel_count * 95;
}

uint16_t delta_v_controller_count_active_channels(const delta_v_controller_config_t *ctrl) {
    return ctrl ? ctrl->active_channel_count : 0;
}

uint16_t delta_v_controller_count_faulty_channels(const delta_v_controller_config_t *ctrl) {
    return ctrl ? (ctrl->active_channel_count - ctrl->healthy_channel_count) : 0;
}

double delta_v_controller_availability_calculate(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return 0.0;
    double lambda_ctrl = 1.0 / 350400.0;
    double lambda_io = 1.0 / 876000.0;
    double mttr = 8.0;
    double u_ctrl = 1.0 - exp(-lambda_ctrl * 8760);
    double a_ctrl = 1.0 - u_ctrl * mttr / 8760.0;
    double u_io = 1.0 - exp(-lambda_io * 8760 * ctrl->active_channel_count);
    double a_io = 1.0 - u_io * mttr / 8760.0;
    return a_ctrl * a_io;
}

double delta_v_controller_mtbf_estimate(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return 0.0;
    double base_mtbf = 350400.0;
    double channel_penalty = ctrl->active_channel_count * 0.02;
    double temp_penalty = (ctrl->ambient_temp_celsius > 40.0) ? 0.15 : 0.0;
    return base_mtbf * (1.0 - channel_penalty - temp_penalty);
}

const char *delta_v_controller_type_to_string(delta_v_controller_type_t type) {
    static const char *s[] = {"MD","MD_Plus","MX","SQ","SD","SZ","PK"};
    return (type <= DELTAV_CTRL_PK) ? s[type] : "Unknown";
}

const char *delta_v_charm_signal_type_to_string(delta_v_charm_signal_type_t type) {
    static const char *s[] = {"AI_4-20mA_HART","AI_0-20mA","AI_1-5V","AI_0-10V","AI_TC_K","AI_TC_J","AI_RTD_PT100","AI_PULSE","AO_4-20mA_HART","DI_24VDC","DI_120VAC","DO_24VDC","DO_RELAY"};
    return (type <= DELTAV_SIG_DO_RELAY) ? s[type] : "Unknown";
}

const char *delta_v_controller_mode_to_string(delta_v_controller_mode_t mode) {
    static const char *s[] = {"RUN","PROGRAM","FAILSAFE","MAINT","SIMULATE"};
    return (mode <= DELTAV_CTRL_MODE_SIMULATE) ? s[mode] : "Unknown";
}

const char *delta_v_io_bus_type_to_string(delta_v_io_bus_type_t type) {
    static const char *s[] = {"Local","CHARMs","Profibus_DP","DeviceNet","AS-i","Foundation_H1"};
    return (type <= DELTAV_IOBUS_FOUNDATION_H1) ? s[type] : "Unknown";
}

double delta_v_controller_calculate_cpu_temperature_margin(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return 0.0;
    double max_temp = 85.0;
    return max_temp - ctrl->cpu_temp_celsius;
}

bool delta_v_controller_is_overloaded(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return false;
    return (ctrl->cpu_load_percent > 85.0 || ctrl->scan_overrun_percent > 20.0 || ctrl->memory_usage_percent > 90.0);
}

uint32_t delta_v_controller_estimate_remaining_dsts(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return 0;
    uint32_t limit = ctrl->license_dst_limit;
    return (limit > ctrl->total_dsts) ? (limit - ctrl->total_dsts) : 0;
}

double delta_v_charm_signal_noise_estimate(const delta_v_charm_io_channel_t *ch, double raw_value)
{
    (void)raw_value;
    if (!ch) return 0.0;
    double span = fabs(ch->raw_max - ch->raw_min);
    if (span < 1e-12) return 0.0;
    return 0.001 * span;
}

bool delta_v_charm_channel_diagnose(const delta_v_charm_io_channel_t *ch)
{
    if (!ch) return false;
    if (!ch->channel_active) return true;
    if (ch->open_circuit_alarm || ch->short_circuit_alarm) return false;
    return !ch->fault_detected;
}

double delta_v_controller_calculate_power_consumption(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return 0.0;
    double base_watts = 35.0;
    double channel_watts = ctrl->active_channel_count * 0.5;
    double carrier_watts = ctrl->carrier_count * 2.0;
    double cpu_factor = 1.0 + ctrl->cpu_load_percent / 100.0;
    return (base_watts + channel_watts + carrier_watts) * cpu_factor;
}

bool delta_v_controller_validate_io_configuration(const delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return false;
    uint16_t total_channels = 0;
    for (uint8_t c = 0; c < ctrl->carrier_count; c++) {
        for (uint8_t s = 0; s < 8; s++) {
            if (ctrl->carriers[c].slots[s].installed) {
                for (uint8_t ch = 0; ch < 8; ch++)
                    if (ctrl->carriers[c].slots[s].channels[ch].channel_active) total_channels++;
            }
        }
    }
    return (total_channels <= DELTAV_MAX_IO_PER_CONT);
}

double delta_v_controller_mean_time_between_failure(const delta_v_controller_config_t *ctrl, double ambient_temp)
{
    if (!ctrl) return 0.0;
    double base_mtbf = 350400.0;
    double temp_factor = 1.0;
    if (ambient_temp > 40.0) temp_factor = exp((ambient_temp - 40.0) / 10.0);
    return base_mtbf / temp_factor;
}

bool delta_v_controller_perform_self_test(delta_v_controller_config_t *ctrl)
{
    if (!ctrl) return false;
    bool ram_ok = (ctrl->memory_kb > 0);
    bool flash_ok = (ctrl->flash_storage_kb > 0);
    bool nvram_ok = (ctrl->nvram_kb > 0);
    bool temp_ok = (ctrl->cpu_temp_celsius < 90.0);
    bool health_ok = (ctrl->healthy_channel_count > 0 || ctrl->active_channel_count == 0);
    return ram_ok && flash_ok && nvram_ok && temp_ok && health_ok;
}
