/**
 * @file dcs_alarm.h
 * @brief DCS alarm management per ISA-18.2 / IEC 62682.
 *
 * Knowledge Level: L2 Core Concepts + L4 Engineering Standards
 *
 * References:
 *   - ISA-18.2 / IEC 62682: Management of Alarm Systems for the Process Industries
 *   - EEMUA Publication 191: Alarm Systems — A Guide to Design, Management, and Procurement
 *   - Honeywell Experion PKS Alarm Management
 *   - Yokogawa CENTUM VP Alarm Rationalization
 *   - ASM Consortium guidelines for alarm management
 *
 * Covers alarm lifecycle, alarm states, rationalization, shelving,
 * suppression, flood analysis, and alarm system performance metrics.
 */

#ifndef DCS_ALARM_H
#define DCS_ALARM_H

#include "dcs_types.h"
#include <stdint.h>

/*===========================================================================
 * L1: Alarm Definitions (ISA-18.2)
 *===========================================================================*/

/**
 * @brief Alarm type classification per ISA-18.2.
 *
 * ABSOLUTE:  Setpoint compared to process value (PV > HI, PV < LO).
 * DEVIATION: PV deviation from setpoint (|PV - SP| > DEV).
 * RATE:      Rate of change exceeds limit (dPV/dt > ROC).
 * DIGITAL:   Discrete state change (pump stopped, valve closed).
 * DISCREPANCY: Commanded state vs. actual state mismatch.
 * STATISTICAL: Statistical alarm based on process model.
 * SYSTEM:    Hardware/software diagnostic alarm.
 */
typedef enum {
    DCS_ALARM_ABSOLUTE    = 0,
    DCS_ALARM_DEVIATION   = 1,
    DCS_ALARM_RATE        = 2,
    DCS_ALARM_DIGITAL     = 3,
    DCS_ALARM_DISCREPANCY = 4,
    DCS_ALARM_STATISTICAL = 5,
    DCS_ALARM_SYSTEM      = 6
} dcs_alarm_type_t;

/**
 * @brief Alarm priority per ISA-18.2 / EEMUA 191.
 *
 * Priority 1 (Emergency/Critical): Immediate operator action required.
 * Priority 2 (High):              Prompt operator action required.
 * Priority 3 (Medium):            Operator awareness required.
 * Priority 4 (Low):               Informational, no immediate action.
 *
 * Priority is determined by consequence severity × response time.
 */
typedef enum {
    DCS_ALARM_PRIORITY_CRITICAL = 1,
    DCS_ALARM_PRIORITY_HIGH     = 2,
    DCS_ALARM_PRIORITY_MEDIUM   = 3,
    DCS_ALARM_PRIORITY_LOW      = 4,
    DCS_ALARM_PRIORITY_JOURNAL  = 5   /* Logged only, not annunciated */
} dcs_alarm_priority_t;

/**
 * @brief Alarm state machine states (ISA-18.2 lifecycle states).
 */
typedef enum {
    DCS_ALARM_STATE_NORMAL       = 0,  /* No alarm condition */
    DCS_ALARM_STATE_UNACK_ACTIVE = 1,  /* Alarm active, not acknowledged */
    DCS_ALARM_STATE_ACK_ACTIVE   = 2,  /* Alarm active, acknowledged */
    DCS_ALARM_STATE_UNACK_CLEAR  = 3,  /* Alarm cleared, not acknowledged */
    DCS_ALARM_STATE_ACK_CLEAR    = 4,  /* Alarm cleared and acknowledged */
    DCS_ALARM_STATE_SHELVED      = 5,  /* Alarm temporarily suppressed */
    DCS_ALARM_STATE_SUPPRESSED   = 6,  /* Alarm suppressed by logic */
    DCS_ALARM_STATE_OUT_OF_SVC   = 7   /* Alarm out of service */
} dcs_alarm_state_t;

/*===========================================================================
 * L2: Alarm Configuration
 *===========================================================================*/

/**
 * @brief Alarm configuration for a single tag.
 *
 * Each process tag (AI, AO, DI, DO, PID, etc.) can have
 * multiple configured alarms: HH, HI, LO, LL, ROC, DEV, etc.
 */
typedef struct {
    uint32_t            alarm_id;
    uint32_t            associated_tag_id;
    char                alarm_name[48];
    char                alarm_description[128];
    dcs_alarm_type_t    type;
    dcs_alarm_priority_t priority;
    double              setpoint;           /* Trip value in EU */
    double              hysteresis;         /* Deadband to prevent chatter */
    double              on_delay_s;         /* Delay before activation */
    double              off_delay_s;        /* Delay before clearing */
    int                 enabled;
    int                 latching;           /* Must be manually reset */
    char                consequence[64];    /* Consequence if not responded */
    double              response_time_s;    /* Max allowed response time */
    dcs_alarm_state_t   current_state;
    double              current_value;
    uint64_t            activation_timestamp;
    uint64_t            acknowledgement_timestamp;
    uint64_t            clear_timestamp;
} dcs_alarm_config_t;

/*===========================================================================
 * L4: Alarm Rationalization (ISA-18.2 Clause 9)
 *===========================================================================*/

/**
 * @brief Alarm rationalization record.
 *
 * ISA-18.2 requires that every alarm be justified by a rationalization
 * that documents the hazard, consequence, operator action, and priority.
 */
typedef struct {
    uint32_t  alarm_id;
    int       is_rationalized;
    int       is_justified;        /* Alarm has a documented purpose */
    char      consequence_of_inaction[128];
    char      operator_action[128];
    double    time_to_respond_s;
    int       priority_matches_risk;  /* Priority aligns with hazard */
    int       has_safety_consequence;
} dcs_alarm_rationalization_t;

/*===========================================================================
 * L4: Alarm System Performance Metrics (ISA-18.2 Clause 16)
 *===========================================================================*/

/**
 * @brief Alarm system key performance indicators.
 *
 * ISA-18.2 / EEMUA 191 benchmarks:
 *   - Average alarm rate: < 1 alarm per 10 minutes (steady state)
 *   - Peak alarm rate:    < 10 alarms per 10 minutes
 *   - Standing (active) alarms: < 10
 *   - Stale alarms (unacknowledged > 24h): 0
 *   - Annunciated priority distribution:
 *       Critical ≤ 5%, High ≤ 15%, Medium 15-20%, Low 60-75%
 *   - Chattering alarms: 0
 *   - Alarm flood threshold: > 10 alarms in 10 minutes
 */
typedef struct {
    double    avg_alarms_per_hour;
    double    peak_alarms_per_10min;
    uint32_t  standing_alarms;
    uint32_t  stale_alarms;
    double    chattering_count;        /* Alarms with > 3 transitions in 1 min */
    uint32_t  flood_events_24h;        /* Flood events in last 24 hours */
    double    priority_pct[6];         /* Distribution: indices 1-5 for each priority */
    double    operator_response_rate;  /* Fraction acknowledged within time_to_respond */
    int       kpi_acceptable;
} dcs_alarm_system_kpi_t;

/*===========================================================================
 * L3: Alarm Core Functions
 *===========================================================================*/

/**
 * @brief Process an alarm state transition.
 *
 * Core alarm state machine that manages the full lifecycle:
 * NORMAL → UNACK_ACTIVE → ACK_ACTIVE → UNACK_CLEAR → ACK_CLEAR → NORMAL
 *
 * @param alarm        The alarm configuration (modified in place).
 * @param new_value    New process value to evaluate against setpoints.
 * @return             1 if alarm state changed, 0 if unchanged.
 */
int dcs_alarm_process(dcs_alarm_config_t *alarm, double new_value);

/**
 * @brief Evaluate alarm activation with hysteresis and on-delay.
 *
 * Prevents chattering by requiring:
 *   - On-delay: signal must exceed threshold for on_delay_s continuously.
 *   - Hysteresis: once active, signal must fall below setpoint - hysteresis to clear.
 *
 * @param alarm        Alarm configuration.
 * @param value        Current process value.
 * @param dt_s         Time step since last evaluation.
 * @return             1 if alarm should be active, 0 if should be clear.
 */
int dcs_alarm_evaluate_with_hysteresis(dcs_alarm_config_t *alarm,
                                        double value, double dt_s);

/**
 * @brief Acknowledge an active alarm.
 *
 * @param alarm        The alarm to acknowledge.
 * @return             1 on success, 0 if alarm not in acknowledged-able state.
 */
int dcs_alarm_acknowledge(dcs_alarm_config_t *alarm);

/**
 * @brief Shelve an alarm for a specified duration.
 *
 * Per ISA-18.2, shelving temporarily suppresses alarm annunciation
 * for a defined period. Unshelving is automatic on expiry.
 *
 * @param alarm          Alarm to shelve.
 * @param duration_min   Shelve duration in minutes.
 * @return               1 on success, 0 on failure.
 */
int dcs_alarm_shelve(dcs_alarm_config_t *alarm, double duration_min);

/**
 * @brief Check if shelved alarm should auto-unshelve.
 *
 * @param alarm          Shelved alarm.
 * @param elapsed_min    Minutes since shelved.
 * @return               1 if should unshelve, 0 otherwise.
 */
int dcs_alarm_check_unshelve(dcs_alarm_config_t *alarm, double elapsed_min);

/*===========================================================================
 * L5: Algorithm — Alarm Rationalization
 *===========================================================================*/

/**
 * @brief Perform alarm rationalization per ISA-18.2 methodology.
 *
 * Evaluates whether an alarm is justified based on:
 *   1. Does a hazard or undesirable situation exist?
 *   2. Can the operator respond in time?
 *   3. Is there another alarm that already covers this?
 *   4. Does the alarm provide actionable information?
 *
 * @param alarm          Alarm to rationalize.
 * @param rationalization Output: rationalization assessment.
 * @return               1 if alarm is justified, 0 if it should be removed.
 */
int dcs_alarm_rationalize(const dcs_alarm_config_t *alarm,
                           dcs_alarm_rationalization_t *rationalization);

/*===========================================================================
 * L5: Algorithm — Alarm Flood Detection
 *===========================================================================*/

/**
 * @brief Detect alarm flood condition.
 *
 * ISA-18.2 defines alarm flood: > 10 alarms in any 10-minute window.
 *
 * @param alarm_timestamps  Array of alarm activation timestamps (sorted).
 * @param num_alarms        Number of alarms in the array.
 * @param window_min        Analysis window in minutes (default: 10).
 * @param threshold         Flood threshold (default: 10).
 * @param flood_start       Output: index of first alarm in flood, or -1.
 * @param flood_count       Output: number of alarms in flood.
 * @return                  1 if flood detected, 0 otherwise.
 */
int dcs_alarm_detect_flood(const uint64_t *alarm_timestamps,
                            uint32_t num_alarms,
                            double window_min,
                            uint32_t threshold,
                            int32_t *flood_start,
                            uint32_t *flood_count);

/**
 * @brief Calculate alarm system KPI metrics.
 *
 * @param alarm_configs      Array of alarm configurations.
 * @param num_alarms         Number of configured alarms.
 * @param observation_hours  Observation period in hours.
 * @param kpi                Output: KPI metrics.
 * @return                   1 on success.
 */
int dcs_alarm_calculate_kpi(const dcs_alarm_config_t *alarm_configs,
                             uint32_t num_alarms,
                             double observation_hours,
                             dcs_alarm_system_kpi_t *kpi);

/*===========================================================================
 * L6: Classic Problem — Alarm Chatter Prevention
 *===========================================================================*/

/**
 * @brief Analyze alarm chattering and recommend hysteresis.
 *
 * Chatter = alarm transitions > N times in observation window.
 * Recommended hysteresis for absolute alarms: 2% of span, min 0.5% of range.
 *
 * @param setpoint           Alarm setpoint value.
 * @param signal_noise_std   Standard deviation of process noise.
 * @param recommended_hyst   Output: recommended hysteresis value.
 * @return                   Predicted chatter frequency (transitions/hour).
 */
double dcs_alarm_recommend_hysteresis(double setpoint,
                                       double signal_noise_std,
                                       double *recommended_hyst);

#endif /* DCS_ALARM_H */
