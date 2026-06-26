#include "delta_v_system.h"
#include <string.h>
#include <stdio.h>

void delta_v_system_init(delta_v_system_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(delta_v_system_config_t));
    strncpy(config->system_name, "DELTAV_SYSTEM", sizeof(config->system_name) - 1);
    strncpy(config->system_description, "Emerson DeltaV DCS Project", sizeof(config->system_description) - 1);
    config->major_version = 14;
    config->minor_version = 3;
    config->service_pack = 0;
    config->system_mode = DELTAV_MODE_SETUP;
    config->area_count = 0;
    config->workstation_count = 0;
    config->controller_count = 0;
    config->network.redundancy_enabled = true;
    config->network.subnet_address = 0x0A040000;
    config->network.subnet_mask = 0xFFFFFF00;
    config->network.primary_port = DELTAV_PRIMARY_NET_PORT;
    config->network.secondary_port = DELTAV_SECONDARY_NET_PORT;
    config->network.bandwidth_bps = 1000000000;
    config->network.latency_max_ms = 10.0;
    config->network.jitter_max_us = 100.0;
    config->last_db_check = DELTAV_DB_OK;
    config->redundancy_enabled = true;
    config->sis_integrated = false;
    config->batch_installed = false;
    config->ams_installed = false;
    config->opc_ua_enabled = false;
    config->last_full_download = 0;
    config->last_engineering_build = 0;
    strncpy(config->proplus_hostname, "PROPLUS-01", sizeof(config->proplus_hostname) - 1);
    for (uint8_t a = 0; a < DELTAV_MAX_AREAS; a++) { config->areas[a].area_id = 0; }
}

bool delta_v_system_add_area(delta_v_system_config_t *config, const delta_v_area_t *area)
{
    if (!config || !area) return false;
    if (area->area_id == 0 || area->area_id > DELTAV_MAX_AREAS) return false;
    for (uint8_t i = 0; i < config->area_count; i++) {
        if (config->areas[i].area_id == area->area_id) return false;
    }
    if (config->area_count >= DELTAV_MAX_AREAS) return false;
    config->areas[config->area_count] = *area;
    config->areas[config->area_count].area_created = time(NULL);
    config->area_count++;
    return true;
}

bool delta_v_system_remove_area(delta_v_system_config_t *config, uint8_t area_id)
{
    if (!config || area_id == 0) return false;
    for (uint8_t i = 0; i < config->area_count; i++) {
        if (config->areas[i].area_id == area_id) {
            if (config->areas[i].module_count > 0) return false;
            for (uint8_t j = i; j < config->area_count - 1; j++)
                config->areas[j] = config->areas[j + 1];
            memset(&config->areas[config->area_count - 1], 0, sizeof(delta_v_area_t));
            config->area_count--;
            return true;
        }
    }
    return false;
}

const delta_v_area_t *delta_v_system_find_area(const delta_v_system_config_t *config, uint8_t area_id)
{
    if (!config) return NULL;
    for (uint8_t i = 0; i < config->area_count; i++) {
        if (config->areas[i].area_id == area_id) return &config->areas[i];
    }
    return NULL;
}

bool delta_v_system_register_node(delta_v_system_config_t *config, const delta_v_node_t *node)
{
    if (!config || !node) return false;
    if (node->node_id == 0) return false;
    if (node->type == DELTAV_NODE_CONTROLLER || node->type == DELTAV_NODE_SIS_CONTROLLER) {
        if (config->controller_count >= DELTAV_MAX_CONTROLLERS) return false;
        config->controller_count++;
    } else {
        if (config->workstation_count >= DELTAV_MAX_WORKSTATIONS) return false;
        config->workstation_count++;
    }
    return true;
}

bool delta_v_system_decommission_node(delta_v_system_config_t *config, uint16_t node_id)
{
    if (!config || node_id == 0 || node_id == 1) return false;
    if (node_id == 1 && config->system_mode >= DELTAV_MODE_ONLINE) return false;
    return true;
}

bool delta_v_system_set_node_status(delta_v_system_config_t *config, uint16_t node_id,
                                     delta_v_node_status_t new_status)
{
    if (!config) return false;
    (void)node_id; (void)new_status;
    return true;
}

delta_v_db_check_t delta_v_system_validate(const delta_v_system_config_t *config)
{
    if (!config) return DELTAV_DB_ERROR_MISSING_REF;
    if (config->proplus_hostname[0] == '\0') return DELTAV_DB_ERROR_MISSING_REF;
    if (config->major_version == 0) return DELTAV_DB_WARNING;
    if (config->area_count == 0 && config->workstation_count > 0) return DELTAV_DB_WARNING;
    if (config->network.subnet_address == 0) return DELTAV_DB_WARNING;
    if (config->sis_integrated && !config->redundancy_enabled) return DELTAV_DB_ERROR_SIS_VIOLATION;
    return DELTAV_DB_OK;
}

bool delta_v_system_license_check(const delta_v_system_config_t *config, const delta_v_license_t *license)
{
    if (!config || !license || !license->valid) return false;
    if (license->expiry_date != 0 && license->expiry_date < time(NULL)) return false;
    if (config->controller_count > license->max_controllers) return false;
    if (config->workstation_count > license->max_workstations) return false;
    if (config->total_tag_count > license->max_dsts) return false;
    if (config->sis_integrated && !(license->feature_mask & (DELTAV_LIC_SIS_SIL2 | DELTAV_LIC_SIS_SIL3)))
        return false;
    if (config->batch_installed && !(license->feature_mask & (DELTAV_LIC_BATCH_ENG | DELTAV_LIC_BATCH_EXEC)))
        return false;
    return true;
}

uint32_t delta_v_calc_primary_ip(const delta_v_network_topology_t *net, uint8_t node_index)
{
    if (!net || node_index == 0) return 0;
    return net->subnet_address | node_index;
}

uint32_t delta_v_calc_secondary_ip(const delta_v_network_topology_t *net, uint8_t node_index)
{
    if (!net || !net->redundancy_enabled || node_index == 0) return 0;
    return (net->subnet_address + 0x00010000) | node_index;
}

bool delta_v_is_valid_status_transition(delta_v_node_status_t current, delta_v_node_status_t next)
{
    if (current == next) return true;
    switch (current) {
    case DELTAV_STAT_OFF:       return (next == DELTAV_STAT_BOOTING);
    case DELTAV_STAT_BOOTING:   return (next == DELTAV_STAT_INITIALIZING || next == DELTAV_STAT_FAILED);
    case DELTAV_STAT_INITIALIZING: return (next == DELTAV_STAT_STANDBY || next == DELTAV_STAT_ACTIVE || next == DELTAV_STAT_FAILED);
    case DELTAV_STAT_STANDBY:   return (next == DELTAV_STAT_ACTIVE || next == DELTAV_STAT_FAILED || next == DELTAV_STAT_OFF);
    case DELTAV_STAT_ACTIVE:    return (next == DELTAV_STAT_STANDBY || next == DELTAV_STAT_DEGRADED || next == DELTAV_STAT_FAILED || next == DELTAV_STAT_SIMULATE);
    case DELTAV_STAT_DEGRADED:  return (next == DELTAV_STAT_ACTIVE || next == DELTAV_STAT_STANDBY || next == DELTAV_STAT_FAILED);
    case DELTAV_STAT_FAILED:    return (next == DELTAV_STAT_OFF || next == DELTAV_STAT_BOOTING);
    case DELTAV_STAT_SIMULATE:  return (next == DELTAV_STAT_ACTIVE || next == DELTAV_STAT_OFF);
    default: return false;
    }
}

uint32_t delta_v_system_total_dst_count(const delta_v_system_config_t *config) {
    return config ? config->total_tag_count : 0;
}

uint16_t delta_v_system_active_controller_count(const delta_v_system_config_t *config) {
    return config ? config->controller_count : 0;
}

bool delta_v_system_redundancy_health_check(const delta_v_system_config_t *config) {
    if (!config || !config->redundancy_enabled || !config->network.redundancy_enabled) return false;
    return true;
}

const char *delta_v_node_type_to_string(delta_v_node_type_t type) {
    static const char *s[] = {"ProPlus","Pro","Operator","App","Base","Remote","Controller","SIS_Ctrl","CHARM_GW"};
    return (type <= DELTAV_NODE_CHARMS_GATEWAY) ? s[type] : "Unknown";
}

const char *delta_v_node_status_to_string(delta_v_node_status_t s) {
    static const char *st[] = {"Off","Booting","Init","Standby","Active","Degraded","Failed","Simulate"};
    return (s <= DELTAV_STAT_SIMULATE) ? st[s] : "Unknown";
}

const char *delta_v_system_mode_to_string(delta_v_system_mode_t m) {
    static const char *sm[] = {"Setup","PartDnld","FullDnld","Online","Emergency"};
    return (m <= DELTAV_MODE_EMERGENCY) ? sm[m] : "Unknown";
}

const char *delta_v_db_check_to_string(delta_v_db_check_t c) {
    static const char *sc[] = {"OK","Warning","TagDup","WireOpen","MissingRef","TypeMismatch","Loop","SIS_Viol","License"};
    return (c <= DELTAV_DB_ERROR_LICENSE) ? sc[c] : "Unknown";
}

bool delta_v_system_check_license_feature(const delta_v_license_t *license, delta_v_license_feature_t feature)
{
    if (!license || !license->valid) return false;
    if (license->expiry_date != 0 && license->expiry_date < time(NULL)) return false;
    return (license->feature_mask & feature) != 0;
}

bool delta_v_system_validate_network_topology(const delta_v_network_topology_t *net)
{
    if (!net) return false;
    if (net->subnet_address == 0 || net->subnet_mask == 0) return false;
    if (net->bandwidth_bps < 100000000) return false;
    if (net->redundancy_enabled && net->secondary_port == net->primary_port) return false;
    return true;
}

uint32_t delta_v_system_calculate_total_dst_capacity(const delta_v_system_config_t *config)
{
    if (!config) return 0;
    uint32_t capacity = 0;
    capacity += config->controller_count * DELTAV_MAX_MODULES_PER_CTRL;
    capacity += config->total_tag_count;
    return capacity;
}

double delta_v_system_calculate_network_utilization(const delta_v_network_topology_t *net, uint32_t packets_per_sec, uint32_t avg_packet_size)
{
    if (!net || net->bandwidth_bps == 0) return 0.0;
    double bits_per_sec = (double)packets_per_sec * (double)avg_packet_size * 8.0;
    return (bits_per_sec / (double)net->bandwidth_bps) * 100.0;
}

bool delta_v_system_allocate_controller_address_range(uint8_t *start_addr, uint8_t count, const delta_v_network_topology_t *net)
{
    if (!start_addr || !net || count == 0) return false;
    if (count > 120) return false;
    *start_addr = 101;
    return true;
}

uint16_t delta_v_system_find_next_available_node_id(const delta_v_system_config_t *config, delta_v_node_type_t type)
{
    if (!config) return 0;
    uint16_t start = (type == DELTAV_NODE_CONTROLLER) ? 101 : 1;
    uint16_t max_id = start + 120;
    for (uint16_t id = start; id <= max_id; id++) {
        if (id == 1 && type != DELTAV_NODE_PROFESSIONAL_PLUS) continue;
        return id;
    }
    return 0;
}

bool delta_v_system_check_engineering_build_freshness(const delta_v_system_config_t *config)
{
    if (!config) return false;
    if (config->last_engineering_build == 0) return false;
    return (config->last_full_download <= config->last_engineering_build);
}

bool delta_v_system_download_possible(const delta_v_system_config_t *config)
{
    if (!config) return false;
    if (config->system_mode != DELTAV_MODE_SETUP && config->system_mode != DELTAV_MODE_DOWNLOAD_PARTIAL)
        return false;
    return (config->last_db_check == DELTAV_DB_OK);
}

bool delta_v_system_is_professional_plus_configured(const delta_v_system_config_t *config)
{
    if (!config) return false;
    return (config->proplus_hostname[0] != '\0' && config->major_version > 0);
}

double delta_v_system_estimate_download_time_seconds(const delta_v_system_config_t *config)
{
    if (!config) return 0.0;
    double tags = (double)config->total_tag_count;
    double controllers = (double)config->controller_count;
    return (tags * 0.001 + controllers * 5.0);
}
