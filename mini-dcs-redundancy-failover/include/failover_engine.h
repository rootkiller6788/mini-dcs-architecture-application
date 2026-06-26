/**
 * @file failover_engine.h
 * @brief DCS Failover Engine -- State Machine, Heartbeat, Leader Election
 *
 * Part of mini-control-engineering-practice
 * Submodule: mini-dcs-redundancy-failover (7. mini-dcs-architecture-application)
 *
 * Knowledge Coverage:
 *   L2 - Core concepts: failover, bumpless transfer, split-brain avoidance
 *   L3 - Engineering structures: heartbeat protocol, failover state machine
 *   L5 - Algorithms: Bully leader election, quorum management
 *
 * Reference:
 *   - Tanenbaum & van Steen, Distributed Systems (2007) -- leader election
 *   - Cristian, F. "Understanding Fault-Tolerant Distributed Systems" (1991)
 *   - Honeywell Experion C300 Redundancy Architecture
 */

#ifndef FAILOVER_ENGINE_H
#define FAILOVER_ENGINE_H

#include "redundancy_core.h"
#include <stdint.h>
#include <stdbool.h>

#define FAILOVER_MAX_EVENTS 256
#define FAILOVER_HEARTBEAT_TIMEOUT_MS 500
#define FAILOVER_QUORUM_MIN_PCT 0.50
#define FAILOVER_MAX_ELECTION_ROUNDS 5

typedef enum {
    FEV_NONE               = 0,
    FEV_HEARTBEAT_LOST     = 1,
    FEV_HEARTBEAT_RESTORED = 2,
    FEV_PRIMARY_FAULT      = 3,
    FEV_PRIMARY_RECOVERED  = 4,
    FEV_FAILOVER_START     = 5,
    FEV_FAILOVER_COMPLETE  = 6,
    FEV_FAILBACK_START     = 7,
    FEV_FAILBACK_COMPLETE  = 8,
    FEV_SPLIT_BRAIN_DETECT = 9,
    FEV_SPLIT_BRAIN_RESOLVED = 10,
    FEV_QUORUM_LOST        = 11,
    FEV_QUORUM_RESTORED    = 12,
    FEV_ELECTION_START     = 13,
    FEV_ELECTION_WON       = 14,
    FEV_STATE_TRANSFER_START = 15,
    FEV_STATE_TRANSFER_COMPLETE = 16,
    FEV_MANUAL_SWITCHOVER  = 17
} failover_event_type_t;

typedef struct {
    failover_event_type_t type;
    uint64_t timestamp_ms;
    uint8_t  from_module;
    uint8_t  to_module;
    char     description[64];
} failover_event_t;

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_ms;
    uint8_t  module_slot;
    module_health_t health;
    double   cpu_load;
    uint32_t crc32;
} heartbeat_msg_t;

typedef struct {
    redundancy_group_t *group;
    failover_event_t    event_log[FAILOVER_MAX_EVENTS];
    uint32_t            event_count;
    uint32_t            heartbeat_timeout_ms;
    uint32_t            heartbeat_interval_ms;
    bool                auto_failback_enabled;
    uint32_t            failback_delay_ms;
    bool                split_brain_prevention;
    uint8_t             election_priority[REDUNDANCY_MAX_MODULES];
    uint64_t            last_heartbeat_time[REDUNDANCY_MAX_MODULES];
    uint32_t            missed_heartbeats[REDUNDANCY_MAX_MODULES];
    uint32_t            max_missed_heartbeats;
    bool                quorum_established;
} failover_engine_t;

int failover_engine_init(failover_engine_t *engine,
                         redundancy_group_t *group,
                         uint32_t heartbeat_ms,
                         uint32_t timeout_ms);

int failover_process_heartbeat(failover_engine_t *engine,
                               const heartbeat_msg_t *msg);

int failover_check_timeouts(failover_engine_t *engine);

int failover_execute(failover_engine_t *engine);

int failover_execute_failback(failover_engine_t *engine, uint8_t target_slot);

int failover_elect_primary(const failover_engine_t *engine);

int failover_bully_election(failover_engine_t *engine, uint8_t initiator_slot);

int failover_detect_split_brain(failover_engine_t *engine);

bool failover_quorum_check(failover_engine_t *engine);

int failover_log_event(failover_engine_t *engine,
                       failover_event_type_t type,
                       uint8_t from_module, uint8_t to_module,
                       const char *desc);

const failover_event_t *failover_last_event(const failover_engine_t *engine);

uint32_t failover_switch_time_ms(const failover_engine_t *engine);

bool failover_bumpless_possible(const failover_engine_t *engine);

double failover_observed_mttr(const failover_engine_t *engine);

int failover_dump_events(const failover_engine_t *engine,
                         char *buffer, size_t bufsize);

#endif /* FAILOVER_ENGINE_H */
