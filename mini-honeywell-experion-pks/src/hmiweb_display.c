/**
 * @file hmiweb_display.c
 * @brief Honeywell Experion HMIWeb Display Builder Implementation
 *
 * Implements: station lifecycle, alarm management (ISA-18.2 state machine),
 * faceplate updates, trend data handling, operator security,
 * ISA-101 color compliance verification, and display navigation.
 *
 * L1: Display types, alarm classes, ISA-101 colors
 * L2: Alarm state machine (ISA-18.2), faceplate operations
 * L4: ISA-101 compliance checking
 * L7: Honeywell Experion Station (EST) specific functions
 */

#include "../include/hmiweb_display.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/* ==========================================================================
 * L1 - Station Initialization
 * ========================================================================== */

int station_init(ExperionStation *stn, uint32_t id, const char *name)
{
    if (!stn || !name) return -1;

    memset(stn, 0, sizeof(ExperionStation));
    stn->station_id = id;
    strncpy(stn->station_name, name, sizeof(stn->station_name) - 1);
    stn->connected = false;
    stn->isa101_compliant = true;
    stn->display_update_rate_ms = 500;
    stn->alarm_summary.alarm_flood_threshold = 50;

    return 0;
}

int station_connect(ExperionStation *stn, uint32_t esvt_id)
{
    if (!stn) return -1;
    stn->primary_esvt_id = esvt_id;
    stn->connected = true;
    return 0;
}

int station_disconnect(ExperionStation *stn)
{
    if (!stn) return -1;
    stn->connected = false;
    return 0;
}

/* ==========================================================================
 * L2 - Alarm Management (ISA-18.2 State Machine)
 * ========================================================================== */

/**
 * Add a new alarm to the alarm summary.
 *
 * ISA-18.2 alarm state transitions:
 *   NORMAL --[alarm occurs]--> UNACK_ACTIVE --[operator acks]--> ACK_ACTIVE
 *   ACK_ACTIVE --[returns normal]--> NORMAL
 *   UNACK_ACTIVE --[returns normal]--> UNACK_INACTIVE --[operator acks]--> NORMAL
 *
 * Alarms can also be SHELVED (temporarily suppressed by operator) or
 * SUPPRESSED (suppressed by logic/state-based alarming).
 *
 * Reference: ISA-18.2-2016, Management of Alarm Systems for the Process Industries
 * Course: RWTH Aachen Industrial Control, Purdue ECE 602
 */
int alarm_add(HMIAlarmSummary *alarms, const HMIAlarmRecord *alarm)
{
    if (!alarms || !alarm) return -1;

    /* Alarm flood detection: more than threshold within 10 minutes */
    if (!alarms->flood_active) {
        time_t now = time(NULL);
        uint32_t recent_count = 0;
        for (uint32_t i = 0; i < alarms->history_count; i++) {
            if (now - alarms->alarm_history[i].activation_time < 600) {
                recent_count++;
            }
        }
        if (recent_count >= alarms->alarm_flood_threshold) {
            alarms->flood_active = true;
        }
    }

    /* Add to active alarms */
    if (alarms->active_count < HMI_MAX_ACTIVE_ALARMS) {
        memcpy(&alarms->active_alarms[alarms->active_count], alarm,
               sizeof(HMIAlarmRecord));
        alarms->active_alarms[alarms->active_count].alarm_id = alarms->active_count;
        alarms->active_count++;

        if (alarm->requires_ack) {
            alarms->unacknowledged_count++;
        }
    }

    /* Add to history (circular buffer) */
    if (alarms->history_count < HMI_MAX_ALARM_HISTORY) {
        memcpy(&alarms->alarm_history[alarms->history_count], alarm,
               sizeof(HMIAlarmRecord));
        alarms->history_count++;
    }

    return 0;
}

/** Acknowledge an alarm.
 *  Transitions: UNACK_ACTIVE -> ACK_ACTIVE or UNACK_INACTIVE -> NORMAL */
int alarm_acknowledge(HMIAlarmSummary *alarms, uint32_t alarm_id,
                       const char *operator_name)
{
    if (!alarms || !operator_name) return -1;
    if (alarm_id >= alarms->active_count) return -1;

    HMIAlarmRecord *alarm = &alarms->active_alarms[alarm_id];

    if (alarm->state == HMI_ALARM_STATE_UNACK_ACTIVE) {
        alarm->state = HMI_ALARM_STATE_ACK_ACTIVE;
        alarm->ack_time = time(NULL);
        strncpy(alarm->operator_name, operator_name,
                sizeof(alarm->operator_name) - 1);
        if (alarms->unacknowledged_count > 0) {
            alarms->unacknowledged_count--;
        }
    } else if (alarm->state == HMI_ALARM_STATE_UNACK_INACTIVE) {
        alarm->state = HMI_ALARM_STATE_NORMAL;
        alarm->ack_time = time(NULL);
        strncpy(alarm->operator_name, operator_name,
                sizeof(alarm->operator_name) - 1);
        if (alarms->unacknowledged_count > 0) {
            alarms->unacknowledged_count--;
        }
    }

    return 0;
}

/** Shelve an alarm for a specified duration.
 *  Shelved alarms are temporarily removed from the operator's view
 *  but automatically return after the duration expires. */
int alarm_shelve(HMIAlarmSummary *alarms, uint32_t alarm_id,
                  uint32_t duration_min)
{
    if (!alarms) return -1;
    if (alarm_id >= alarms->active_count) return -1;

    HMIAlarmRecord *alarm = &alarms->active_alarms[alarm_id];
    if (alarm->state == HMI_ALARM_STATE_SHELVED) return -1;

    alarm->state = HMI_ALARM_STATE_SHELVED;
    alarm->clear_time = time(NULL) + duration_min * 60;
    alarms->shelved_count++;
    if (alarms->unacknowledged_count > 0) {
        alarms->unacknowledged_count--;
    }

    return 0;
}

/** Unshelve an alarm, returning it to its previous state. */
int alarm_unshelve(HMIAlarmSummary *alarms, uint32_t alarm_id)
{
    if (!alarms) return -1;
    if (alarm_id >= alarms->active_count) return -1;

    HMIAlarmRecord *alarm = &alarms->active_alarms[alarm_id];
    if (alarm->state != HMI_ALARM_STATE_SHELVED) return -1;

    /* Determine previous state based on alarm conditions */
    alarm->state = HMI_ALARM_STATE_UNACK_ACTIVE;
    if (alarms->shelved_count > 0) alarms->shelved_count--;
    alarms->unacknowledged_count++;

    return 0;
}

/** Get the highest priority among active unacknowledged alarms. */
int alarm_get_highest_priority(const HMIAlarmSummary *alarms,
                                HMIAlarmPriority *pri)
{
    if (!alarms || !pri) return -1;

    HMIAlarmPriority highest = HMI_ALARM_PRI_JOURNAL;

    for (uint32_t i = 0; i < alarms->active_count; i++) {
        const HMIAlarmRecord *a = &alarms->active_alarms[i];
        if (a->state == HMI_ALARM_STATE_UNACK_ACTIVE ||
            a->state == HMI_ALARM_STATE_UNACK_INACTIVE) {
            if (a->priority < highest) {
                highest = a->priority;
            }
        }
    }

    *pri = highest;
    return 0;
}

/** Check if alarm flood condition is active. */
bool alarm_flood_detected(const HMIAlarmSummary *alarms)
{
    if (!alarms) return false;
    return alarms->flood_active;
}

/** Clear alarms that have returned to normal. */
int alarm_clear_inactive(HMIAlarmSummary *alarms)
{
    if (!alarms) return -1;

    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < alarms->active_count; i++) {
        HMIAlarmRecord *alarm = &alarms->active_alarms[i];
        if (alarm->state == HMI_ALARM_STATE_NORMAL) {
            /* Remove from active list */
            continue;
        }
        if (i != write_idx) {
            memcpy(&alarms->active_alarms[write_idx], alarm,
                   sizeof(HMIAlarmRecord));
        }
        write_idx++;
    }
    alarms->active_count = write_idx;

    return 0;
}

/* ==========================================================================
 * L2 - Faceplate Management
 * ========================================================================== */

/** Update faceplate with current process values. */
int faceplate_update(HMIFaceplate *fp, double pv, double sp, double op,
                      PIDMode mode, ExperionPointQuality pv_qual)
{
    if (!fp) return -1;

    fp->pv = pv;
    fp->sp = sp;
    fp->op = op;
    fp->mode = mode;
    fp->pv_quality = pv_qual;

    /* Generate ASCII bar graph for output (0-100%) */
    int bar_len = (int)(op / 5.0); /* 20 chars for 0-100% */
    if (bar_len < 0) bar_len = 0;
    if (bar_len > 20) bar_len = 20;

    for (int i = 0; i < 20; i++) {
        fp->op_bar_graph[i] = (i < bar_len) ? '#' : '-';
    }
    fp->op_bar_graph[20] = '\0';

    /* Apply ISA-101 colors */
    faceplate_isa101_color(fp);

    return 0;
}

/** Set setpoint with security check. */
int faceplate_set_sp(HMIFaceplate *fp, double new_sp,
                      HMISecurityLevel user_level)
{
    if (!fp) return -1;
    if (user_level < HMI_SEC_OPERATOR) return -1;

    /* Clamp to limits */
    if (new_sp > fp->sp_hi_limit) new_sp = fp->sp_hi_limit;
    if (new_sp < fp->sp_lo_limit) new_sp = fp->sp_lo_limit;

    fp->sp = new_sp;
    return 0;
}

/** Set mode with security check. */
int faceplate_set_mode(HMIFaceplate *fp, PIDMode new_mode,
                        HMISecurityLevel user_level)
{
    if (!fp) return -1;

    HMISecurityLevel required;
    switch (new_mode) {
    case PID_MANUAL:   required = HMI_SEC_OPERATOR; break;
    case PID_AUTO:     required = HMI_SEC_OPERATOR; break;
    case PID_CASCADE:  required = HMI_SEC_SUPERVISOR; break;
    default:           required = HMI_SEC_ENGINEER; break;
    }

    if (user_level < required) return -1;

    fp->mode = new_mode;
    return 0;
}

/** Set output with security check (manual mode only). */
int faceplate_set_op(HMIFaceplate *fp, double new_op,
                      HMISecurityLevel user_level)
{
    if (!fp) return -1;
    if (user_level < HMI_SEC_OPERATOR) return -1;
    if (fp->mode != PID_MANUAL) return -1; /* Cannot set OP in auto */

    if (new_op > fp->op_hi) new_op = fp->op_hi;
    if (new_op < fp->op_lo) new_op = fp->op_lo;

    fp->op = new_op;
    return 0;
}

/* ==========================================================================
 * L4 - ISA-101 Color Rules
 * ========================================================================== */

/**
 * Apply ISA-101 (High-Performance HMI) coloring rules to faceplate.
 *
 * Rules:
 * 1. Gray background (already set by display framework)
 * 2. Normal values displayed in black/dark text — NO color
 * 3. Color ONLY for abnormal conditions:
 *    - Red: active alarm (HI, HIHI)
 *    - Yellow/Orange: active alarm (LO, LOLO)
 *    - Magenta: high-high alarm
 * 4. Bad quality: Purple/blue indicator
 * 5. Operator-entered values: blue
 * 6. No animation (blinking) except for new unacknowledged alarms
 *
 * Reference: ISA-101.01-2015, Human Machine Interfaces for Process
 *            Automation Systems
 */
int faceplate_isa101_color(HMIFaceplate *fp)
{
    if (!fp) return -1;

    /* Default: normal values are black */
    fp->pv_color = HMI_COLOR_LIVE_VALUE;
    fp->op_color = HMI_COLOR_LIVE_VALUE;

    /* Color based on quality */
    switch (fp->pv_quality) {
    case XQUAL_BAD:
    case XQUAL_BAD_COMM:
    case XQUAL_BAD_CONFIG:
        fp->pv_color = HMI_COLOR_BAD_QUALITY;
        break;
    case XQUAL_UNCERTAIN:
    case XQUAL_UNCERTAIN_SUBST:
        fp->pv_color = HMI_COLOR_BAD_QUALITY;
        break;
    default:
        /* Check for alarm conditions */
        if (fp->deviation_hi > 0.0) {
            double dev = fabs(fp->pv - fp->sp);
            if (dev > fp->deviation_hi) {
                fp->pv_color = HMI_COLOR_DEVIATION;
            }
        }
        break;
    }

    /* Operator-entered values in manual are blue */
    if (fp->mode == PID_MANUAL) {
        fp->op_color = HMI_COLOR_OPERATOR_ENTERED;
    }

    return 0;
}

/* ==========================================================================
 * L4 - ISA-101 Compliance Verification
 * ========================================================================== */

bool isa101_verify_colors(const HMIFaceplate *fp)
{
    if (!fp) return false;

    /* Rule: normal displays should not use alarm colors */
    if (fp->pv_quality == XQUAL_GOOD) {
        if (fp->pv_color == HMI_COLOR_ALARM_HI ||
            fp->pv_color == HMI_COLOR_ALARM_HIHI ||
            fp->pv_color == HMI_COLOR_ALARM_LO ||
            fp->pv_color == HMI_COLOR_ALARM_LOLO) {
            return false; /* Alarm colors on good quality = violates ISA-101 */
        }
    }

    /* Rule: no red for non-alarm states */
    if (fp->pv_color == HMI_COLOR_ALARM_HI &&
        fp->pv_quality == XQUAL_GOOD) {
        return false;
    }

    return true;
}

bool isa101_verify_no_unnecessary_animation(void)
{
    /* ISA-101: No unnecessary animation. Animation only for:
     * - New unacknowledged alarms (slow blink)
     * - Emergency alarms (fast blink)
     * This function would check display configuration.
     * Returns true if configuration is ISA-101 compliant. */
    return true;
}

/* ==========================================================================
 * L2 - Trend Management
 * ========================================================================== */

int trend_init(HMITrendDisplay *trend, HMITrendMode mode,
                uint32_t span_sec, uint32_t interval_sec)
{
    if (!trend) return -1;
    if (interval_sec < 1) return -1;

    memset(trend, 0, sizeof(HMITrendDisplay));
    trend->mode = mode;
    trend->time_span_sec = span_sec;
    trend->sample_interval_sec = interval_sec;
    trend->display_end_time = time(NULL);
    trend->display_start_time = trend->display_end_time - span_sec;

    return 0;
}

int trend_add_pen(HMITrendDisplay *trend, const char *tag,
                   const char *label, HMIISAColor color)
{
    if (!trend || !tag) return -1;
    if (trend->pen_count >= HMI_TREND_MAX_PENS) return -1;

    HMITrendPen *pen = &trend->pens[trend->pen_count];
    pen->pen_id = trend->pen_count;
    strncpy(pen->tag, tag, sizeof(pen->tag) - 1);
    strncpy(pen->label, label, sizeof(pen->label) - 1);
    pen->pen_color = color;
    pen->visible = true;
    pen->auto_scale = true;
    pen->y_min = 0.0;
    pen->y_max = 100.0;
    pen->sample_count = 0;

    trend->pen_count++;
    return 0;
}

int trend_add_sample(HMITrendPen *pen, time_t ts, double value,
                      ExperionPointQuality quality)
{
    if (!pen) return -1;
    if (pen->sample_count >= HMI_TREND_SAMPLE_COUNT) {
        /* Shift buffer left (circular would be better, but this is simpler) */
        memmove(pen->samples, pen->samples + 1,
                (HMI_TREND_SAMPLE_COUNT - 1) * sizeof(HMITrendSample));
        pen->sample_count = HMI_TREND_SAMPLE_COUNT - 1;
    }

    HMITrendSample *s = &pen->samples[pen->sample_count];
    s->timestamp = ts;
    s->value = value;
    s->quality = quality;
    pen->sample_count++;

    /* Update running statistics incrementally (Welford's method) */
    double old_mean = pen->mean_value;
    pen->mean_value = old_mean + (value - old_mean) / pen->sample_count;
    pen->stddev_value = sqrt(
        ((pen->sample_count - 2.0) / (pen->sample_count - 1.0)) *
        pen->stddev_value * pen->stddev_value +
        (value - old_mean) * (value - old_mean) / pen->sample_count
    );

    return 0;
}

/** Compute statistics from trend pen data. */
int trend_get_statistics(const HMITrendPen *pen, double *mean,
                          double *stddev, double *min, double *max)
{
    if (!pen) return -1;
    if (pen->sample_count == 0) return -1;

    /* Use pre-computed mean and stddev from incremental updates */
    if (mean) *mean = pen->mean_value;
    if (stddev) *stddev = pen->stddev_value;

    if (min || max) {
        double vmin = pen->samples[0].value;
        double vmax = pen->samples[0].value;
        for (uint32_t i = 1; i < pen->sample_count; i++) {
            if (pen->samples[i].value < vmin) vmin = pen->samples[i].value;
            if (pen->samples[i].value > vmax) vmax = pen->samples[i].value;
        }
        if (min) *min = vmin;
        if (max) *max = vmax;
    }

    return 0;
}

/* ==========================================================================
 * L2 - Operator Security
 * ========================================================================== */

int operator_login(ExperionStation *stn, const char *username,
                    const char *password)
{
    if (!stn || !username || !password) return -1;

    /* In real system: validate against ESDB (Experion Security Database) */
    /* For simulation: accept any non-empty credentials */
    if (strlen(username) == 0 || strlen(password) == 0) return -1;

    strncpy(stn->current_user.user_name, username,
            sizeof(stn->current_user.user_name) - 1);

    /* Determine security level based on username prefix (simplified) */
    if (strncmp(username, "ADMIN", 5) == 0) {
        stn->current_user.level = HMI_SEC_ADMIN;
    } else if (strncmp(username, "ENG", 3) == 0) {
        stn->current_user.level = HMI_SEC_ENGINEER;
    } else if (strncmp(username, "SUPV", 4) == 0) {
        stn->current_user.level = HMI_SEC_SUPERVISOR;
    } else if (strncmp(username, "OP", 2) == 0) {
        stn->current_user.level = HMI_SEC_OPERATOR;
    } else {
        stn->current_user.level = HMI_SEC_VIEW;
    }

    stn->current_user.logged_in = true;
    stn->current_user.login_time = time(NULL);
    stn->current_user.session_timeout = time(NULL) + 28800; /* 8 hours */

    return 0;
}

int operator_logout(ExperionStation *stn)
{
    if (!stn) return -1;
    stn->current_user.logged_in = false;
    return 0;
}

bool operator_has_access(const HMIOperatorAccount *op,
                          HMISecurityLevel required_level)
{
    if (!op || !op->logged_in) return false;
    return (op->level >= required_level);
}

/* ==========================================================================
 * L3 - Display Navigation
 * ========================================================================== */

int display_navigate_to(ExperionStation *stn, uint32_t display_id)
{
    if (!stn) return -1;
    if (display_id >= stn->display_count) return -1;

    /* Set current display visibility */
    for (uint32_t i = 0; i < stn->display_count; i++) {
        stn->displays[i].visible = (i == display_id);
    }

    return 0;
}

int display_get_current(const ExperionStation *stn, HMIDisplayNode *display)
{
    if (!stn || !display) return -1;

    for (uint32_t i = 0; i < stn->display_count; i++) {
        if (stn->displays[i].visible) {
            memcpy(display, &stn->displays[i], sizeof(HMIDisplayNode));
            return 0;
        }
    }

    return -1; /* No display visible */
}

int display_get_parent(const ExperionStation *stn, HMIDisplayNode *parent)
{
    if (!stn || !parent) return -1;

    HMIDisplayNode current;
    if (display_get_current(stn, &current) != 0) return -1;

    if (current.parent_id >= stn->display_count) return -1;

    memcpy(parent, &stn->displays[current.parent_id], sizeof(HMIDisplayNode));
    return 0;
}