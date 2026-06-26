/**
 * ff_h1_datalink.c ? Foundation Fieldbus H1 Data Link Layer Implementation
 *
 * Implements LAS scheduling engine, token passing, Live List management,
 * Link Master election algorithm, DL-address allocation, VCR handling,
 * and CD schedule execution.
 *
 * Knowledge Levels: L1, L2, L3, L5
 */

#include "ff_h1_datalink.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* ============================================================================
 * L5: LAS Context Management
 * ============================================================================ */

void ff_las_init(ff_las_context_t *ctx, uint8_t las_address) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = FF_LAS_STATE_STARTUP;
    ctx->las_address = las_address;
    ctx->token_pass_overhead_us = 200; /* Typical PT overhead: 200 ?s */
    ff_live_list_init(&ctx->live_list);
    ctx->cd_schedule.entries = NULL;
    ctx->cd_schedule.count = 0;
    ctx->cd_schedule.capacity = 0;
    ctx->cd_schedule.macrocycle_us = 1000000; /* Default 1 second */
}

int ff_las_cd_add(ff_las_context_t *ctx, const ff_cd_entry_t *entry) {
    if (!ctx || !entry) return -1;

    /* Must be in ascending offset order */
    if (ctx->cd_schedule.count > 0) {
        ff_cd_entry_t *last = &ctx->cd_schedule.entries[ctx->cd_schedule.count - 1];
        if (entry->offset_us <= last->offset_us) {
            return -1; /* Not monotonic ascending */
        }
    }

    size_t new_count = ctx->cd_schedule.count + 1;
    ff_cd_entry_t *new_entries = realloc(ctx->cd_schedule.entries,
                                          new_count * sizeof(ff_cd_entry_t));
    if (!new_entries) return -1;

    ctx->cd_schedule.entries = new_entries;
    ctx->cd_schedule.entries[ctx->cd_schedule.count] = *entry;
    ctx->cd_schedule.count = new_count;
    ctx->cd_schedule.capacity = new_count;

    return 0;
}


/* ============================================================================
 * L5: LAS Macrocycle Execution
 *
 * Within each macrocycle:
 *   1. Execute CD schedule entries at their scheduled offsets
 *   2. Fill remaining time with token passes (round-robin)
 *
 * In a real implementation, this would be driven by hardware timers and
 * interrupts. Here we simulate the scheduling decision logic.
 * ============================================================================ */

int ff_las_run_macrocycle(ff_las_context_t *ctx) {
    if (!ctx) return -1;
    if (ctx->state != FF_LAS_STATE_ACTIVE) return -1;

    ctx->macrocycle_start_us = ctx->current_time_us;
    uint32_t cycle_end = ctx->macrocycle_start_us + ctx->cd_schedule.macrocycle_us;

    int executed = 0;
    uint32_t last_cd_end_us = ctx->macrocycle_start_us;

    /* Phase 1: Execute CD schedule */
    for (size_t i = 0; i < ctx->cd_schedule.count; i++) {
        ff_cd_entry_t *entry = &ctx->cd_schedule.entries[i];
        uint32_t scheduled_time = ctx->macrocycle_start_us + entry->offset_us;

        /* Simulate waiting until scheduled time */
        ctx->current_time_us = scheduled_time;

        /* Check for overrun: if we are already past the scheduled time */
        if (ctx->current_time_us > scheduled_time + entry->max_response_us) {
            ctx->cd_overruns++;
            continue; /* Skip this entry ? overrun */
        }

        /* Simulate CD execution: issue CD, device responds within max_response_us */
        uint32_t cd_duration = 500; /* Typical CD + response: ~500 ?s */
        if (cd_duration > entry->max_response_us) {
            cd_duration = entry->max_response_us;
        }
        uint32_t cd_end = scheduled_time + cd_duration;

        ctx->current_time_us = cd_end;
        last_cd_end_us = cd_end;
        executed++;
    }

    /* Phase 2: Unscheduled traffic via token passing */
    uint32_t remaining_us = cycle_end - last_cd_end_us;
    uint32_t pt_time = ctx->token_pass_overhead_us + 300; /* PT + typical response time */

    size_t pt_count = 0;
    while (remaining_us >= pt_time && pt_count < ctx->live_list.count) {
        uint8_t next_dev = ff_live_list_next_token(&ctx->live_list, &ctx->next_pt_index);
        if (next_dev == 0) break;

        ctx->current_time_us += pt_time;
        remaining_us -= pt_time;
        pt_count++;
    }

    ctx->idle_time_accumulated_us += remaining_us;
    ctx->macrocycle_count++;

    return executed;
}

double ff_las_cd_utilization(const ff_las_context_t *ctx) {
    if (!ctx || ctx->cd_schedule.macrocycle_us == 0) return -1.0;

    uint32_t total_cd_us = 0;
    for (size_t i = 0; i < ctx->cd_schedule.count; i++) {
        /* Estimate each CD entry consumes ~500 ?s */
        total_cd_us += 500;
    }

    return (double)total_cd_us / (double)ctx->cd_schedule.macrocycle_us;
}

int ff_las_has_idle_time(const ff_las_context_t *ctx, uint32_t min_idle_us) {
    if (!ctx) return 0;

    uint32_t cd_total = 0;
    for (size_t i = 0; i < ctx->cd_schedule.count; i++) {
        cd_total += 500; /* Per-entry time estimate */
    }

    uint32_t idle = ctx->cd_schedule.macrocycle_us - cd_total;
    return (idle >= min_idle_us) ? 1 : 0;
}


/* ============================================================================
 * L5: Link Master Election Algorithm
 *
 * Hold-off time: Higher-priority (lower value) LMs wait less time before
 * sending Claim LAS. This ensures deterministic election outcomes.
 *
 * holdoff_ms = (priority_value - 1) * slot_time_ms
 *
 * Priority 0x01: holdoff = 0 * 4 ms = 0 ms  (wins immediately)
 * Priority 0x80: holdoff = 127 * 4 ms = 508 ms
 * Priority 0xFF: holdoff = 254 * 4 ms = 1016 ms
 *
 * Reference: IEC 61158-4 ?9.4 "LAS redundancy and transfer"
 * ============================================================================ */

uint32_t ff_lm_holdoff_ms(uint8_t priority) {
    if (priority == 0) return 0; /* Invalid priority */
    return (uint32_t)(priority - 1) * FF_LAS_HOLDOFF_BASE_MS;
}

int ff_lm_election_compare(uint8_t priority_a, uint8_t priority_b) {
    /* Lower numerical value = higher priority = wins */
    if (priority_a < priority_b) return 1;
    if (priority_a > priority_b) return -1;
    return 0; /* Equal priority ? tie (both proceed to next tiebreaker rule) */
}


/* ============================================================================
 * L5: Live List Operations
 * ============================================================================ */

void ff_live_list_init(ff_live_list_t *list) {
    if (!list) return;
    memset(list, 0, sizeof(*list));
}

int ff_live_list_add(ff_live_list_t *list, uint8_t address, uint8_t device_class) {
    if (!list) return -1;
    if (address < FF_DL_ADDR_PERM_MIN || address > FF_DL_ADDR_PERM_MAX) return -1;

    /* Check if already exists */
    for (size_t i = 0; i < list->count; i++) {
        if (list->entries[i].dl_address == address) {
            list->entries[i].device_class = device_class;
            list->entries[i].is_operational = 1;
            return 0; /* Updated existing entry */
        }
    }

    /* Check capacity */
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        ff_live_list_entry_t *new_entries = realloc(list->entries,
                                                      new_cap * sizeof(ff_live_list_entry_t));
        if (!new_entries) return -1;
        list->entries = new_entries;
        list->capacity = new_cap;
    }

    list->entries[list->count].dl_address = address;
    list->entries[list->count].device_class = device_class;
    list->entries[list->count].is_operational = 1;
    list->entries[list->count].last_seen_us = 0;
    list->count++;

    return 0;
}

int ff_live_list_remove(ff_live_list_t *list, uint8_t address) {
    if (!list) return -1;

    for (size_t i = 0; i < list->count; i++) {
        if (list->entries[i].dl_address == address) {
            /* Shift remaining entries left */
            if (i < list->count - 1) {
                memmove(&list->entries[i], &list->entries[i + 1],
                        (list->count - i - 1) * sizeof(ff_live_list_entry_t));
            }
            list->count--;
            return 0;
        }
    }
    return -1; /* Not found */
}

int ff_live_list_find(const ff_live_list_t *list, uint8_t address) {
    if (!list) return 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->entries[i].dl_address == address && list->entries[i].is_operational) {
            return 1;
        }
    }
    return 0;
}

int ff_live_list_count_operational(const ff_live_list_t *list) {
    if (!list) return 0;
    int count = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->entries[i].is_operational) count++;
    }
    return count;
}

void ff_live_list_mark_seen(ff_live_list_t *list, uint8_t address, uint32_t timestamp) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        if (list->entries[i].dl_address == address) {
            list->entries[i].last_seen_us = timestamp;
            return;
        }
    }
}

int ff_live_list_is_full(const ff_live_list_t *list) {
    if (!list) return 0;
    return (list->count >= FF_DL_MAX_PERMANENT_DEVICES) ? 1 : 0;
}

uint8_t ff_live_list_next_token(const ff_live_list_t *list, size_t *current_index) {
    if (!list || !current_index || list->count == 0) return 0;

    /* Find the next operational device in round-robin order */
    size_t start = *current_index % list->count;
    for (size_t attempt = 0; attempt < list->count; attempt++) {
        size_t idx = (start + attempt) % list->count;
        if (list->entries[idx].is_operational) {
            *current_index = (idx + 1) % list->count;
            return list->entries[idx].dl_address;
        }
    }
    return 0; /* No operational devices */
}