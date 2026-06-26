#include "delta_v_redundancy.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

void delta_v_redundancy_pair_init(delta_v_redundancy_pair_t *pair, delta_v_redundancy_pair_type_t type, uint16_t id)
{
    if (!pair) return;
    memset(pair, 0, sizeof(delta_v_redundancy_pair_t));
    pair->pair_id = id;
    pair->pair_type = type;
    pair->primary.role = DELTAV_REDUN_ROLE_PRIMARY;
    pair->standby.role = DELTAV_REDUN_ROLE_STANDBY;
    pair->primary.hardware_healthy = true; pair->primary.software_healthy = true;
    pair->standby.hardware_healthy = true; pair->standby.software_healthy = true;
    pair->pair_healthy = true;
}

bool delta_v_redundancy_set_role(delta_v_controller_member_t *member, delta_v_redundancy_role_t role)
{
    if (!member) return false;
    if (member->role == role) return true;
    if (role == DELTAV_REDUN_ROLE_PRIMARY && member->role == DELTAV_REDUN_ROLE_SYNCING)
        role = DELTAV_REDUN_ROLE_PRIMARY;
    member->role = role;
    member->last_role_change = 0;
    return true;
}

bool delta_v_redundancy_initiate_sync(delta_v_redundancy_pair_t *pair)
{
    if (!pair) return false;
    if (!pair->primary.hardware_healthy || !pair->standby.hardware_healthy) return false;
    if (pair->standby.role != DELTAV_REDUN_ROLE_STANDBY) return false;
    pair->standby.role = DELTAV_REDUN_ROLE_SYNCING;
    pair->standby.sync_state = DELTAV_REDUN_SYNC_COPYING;
    pair->standby.sync_data_bytes = 0;
    pair->standby.sync_data_total = 1000000;
    pair->standby.sync_progress_percent = 0.0;
    return true;
}

bool delta_v_redundancy_check_sync_health(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return false;
    return (pair->standby.sync_state == DELTAV_REDUN_SYNC_SYNCHRONIZED &&
            pair->primary.database_consistent && pair->standby.database_consistent &&
            pair->primary.firmware_identical);
}

double delta_v_redundancy_sync_progress(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;
    return pair->standby.sync_progress_percent;
}

bool delta_v_redundancy_perform_failover(delta_v_redundancy_pair_t *pair, delta_v_failover_type_t type, delta_v_failover_log_t *log)
{
    if (!pair || !pair->pair_healthy) return false;
    if (pair->standby.role != DELTAV_REDUN_ROLE_STANDBY && pair->standby.sync_state != DELTAV_REDUN_SYNC_SYNCHRONIZED)
        return false;
    delta_v_redundancy_role_t prev_primary = pair->primary.role;
    delta_v_redundancy_role_t prev_standby = pair->standby.role;
    (void)prev_standby;
    pair->primary.role = DELTAV_REDUN_ROLE_STANDBY;
    pair->standby.role = DELTAV_REDUN_ROLE_PRIMARY;
    pair->last_failover_type = type;
    pair->last_failover_time = 0;
    pair->failover_count++;
    if (log) {
        delta_v_failover_event_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.failover_type = type; evt.prev_role = prev_primary;
        evt.new_role = DELTAV_REDUN_ROLE_PRIMARY;
        evt.success = true; evt.switchover_time_ms = 50.0;
        snprintf(evt.event_description, sizeof(evt.event_description), "Failover pair %d type %d", pair->pair_id, type);
        delta_v_failover_log_add(log, &evt);
    }
    return true;
}

bool delta_v_redundancy_manual_switchover(delta_v_redundancy_pair_t *pair, delta_v_failover_log_t *log)
{
    return delta_v_redundancy_perform_failover(pair, DELTAV_REDUN_FAILOVER_MANUAL, log);
}

bool delta_v_redundancy_validate_pair_health(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return false;
    bool primary_ok = pair->primary.hardware_healthy && pair->primary.software_healthy;
    bool standby_ok = pair->standby.hardware_healthy && pair->standby.software_healthy;
    return primary_ok && standby_ok;
}

double delta_v_redundancy_calculate_availability(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;
    double a_single = 0.999;
    double a_parallel = 1.0 - (1.0 - a_single) * (1.0 - a_single);
    return a_parallel;
}

double delta_v_redundancy_mtbf_hours(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;
    double base_mtbf = 350400.0;
    if (pair->pair_type == DELTAV_REDUN_PAIR_CONTROLLER) base_mtbf = 350400.0;
    else if (pair->pair_type == DELTAV_REDUN_PAIR_POWER) base_mtbf = 175200.0;
    else if (pair->pair_type == DELTAV_REDUN_PAIR_NETWORK) base_mtbf = 262800.0;
    double factor = 1.0 + (1.0 - (double)pair->failover_count / 1000.0);
    return base_mtbf * factor;
}

double delta_v_redundancy_mttr_seconds(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;
    double base_mttr = 28800.0;
    if (pair->failover_count > 0) base_mttr = 3600.0;
    return base_mttr;
}

void delta_v_failover_log_init(delta_v_failover_log_t *log)
{
    if (!log) return;
    memset(log, 0, sizeof(delta_v_failover_log_t));
}

void delta_v_failover_log_add(delta_v_failover_log_t *log, const delta_v_failover_event_t *event)
{
    if (!log || !event) return;
    log->events[log->write_index] = *event;
    log->write_index = (log->write_index + 1) % DELTAV_REDUN_MAX_EVENTS;
    if (log->event_count < DELTAV_REDUN_MAX_EVENTS) log->event_count++;
    if (event->success) log->successful_failovers++;
    log->total_failovers++;
    if (log->total_failovers > 0) log->avg_switchover_ms = (log->avg_switchover_ms * (log->total_failovers - 1) + event->switchover_time_ms) / log->total_failovers;
}

const delta_v_failover_event_t *delta_v_failover_log_get_latest(const delta_v_failover_log_t *log)
{
    if (!log || log->event_count == 0) return NULL;
    uint16_t idx = (log->write_index == 0) ? DELTAV_REDUN_MAX_EVENTS - 1 : log->write_index - 1;
    return &log->events[idx];
}

bool delta_v_redundancy_is_bumpless_possible(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return false;
    return (pair->standby.sync_state == DELTAV_REDUN_SYNC_SYNCHRONIZED &&
            pair->primary.database_consistent && pair->standby.database_consistent &&
            pair->primary.firmware_identical);
}

double delta_v_redundancy_nines_availability(double availability)
{
    if (availability <= 0.0 || availability >= 1.0) return 0.0;
    return -log10(1.0 - availability);
}

const char *delta_v_redundancy_role_to_string(delta_v_redundancy_role_t role) {
    static const char *s[] = {"Primary","Standby","Syncing","Offline","Maint","Isolated"};
    return (role <= DELTAV_REDUN_ROLE_ISOLATED) ? s[role] : "Unknown";
}

const char *delta_v_sync_state_to_string(delta_v_sync_state_t state) {
    static const char *s[] = {"Idle","Copying","Equalizing","Tracking","Synced","Mismatch"};
    return (state <= DELTAV_REDUN_SYNC_MISMATCH) ? s[state] : "Unknown";
}

const char *delta_v_failover_type_to_string(delta_v_failover_type_t type) {
    static const char *s[] = {"Manual","Auto","Scheduled","Fault"};
    return (type <= DELTAV_REDUN_FAILOVER_FAULT) ? s[type] : "Unknown";
}

double delta_v_redundancy_calculate_system_availability(const delta_v_redundancy_pair_t *pairs, uint8_t pair_count)
{
    if (!pairs || pair_count == 0) return 0.0;
    double availability = 1.0;
    for (uint8_t i = 0; i < pair_count; i++)
        availability *= delta_v_redundancy_calculate_availability(&pairs[i]);
    return availability;
}

bool delta_v_redundancy_check_network_redundancy(bool primary_net_up, bool secondary_net_up)
{
    return (primary_net_up || secondary_net_up);
}

bool delta_v_redundancy_should_trigger_failover(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return false;
    if (!pair->primary.hardware_healthy || !pair->primary.software_healthy)
        return pair->standby.hardware_healthy && pair->standby.software_healthy;
    return false;
}

uint32_t delta_v_redundancy_estimate_sync_time_ms(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return 0;
    uint32_t remaining = pair->standby.sync_data_total - pair->standby.sync_data_bytes;
    return remaining / 1000;
}

double delta_v_redundancy_pair_health_score(const delta_v_redundancy_pair_t *pair)
{
    if (!pair) return 0.0;
    double score = 0.0;
    if (pair->primary.hardware_healthy) score += 25.0;
    if (pair->primary.software_healthy) score += 25.0;
    if (pair->standby.hardware_healthy) score += 25.0;
    if (pair->standby.software_healthy) score += 25.0;
    if (pair->standby.sync_state == DELTAV_REDUN_SYNC_SYNCHRONIZED) score += 10.0;
    if (pair->primary.database_consistent && pair->standby.database_consistent) score += 10.0;
    return fmin(100.0, score);
}
