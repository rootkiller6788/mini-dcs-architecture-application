#ifndef CENTUM_VP_REDUNDANCY_H
#define CENTUM_VP_REDUNDANCY_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    REDUN_ROLE_PRIMARY   = 0,
    REDUN_ROLE_STANDBY   = 1,
    REDUN_ROLE_OFFLINE   = 2,
    REDUN_ROLE_SYNCING   = 3,
    REDUN_ROLE_MAINT     = 4,
    REDUN_ROLE_ISOLATED  = 5
} centum_redundancy_role_t;

typedef enum {
    REDUN_FAILOVER_MANUAL    = 0,
    REDUN_FAILOVER_AUTO      = 1,
    REDUN_FAILOVER_SCHEDULED = 2,
    REDUN_FAILOVER_FAULT     = 3
} centum_failover_type_t;

typedef enum {
    REDUN_PAIR_CPU     = 0,
    REDUN_PAIR_POWER   = 1,
    REDUN_PAIR_VNET    = 2,
    REDUN_PAIR_IO_BUS  = 3,
    REDUN_PAIR_IO_MOD  = 4,
    REDUN_PAIR_FAN     = 5
} centum_redundancy_pair_type_t;

typedef enum {
    REDUN_SYNC_STATE_IDLE          = 0,
    REDUN_SYNC_STATE_COPYING       = 1,
    REDUN_SYNC_STATE_EQUALIZING    = 2,
    REDUN_SYNC_STATE_TRACKING      = 3,
    REDUN_SYNC_STATE_SYNCHRONIZED  = 4,
    REDUN_SYNC_STATE_MISMATCH      = 5
} centum_sync_state_t;

typedef struct {
    centum_redundancy_role_t   role;
    centum_sync_state_t        sync_state;
    uint32_t    sync_data_bytes;
    uint32_t    sync_data_total;
    double      sync_progress_percent;
    time_t      last_sync_time;
    time_t      last_role_change;
    bool        hardware_healthy;
    bool        software_healthy;
    bool        memory_consistent;
    bool        database_consistent;
} centum_cpu_pair_member_t;

typedef struct {
    uint16_t    pair_id;
    centum_redundancy_pair_type_t pair_type;
    centum_cpu_pair_member_t primary;
    centum_cpu_pair_member_t standby;
    bool        pair_healthy;
    uint32_t    failover_count;
    time_t      last_failover_time;
    centum_failover_type_t last_failover_type;
    uint32_t    uptime_synchronized_seconds;
} centum_redundancy_pair_t;

typedef struct {
    centum_redundancy_pair_t cpu_pair;
    centum_redundancy_pair_t power_pair;
    centum_redundancy_pair_t vnet_pair_a;
    centum_redundancy_pair_t vnet_pair_b;
    centum_redundancy_pair_t io_bus_pair;
    centum_redundancy_pair_t io_module_pairs[32];
    uint16_t    io_module_pair_count;
    bool        system_redundant;
    bool        failover_in_progress;
    time_t      last_system_failover;
} centum_redundancy_config_t;

typedef struct {
    char        event_description[128];
    centum_failover_type_t failover_type;
    time_t      event_time;
    centum_redundancy_role_t prev_role;
    centum_redundancy_role_t new_role;
    bool        success;
    double      switchover_time_ms;
    char        root_cause[128];
} centum_failover_event_t;

#define CENTUM_REDUN_MAX_FAILOVER_LOG 256

typedef struct {
    centum_failover_event_t events[CENTUM_REDUN_MAX_FAILOVER_LOG];
    uint16_t    event_count;
    uint16_t    write_index;
} centum_failover_log_t;

void centum_redundancy_config_init(centum_redundancy_config_t *redun);
void centum_redundancy_pair_init(centum_redundancy_pair_t *pair, centum_redundancy_pair_type_t type, uint16_t id);

bool centum_redundancy_set_role(centum_cpu_pair_member_t *member, centum_redundancy_role_t role);
bool centum_redundancy_initiate_sync(centum_redundancy_pair_t *pair);
bool centum_redundancy_check_sync_health(const centum_redundancy_pair_t *pair);
double centum_redundancy_sync_progress(const centum_redundancy_pair_t *pair);

bool centum_redundancy_perform_failover(centum_redundancy_pair_t *pair,
                                         centum_failover_type_t type,
                                         centum_failover_log_t *log);
bool centum_redundancy_manual_switchover(centum_redundancy_pair_t *pair,
                                          centum_failover_log_t *log);
bool centum_redundancy_validate_pair_health(const centum_redundancy_pair_t *pair);

double centum_redundancy_calculate_availability(const centum_redundancy_config_t *redun);
double centum_redundancy_mtbf_hours(const centum_redundancy_pair_t *pair);
double centum_redundancy_mttr_seconds(const centum_redundancy_pair_t *pair);

void centum_failover_log_init(centum_failover_log_t *log);
void centum_failover_log_add(centum_failover_log_t *log, const centum_failover_event_t *event);
const centum_failover_event_t *centum_failover_log_get_latest(const centum_failover_log_t *log);
void centum_failover_log_print_summary(const centum_failover_log_t *log);

bool centum_redundancy_is_bumpless_possible(const centum_redundancy_pair_t *pair);
double centum_redundancy_switchover_time_estimate(const centum_redundancy_pair_t *pair);

const char *centum_redundancy_role_to_string(centum_redundancy_role_t role);
const char *centum_sync_state_to_string(centum_sync_state_t state);
const char *centum_failover_type_to_string(centum_failover_type_t type);

#endif