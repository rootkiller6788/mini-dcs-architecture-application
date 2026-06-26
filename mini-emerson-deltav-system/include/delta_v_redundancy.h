#ifndef DELTA_V_REDUNDANCY_H
#define DELTA_V_REDUNDANCY_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    DELTAV_REDUN_ROLE_PRIMARY   = 0,
    DELTAV_REDUN_ROLE_STANDBY   = 1,
    DELTAV_REDUN_ROLE_SYNCING   = 2,
    DELTAV_REDUN_ROLE_OFFLINE   = 3,
    DELTAV_REDUN_ROLE_MAINT     = 4,
    DELTAV_REDUN_ROLE_ISOLATED  = 5
} delta_v_redundancy_role_t;

typedef enum {
    DELTAV_REDUN_FAILOVER_MANUAL    = 0,
    DELTAV_REDUN_FAILOVER_AUTO      = 1,
    DELTAV_REDUN_FAILOVER_SCHEDULED = 2,
    DELTAV_REDUN_FAILOVER_FAULT     = 3
} delta_v_failover_type_t;

typedef enum {
    DELTAV_REDUN_PAIR_CONTROLLER = 0,
    DELTAV_REDUN_PAIR_POWER      = 1,
    DELTAV_REDUN_PAIR_NETWORK    = 2,
    DELTAV_REDUN_PAIR_IO_BUS     = 3,
    DELTAV_REDUN_PAIR_CHARMS     = 4
} delta_v_redundancy_pair_type_t;

typedef enum {
    DELTAV_REDUN_SYNC_IDLE        = 0,
    DELTAV_REDUN_SYNC_COPYING     = 1,
    DELTAV_REDUN_SYNC_EQUALIZING  = 2,
    DELTAV_REDUN_SYNC_TRACKING    = 3,
    DELTAV_REDUN_SYNC_SYNCHRONIZED = 4,
    DELTAV_REDUN_SYNC_MISMATCH    = 5
} delta_v_sync_state_t;

typedef struct {
    delta_v_redundancy_role_t   role;
    delta_v_sync_state_t        sync_state;
    uint32_t    sync_data_bytes;
    uint32_t    sync_data_total;
    double      sync_progress_percent;
    time_t      last_sync_time;
    time_t      last_role_change;
    bool        hardware_healthy;
    bool        software_healthy;
    bool        database_consistent;
    bool        firmware_identical;
} delta_v_controller_member_t;

typedef struct {
    uint16_t    pair_id;
    delta_v_redundancy_pair_type_t pair_type;
    delta_v_controller_member_t primary;
    delta_v_controller_member_t standby;
    bool        pair_healthy;
    uint32_t    failover_count;
    time_t      last_failover_time;
    delta_v_failover_type_t last_failover_type;
    uint32_t    uptime_synchronized_seconds;
    double      switchover_time_ms;
    bool        bumpless_capable;
} delta_v_redundancy_pair_t;

#define DELTAV_REDUN_MAX_EVENTS 256

typedef struct {
    char        event_description[128];
    delta_v_failover_type_t failover_type;
    time_t      event_time;
    delta_v_redundancy_role_t prev_role;
    delta_v_redundancy_role_t new_role;
    bool        success;
    double      switchover_time_ms;
    char        root_cause[128];
} delta_v_failover_event_t;

typedef struct {
    delta_v_failover_event_t events[DELTAV_REDUN_MAX_EVENTS];
    uint16_t    event_count;
    uint16_t    write_index;
    double      avg_switchover_ms;
    uint32_t    total_failovers;
    uint32_t    successful_failovers;
} delta_v_failover_log_t;

void delta_v_redundancy_pair_init(delta_v_redundancy_pair_t *pair, delta_v_redundancy_pair_type_t type, uint16_t id);
bool delta_v_redundancy_set_role(delta_v_controller_member_t *member, delta_v_redundancy_role_t role);
bool delta_v_redundancy_initiate_sync(delta_v_redundancy_pair_t *pair);
bool delta_v_redundancy_check_sync_health(const delta_v_redundancy_pair_t *pair);
double delta_v_redundancy_sync_progress(const delta_v_redundancy_pair_t *pair);
bool delta_v_redundancy_perform_failover(delta_v_redundancy_pair_t *pair, delta_v_failover_type_t type, delta_v_failover_log_t *log);
bool delta_v_redundancy_manual_switchover(delta_v_redundancy_pair_t *pair, delta_v_failover_log_t *log);
bool delta_v_redundancy_validate_pair_health(const delta_v_redundancy_pair_t *pair);
double delta_v_redundancy_calculate_availability(const delta_v_redundancy_pair_t *pair);
double delta_v_redundancy_mtbf_hours(const delta_v_redundancy_pair_t *pair);
double delta_v_redundancy_mttr_seconds(const delta_v_redundancy_pair_t *pair);
void delta_v_failover_log_init(delta_v_failover_log_t *log);
void delta_v_failover_log_add(delta_v_failover_log_t *log, const delta_v_failover_event_t *event);
const delta_v_failover_event_t *delta_v_failover_log_get_latest(const delta_v_failover_log_t *log);
bool delta_v_redundancy_is_bumpless_possible(const delta_v_redundancy_pair_t *pair);
double delta_v_redundancy_nines_availability(double availability);
bool delta_v_redundancy_should_trigger_failover(const delta_v_redundancy_pair_t *pair);
uint32_t delta_v_redundancy_estimate_sync_time_ms(const delta_v_redundancy_pair_t *pair);
double delta_v_redundancy_pair_health_score(const delta_v_redundancy_pair_t *pair);
double delta_v_redundancy_calculate_system_availability(const delta_v_redundancy_pair_t *pairs, uint8_t pair_count);
bool delta_v_redundancy_check_network_redundancy(bool primary_net_up, bool secondary_net_up);
const char *delta_v_redundancy_role_to_string(delta_v_redundancy_role_t role);
const char *delta_v_sync_state_to_string(delta_v_sync_state_t state);
const char *delta_v_failover_type_to_string(delta_v_failover_type_t type);

#endif
