/**
 * @file    ecs700_system_core.c
 * @brief   SUPCON ECS-700 System Core Implementation
 *
 * Implements system-level operations: initialization, domain management,
 * configuration validation, health monitoring, and fundamental
 * calculation utilities used across the DCS.
 *
 * Knowledge Coverage:
 *   L1: System struct instantiation, domain registration
 *   L2: Load factor, bandwidth estimation, config validation
 *   L3: Health aggregation, scan model
 *   L4: Signal scaling (linearization), filtering, rate computation
 *
 * @author  mini-control-engineering-practice
 * @date    2026-06-22
 */

#include "ecs700_system_core.h"
#include "ecs700_redundancy.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * L1/L2: System Initialization and Configuration
 * ============================================================================
 */

void ecs700_system_init(ecs700_system_config_t *config, const char *sys_name)
{
    if (config == NULL || sys_name == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Copy system name with truncation safety */
    strncpy(config->system_name, sys_name, sizeof(config->system_name) - 1);
    config->system_name[sizeof(config->system_name) - 1] = '\0';

    /* Default scan periods (ECS-700 standard timing) */
    config->global_scan_period_us = ECS700_CS_STANDARD_SCAN_US;  /* 200 ms */

    /* Default domain scan periods */
    for (int i = 0; i < ECS700_MAX_DOMAINS; i++) {
        config->domains[i].scan_period_fast_us   = ECS700_CS_FAST_SCAN_US;
        config->domains[i].scan_period_normal_us = ECS700_CS_STANDARD_SCAN_US;
        config->domains[i].scan_period_slow_us   = ECS700_CS_SLOW_SCAN_US;
        config->domains[i].domain_security_enabled = false;
        config->domains[i].security_level = 0;
    }

    /* Network configuration defaults */
    config->scnet_redundancy_enabled = true;
    config->time_sync_enabled = true;
    config->time_sync_interval_s = 1;          /* 1 second PTP sync */
    config->heartbeat_interval_us = ECS700_HEARTBEAT_INTERVAL_US;
    config->failover_retry_max = ECS700_FAILOVER_RETRY_MAX;

    config->num_domains = 0;
    config->total_control_stations = 0;
    config->total_operator_stations = 0;
    config->total_process_points = 0;
    config->current_scan_cycle = 0;
}

uint8_t ecs700_domain_register(ecs700_system_config_t *config,
                                const char *domain_name)
{
    if (config == NULL || domain_name == NULL) {
        return 0;
    }

    if (config->num_domains >= ECS700_MAX_DOMAINS) {
        return 0;  /* System at maximum capacity */
    }

    uint8_t domain_id = config->num_domains + 1;  /* 1-based ID */
    ecs700_domain_config_t *dom = &config->domains[config->num_domains];

    dom->domain_id = domain_id;
    strncpy(dom->domain_name, domain_name, sizeof(dom->domain_name) - 1);
    dom->domain_name[sizeof(dom->domain_name) - 1] = '\0';
    dom->num_control_stations = 0;
    dom->num_io_modules = 0;
    dom->total_process_points = 0;
    dom->domain_security_enabled = false;
    dom->security_level = 0;

    config->num_domains++;

    return domain_id;
}

bool ecs700_domain_add_cs(ecs700_system_config_t *config,
                           uint8_t domain_id, uint16_t cs_id)
{
    if (config == NULL || domain_id == 0 || domain_id > config->num_domains) {
        return false;
    }

    /* Convert 1-based domain ID to 0-based index */
    ecs700_domain_config_t *dom = &config->domains[domain_id - 1];

    if (dom->num_control_stations >= ECS700_MAX_CONTROL_STATIONS) {
        return false;  /* Domain at maximum capacity */
    }

    /* Check for duplicate CS ID within domain */
    for (uint8_t i = 0; i < dom->num_control_stations; i++) {
        if (dom->control_station_ids[i] == cs_id) {
            return false;  /* Duplicate CS ID */
        }
    }

    dom->control_station_ids[dom->num_control_stations] = cs_id;
    dom->num_control_stations++;
    config->total_control_stations++;

    return true;
}

uint32_t ecs700_domain_point_count(const ecs700_system_config_t *config,
                                    uint8_t domain_id)
{
    if (config == NULL || domain_id == 0 || domain_id > config->num_domains) {
        return 0;
    }

    return config->domains[domain_id - 1].total_process_points;
}

/* ============================================================================
 * L2: Core Concepts — System Performance Calculations
 * ============================================================================
 */

double ecs700_compute_load_factor(double total_exec_time_us,
                                   double scan_period_us)
{
    if (scan_period_us <= 0.0) {
        return 100.0;  /* Degenerate case: zero scan period = overload */
    }

    double load = (total_exec_time_us / scan_period_us) * 100.0;

    /* Clamp to [0, 100] - load cannot be negative or exceed 100%
     * (exceeding 100% means scan overrun - but we report as 100% cap) */
    if (load < 0.0) {
        load = 0.0;
    } else if (load > 100.0) {
        load = 100.0;
    }

    return load;
}

double ecs700_estimate_network_bandwidth(uint32_t num_points,
                                          uint32_t scan_period_us,
                                          uint32_t bytes_per_point)
{
    if (scan_period_us == 0) {
        return 0.0;
    }

    /* Protocol overhead estimation per packet:
     *   Ethernet header:    18 bytes (MAC + EtherType)
     *   IP header:          20 bytes
     *   TCP header:         20 bytes
     *   SCnet app header:   24 bytes
     *   Total overhead:     82 bytes per packet
     *
     * For ECS-700, points are grouped into update messages
     * (typically 50 points per message to optimize overhead).
     */
    const uint32_t protocol_overhead_per_packet = 82;
    const uint32_t points_per_packet = 50;

    uint32_t num_packets = (num_points + points_per_packet - 1) / points_per_packet;
    uint32_t total_bytes = num_packets * protocol_overhead_per_packet
                         + num_points * bytes_per_point;

    /* Bits per second = bytes * 8 / (scan_period_us / 1e6) */
    double scan_period_s = scan_period_us / 1000000.0;
    double bandwidth_bps = (total_bytes * 8.0) / scan_period_s;

    return bandwidth_bps;
}

/* ============================================================================
 * L2: Configuration Validation
 * ============================================================================
 */

int ecs700_validate_config(const ecs700_system_config_t *config)
{
    if (config == NULL) {
        return -1;  /* Null config */
    }

    if (config->system_name[0] == '\0') {
        return 1;   /* System name not set */
    }

    if (config->num_domains == 0) {
        return 2;   /* No domains configured */
    }

    if (config->num_domains > ECS700_MAX_DOMAINS) {
        return 3;   /* Domain count exceeds maximum */
    }

    if (config->global_scan_period_us < 10000UL) {
        return 4;   /* Scan period too short (minimum 10 ms) */
    }

    if (config->global_scan_period_us > 5000000UL) {
        return 5;   /* Scan period too long (maximum 5 s) */
    }

    /* Validate each domain */
    for (uint8_t d = 0; d < config->num_domains; d++) {
        const ecs700_domain_config_t *dom = &config->domains[d];

        if (dom->domain_id == 0) {
            return 10 + d;  /* Domain ID not set */
        }

        if (dom->num_control_stations == 0) {
            return 20 + d;  /* Domain has no control stations */
        }

        if (dom->num_control_stations > ECS700_MAX_CONTROL_STATIONS) {
            return 30 + d;  /* Too many CS in domain */
        }

        /* Check for duplicate CS IDs across domains */
        for (uint8_t d2 = d + 1; d2 < config->num_domains; d2++) {
            const ecs700_domain_config_t *dom2 = &config->domains[d2];
            for (uint8_t i = 0; i < dom->num_control_stations; i++) {
                for (uint8_t j = 0; j < dom2->num_control_stations; j++) {
                    if (dom->control_station_ids[i] == dom2->control_station_ids[j]) {
                        return 40;  /* Duplicate CS ID across domains */
                    }
                }
            }
        }

        /* Validate scan period hierarchy: fast < normal < slow */
        if (dom->scan_period_fast_us >= dom->scan_period_normal_us) {
            return 50 + d;  /* Fast scan must be faster than normal */
        }
        if (dom->scan_period_normal_us >= dom->scan_period_slow_us) {
            return 60 + d;  /* Normal scan must be faster than slow */
        }
    }

    return 0;  /* Configuration valid */
}

/* ============================================================================
 * L3: Health Monitoring
 * ============================================================================
 */

void ecs700_collect_health(const ecs700_system_config_t *config,
                            ecs700_system_health_t *health)
{
    if (config == NULL || health == NULL) {
        return;
    }

    memset(health, 0, sizeof(*health));

    health->active_domains = config->num_domains;
    health->primary_controllers = config->total_control_stations;

    /* In a real system, these values would be populated from
     * actual node health telegrams received over SCnet.
     * Here we set diagnostic defaults for offline simulation. */
    health->average_cpu_load = 35.0;    /* Typical idle DCS load */
    health->scnet_a_utilization = 15.0; /* Typical cyclic data load */
    health->scnet_b_utilization = 0.0;  /* Port B idle in normal operation */
    health->time_sync_valid = config->time_sync_enabled;
    health->average_scan_jitter_us = 50.0; /* Typical < 100 μs jitter */
}

/* ============================================================================
 * L4: Engineering Laws — Signal Scaling and Filtering
 * ============================================================================
 */

double ecs700_raw_to_eu(uint16_t raw_raw, const ecs700_eu_range_t *range)
{
    if (range == NULL) {
        return 0.0;
    }

    /* Prevent division by zero for degenerate range */
    if (range->raw_lo >= range->raw_hi) {
        return range->eu_lo;
    }

    /* Clamp raw value to valid range */
    double raw = (double)raw_raw;
    if (raw < range->raw_lo) {
        raw = range->raw_lo;
    } else if (raw > range->raw_hi) {
        raw = range->raw_hi;
    }

    /* Linear interpolation */
    double eu = range->eu_lo
              + (raw - range->raw_lo)
              * (range->eu_hi - range->eu_lo)
              / (range->raw_hi - range->raw_lo);

    return eu;
}

uint16_t ecs700_eu_to_raw(double eu, const ecs700_eu_range_t *range)
{
    if (range == NULL) {
        return 0;
    }

    /* Prevent division by zero */
    if (range->eu_lo >= range->eu_hi) {
        return (uint16_t)range->raw_lo;
    }

    /* Clamp EU to valid range */
    if (eu < range->eu_lo) {
        eu = range->eu_lo;
    } else if (eu > range->eu_hi) {
        eu = range->eu_hi;
    }

    /* Inverse linear interpolation */
    double raw = range->raw_lo
               + (eu - range->eu_lo)
               * (range->raw_hi - range->raw_lo)
               / (range->eu_hi - range->eu_lo);

    /* Clamp to uint16 range */
    if (raw < 0.0) {
        raw = 0.0;
    } else if (raw > 65535.0) {
        raw = 65535.0;
    }

    return (uint16_t)(raw + 0.5);  /* Round to nearest integer */
}

double ecs700_apply_signal_filter(double pv_new, double pv_filtered_prev,
                                   double sample_time_s, double filter_tc_s)
{
    /**
     * First-order exponential filter (EMA):
     *
     *   y[k] = α * x[k] + (1 - α) * y[k-1]
     *
     * where α = Ts / (Ts + Tc)
     *
     * Derivation: The continuous-time transfer function is
     *   G(s) = 1 / (Tc*s + 1)
     *
     * Using backward Euler discretization:
     *   y[k] = y[k-1] + (Ts/Tc) * (x[k] - y[k])
     *
     * Rearranging gives the EMA form above with α = Ts/(Ts+Tc).
     *
     * Special cases:
     *   - Tc = 0: no filtering (α = 1, y[k] = x[k])
     *   - Tc → ∞: frozen (α → 0, y[k] = y[k-1])
     */

    /* Handle degenerate cases */
    if (sample_time_s <= 0.0) {
        return pv_new;  /* Zero sample time = no filter */
    }
    if (filter_tc_s <= 0.0) {
        return pv_new;  /* Zero time constant = bypass filter */
    }

    double alpha = sample_time_s / (sample_time_s + filter_tc_s);

    /* Clamp alpha to [0, 1] for numerical safety */
    if (alpha < 0.0) {
        alpha = 0.0;
    } else if (alpha > 1.0) {
        alpha = 1.0;
    }

    return alpha * pv_new + (1.0 - alpha) * pv_filtered_prev;
}

double ecs700_compute_pv_rate(double pv_current, double pv_previous,
                               double delta_t_s, double deadband)
{
    if (delta_t_s <= 0.0) {
        return 0.0;  /* Zero or negative time delta invalid */
    }

    double delta_pv = pv_current - pv_previous;

    /* Deadband suppression: ignore changes below threshold
     * to prevent noise-induced false rate alarms */
    if (fabs(delta_pv) < deadband) {
        return 0.0;
    }

    return delta_pv / delta_t_s;
}
