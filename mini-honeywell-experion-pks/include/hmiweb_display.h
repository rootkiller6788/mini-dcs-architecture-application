/**
 * @file hmiweb_display.h
 * @brief Honeywell Experion PKS HMIWeb Display Builder Structures
 *
 * L1: Display types, alarm classes, trend configurations, point detail
 * L2: HMI alarm philosophy (ISA-18.2), color conventions (ISA-101)
 * L3: HMIWeb display hierarchy, navigation model, security levels
 * L4: ISA-101 HMI standard, High-Performance HMI principles
 * L7: Industrial Application — Honeywell Experion Station (EST) Operator Interface
 *
 * Reference: Honeywell HMIWeb Display Builder Guide (EP-HMI-500)
 * Course: RWTH Aachen Industrial Control, Berkeley ME233
 */

#ifndef HMIWEB_DISPLAY_H
#define HMIWEB_DISPLAY_H

#include "experion_system.h"
#include "control_blocks.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1 - Display Type Definitions
 * ========================================================================== */

/** HMIWeb display types — the standard display hierarchy
 *  in Experion PKS (Level 1 through Level 4). */
typedef enum {
    HMI_DISP_SYSTEM       = 0x01,  /* System status overview (Level 1) */
    HMI_DISP_AREA         = 0x02,  /* Area/unit overview (Level 2) */
    HMI_DISP_GROUP        = 0x04,  /* Group display — up to 8 points (Level 3) */
    HMI_DISP_POINT_DETAIL = 0x08,  /* Single point detail (Level 4) */
    HMI_DISP_TREND        = 0x10,  /* Trend display — real-time and historical */
    HMI_DISP_ALARM_LIST   = 0x20,  /* Alarm summary list */
    HMI_DISP_ALARM_ANNUN  = 0x40,  /* Alarm annunciator panel */
    HMI_DISP_GRAPHIC      = 0x80,  /* Custom graphic (P\x26ID, schematic) */
    HMI_DISP_FACE_PLATE   = 0x100, /* Standard PID faceplate */
    HMI_DISP_SEQUENCE     = 0x200, /* Sequence control display */
    HMI_DISP_EVENT        = 0x400, /* Event summary / journal */
    HMI_DISP_DIAGNOSTIC   = 0x800, /* System diagnostic display */
    HMI_DISP_REPORT       = 0x1000 /* Report / log display */
} HMIDisplayType;

/* ==========================================================================
 * L1 - ISA-101 Color Palette (High-Performance HMI)
 * ========================================================================== */

/** ISA-101 compliant HMI color conventions.
 *  High-Performance HMI principles:
 *  - Gray background (reduce eye fatigue)
 *  - Color ONLY for abnormal conditions
 *  - No animation except for alarms
 *  - Analog values displayed as numbers, not bar graphs
 *  - Dark/light gray for static elements */
typedef enum {
    HMI_COLOR_BG               = 0x808080,  /* Background gray (RGB) */
    HMI_COLOR_STATIC_TEXT      = 0x000000,  /* Black — static labels */
    HMI_COLOR_LIVE_VALUE       = 0x000000,  /* Black — normal live values */
    HMI_COLOR_ALARM_HI         = 0xFF0000,  /* Red — high alarm */
    HMI_COLOR_ALARM_HIHI       = 0xFF00FF,  /* Magenta — high-high alarm */
    HMI_COLOR_ALARM_LO         = 0xFFFF00,  /* Yellow — low alarm */
    HMI_COLOR_ALARM_LOLO       = 0xFFA500,  /* Orange — low-low alarm */
    HMI_COLOR_BAD_QUALITY      = 0x8080FF,  /* Purple — bad PV quality */
    HMI_COLOR_DEVIATION        = 0x00BFFF,  /* Deep sky blue — deviation alarm */
    HMI_COLOR_NORMAL_VALVE     = 0x00FF00,  /* Green — open/running */
    HMI_COLOR_CLOSED_VALVE     = 0xFF0000,  /* Red — closed/stopped */
    HMI_COLOR_TRANSITION       = 0x00FFFF,  /* Cyan — transitioning */
    HMI_COLOR_OPERATOR_ENTERED = 0x0000FF,  /* Blue — operator-entered data */
    HMI_COLOR_DISABLED         = 0xC0C0C0   /* Silver — disabled/inactive */
} HMIISAColor;

/* ==========================================================================
 * L1 - Alarm Management (ISA-18.2)
 * ========================================================================== */

/** Alarm priority per ISA-18.2 / EEMUA 191.
 *  Priority 1: Emergency (immediate operator action required)
 *  Priority 2: High (prompt operator action required)
 *  Priority 3: Low (operator awareness)
 *  Priority 4: Journal (logged only, no annunciation) */
typedef enum {
    HMI_ALARM_PRI_EMERGENCY = 1,
    HMI_ALARM_PRI_HIGH      = 2,
    HMI_ALARM_PRI_LOW       = 3,
    HMI_ALARM_PRI_JOURNAL   = 4
} HMIAlarmPriority;

/** Alarm state machine per ISA-18.2.
 *  Normal → Unacknowledged → Acknowledged → Return-to-Normal
 *  Also supports: Shelved, Suppressed, Out-of-Service states. */
typedef enum {
    HMI_ALARM_STATE_NORMAL         = 0,
    HMI_ALARM_STATE_UNACK_ACTIVE   = 1,  /* Alarm active, not acknowledged */
    HMI_ALARM_STATE_ACK_ACTIVE     = 2,  /* Acknowledged, still active */
    HMI_ALARM_STATE_UNACK_INACTIVE = 3,  /* Returned to normal, not acked */
    HMI_ALARM_STATE_SHELVED        = 4,  /* Temporarily shelved */
    HMI_ALARM_STATE_SUPPRESSED     = 5,  /* Suppressed by logic */
    HMI_ALARM_STATE_OOS            = 6   /* Out of service */
} HMIAlarmState;

/** Single alarm record. */
typedef struct {
    uint32_t        alarm_id;           /* Unique alarm identifier */
    char            tag[24];            /* Source point tag */
    char            description[40];    /* Alarm description */
    HMIAlarmPriority priority;          /* Alarm priority */
    HMIAlarmState   state;              /* Current alarm state */
    time_t          activation_time;    /* When alarm became active */
    time_t          ack_time;           /* When acknowledged (0 if not) */
    time_t          clear_time;         /* When returned to normal */
    double          alarm_value;        /* Value at alarm trigger */
    double          alarm_limit;        /* Limit that was violated */
    ExperionPointQuality quality;       /* PV quality at alarm time */
    char            operator_name[16];  /* Operator who acknowledged */
    bool            requires_ack;       /* Whether acknowledgment is required */
} HMIAlarmRecord;

/** Alarm summary — maintains the complete alarm state for a station. */
#define HMI_MAX_ACTIVE_ALARMS  100
#define HMI_MAX_ALARM_HISTORY  500

typedef struct {
    uint32_t        active_count;
    HMIAlarmRecord  active_alarms[HMI_MAX_ACTIVE_ALARMS];
    uint32_t        shelved_count;
    uint32_t        suppressed_count;
    uint32_t        unacknowledged_count;
    uint32_t        history_count;
    HMIAlarmRecord  alarm_history[HMI_MAX_ALARM_HISTORY];
    uint32_t        alarm_flood_threshold;  /* Alarms per 10 minutes before flood */
    bool            flood_active;           /* Alarm flood condition detected */
} HMIAlarmSummary;

/* ==========================================================================
 * L1 - Trend Configuration
 * ========================================================================== */

/** Trend sample — a single data point in a trend series. */
typedef struct {
    time_t      timestamp;      /* Sample timestamp */
    double      value;          /* Sample value in EU */
    ExperionPointQuality quality; /* Quality at sample time */
} HMITrendSample;

/** Trend configuration — defines a trend display pen.
 *  Each HMIWeb trend can display up to 8 pens. */
#define HMI_TREND_MAX_PENS     8
#define HMI_TREND_SAMPLE_COUNT 1000  /* 24 hours at 1-second resolution */

typedef enum {
    HMI_TREND_REALTIME   = 0,  /* Real-time trend (last N minutes) */
    HMI_TREND_HISTORICAL = 1,  /* Historical trend (from PHD historian) */
    HMI_TREND_COMPARISON = 2   /* Multi-timeframe comparison */
} HMITrendMode;

typedef struct {
    uint32_t        pen_id;
    char            tag[24];                /* Point tag to trend */
    char            label[32];              /* Pen label */
    HMIISAColor     pen_color;              /* Trend line color */
    double          y_min;                  /* Y-axis minimum */
    double          y_max;                  /* Y-axis maximum */
    bool            visible;                /* Pen visibility */
    uint32_t        sample_count;           /* Samples collected */
    HMITrendSample  samples[HMI_TREND_SAMPLE_COUNT];
    bool            auto_scale;             /* Auto-scale Y-axis */
    double          mean_value;             /* Running mean */
    double          stddev_value;           /* Running std deviation */
} HMITrendPen;

typedef struct {
    HMITrendMode    mode;
    uint32_t        pen_count;
    HMITrendPen     pens[HMI_TREND_MAX_PENS];
    uint32_t        time_span_sec;          /* Display time span */
    uint32_t        sample_interval_sec;    /* Sampling interval */
    time_t          display_start_time;     /* Left edge timestamp */
    time_t          display_end_time;       /* Right edge timestamp */
} HMITrendDisplay;

/* ==========================================================================
 * L2 - Point Faceplate (Core Concept)
 * ========================================================================== */

/** PID faceplate data — the standard operator interface for a PID block.
 *  This is the most frequently used HMI display in process control. */
typedef struct {
    char            tag[24];                /* Point tag */
    char            description[40];        /* Point description */
    char            eu[10];                 /* Engineering units */
    double          pv;                     /* Process variable */
    double          sp;                     /* Setpoint */
    double          op;                     /* Output (%) */
    PIDMode         mode;                   /* MAN/AUTO/CASCADE */
    double          kc;                     /* Current gain */
    double          ti;                     /* Current integral time */
    double          td;                     /* Current derivative time */
    double          pv_hi;                  /* PV scale high */
    double          pv_lo;                  /* PV scale low */
    double          op_hi;                  /* OP scale high */
    double          op_lo;                  /* OP scale low */
    double          sp_hi_limit;            /* SP high limit */
    double          sp_lo_limit;            /* SP low limit */
    double          deviation_hi;           /* Deviation alarm high */
    double          deviation_lo;           /* Deviation alarm low */
    ExperionPointQuality pv_quality;        /* PV quality */
    ExperionPointQuality sp_quality;        /* SP quality */
    bool            alarm_inhibit;          /* Alarm inhibited */
    bool            scan_off;               /* Scan off */
    HMIISAColor     pv_color;               /* PV display color per ISA-101 */
    HMIISAColor     op_color;               /* OP display color */
    char            op_bar_graph[21];       /* ASCII bar graph for output */
} HMIFaceplate;

/* ==========================================================================
 * L3 - Navigation Model (Engineering Structure)
 * ========================================================================== */

/** HMI display hierarchy — the navigation tree from system overview
 *  down to individual point faceplates. */
typedef struct {
    uint32_t        display_id;
    char            title[64];              /* Display title */
    char            file_name[128];         /* Display file (.hmd) */
    HMIDisplayType  type;
    uint32_t        parent_id;              /* Parent display ID (0=root) */
    uint32_t        child_count;
    uint32_t        child_ids[32];          /* Child display IDs */
    int             security_level;         /* Minimum access level required (0-5) */
    bool            visible;                /* Currently displayed */
} HMIDisplayNode;

/* ==========================================================================
 * L2 - Operator Security Model
 * ========================================================================== */

typedef enum {
    HMI_SEC_NONE       = 0,  /* No access */
    HMI_SEC_VIEW       = 1,  /* View only */
    HMI_SEC_OPERATOR   = 2,  /* Standard operator (ack alarms, change SP/Mode) */
    HMI_SEC_SUPERVISOR = 3,  /* Supervisor (change tuning, bypass alarms) */
    HMI_SEC_ENGINEER   = 4,  /* Engineer (configuration changes) */
    HMI_SEC_ADMIN      = 5   /* Administrator (security, system changes) */
} HMISecurityLevel;

typedef struct {
    char            user_name[32];
    char            password_hash[64];      /* SHA-256 hash of password */
    HMISecurityLevel level;
    char            area_scope[8];          /* Area access mask */
    bool            logged_in;
    time_t          login_time;
    time_t          session_timeout;        /* Auto-logout time */
    uint32_t        actions_logged;         /* Audit trail counter */
} HMIOperatorAccount;

/* ==========================================================================
 * L7 - Honeywell Experion Station Configuration
 * ========================================================================== */

/** Complete EST (Experion Station) configuration.
 *  Represents a single operator workstation in the Experion system. */
typedef struct {
    uint32_t        station_id;
    char            station_name[32];       /* e.g., "EST_CRACKER_OP1" */
    uint32_t        primary_esvt_id;        /* Primary server connection */
    uint32_t        backup_esvt_id;         /* Backup server connection */
    uint32_t        display_count;
    HMIDisplayNode  displays[256];
    HMIAlarmSummary alarm_summary;
    HMITrendDisplay active_trend;
    HMIFaceplate    active_faceplate;
    HMIOperatorAccount current_user;
    bool            connected;
    bool            alarm_horn_active;      /* Audible alarm */
    uint32_t        display_update_rate_ms; /* Display refresh rate */
    bool            isa101_compliant;       /* Uses ISA-101 color scheme */
} ExperionStation;

/* ==========================================================================
 * API - HMI / Station Functions
 * ========================================================================== */

/* Station lifecycle */
int  station_init(ExperionStation *stn, uint32_t id, const char *name);
int  station_connect(ExperionStation *stn, uint32_t esvt_id);
int  station_disconnect(ExperionStation *stn);

/* Alarm management */
int  alarm_add(HMIAlarmSummary *alarms, const HMIAlarmRecord *alarm);
int  alarm_acknowledge(HMIAlarmSummary *alarms, uint32_t alarm_id, const char *operator);
int  alarm_shelve(HMIAlarmSummary *alarms, uint32_t alarm_id, uint32_t duration_min);
int  alarm_unshelve(HMIAlarmSummary *alarms, uint32_t alarm_id);
int  alarm_get_highest_priority(const HMIAlarmSummary *alarms, HMIAlarmPriority *pri);
bool alarm_flood_detected(const HMIAlarmSummary *alarms);
int  alarm_clear_inactive(HMIAlarmSummary *alarms);

/* Faceplate */
int  faceplate_update(HMIFaceplate *fp, double pv, double sp, double op,
                      PIDMode mode, ExperionPointQuality pv_qual);
int  faceplate_set_sp(HMIFaceplate *fp, double new_sp, HMISecurityLevel user_level);
int  faceplate_set_mode(HMIFaceplate *fp, PIDMode new_mode, HMISecurityLevel user_level);
int  faceplate_set_op(HMIFaceplate *fp, double new_op, HMISecurityLevel user_level);
int  faceplate_isa101_color(HMIFaceplate *fp); /* Apply ISA-101 coloring rules */

/* Trend */
int  trend_init(HMITrendDisplay *trend, HMITrendMode mode, uint32_t span_sec, uint32_t interval_sec);
int  trend_add_pen(HMITrendDisplay *trend, const char *tag, const char *label, HMIISAColor color);
int  trend_add_sample(HMITrendPen *pen, time_t ts, double value, ExperionPointQuality quality);
int  trend_get_statistics(const HMITrendPen *pen, double *mean, double *stddev, double *min, double *max);

/* Operator Security */
int  operator_login(ExperionStation *stn, const char *username, const char *password);
int  operator_logout(ExperionStation *stn);
bool operator_has_access(const HMIOperatorAccount *op, HMISecurityLevel required_level);

/* Display navigation */
int  display_navigate_to(ExperionStation *stn, uint32_t display_id);
int  display_get_current(const ExperionStation *stn, HMIDisplayNode *display);
int  display_get_parent(const ExperionStation *stn, HMIDisplayNode *parent);

/* ISA-101 compliance verification */
bool isa101_verify_colors(const HMIFaceplate *fp);
bool isa101_verify_no_unnecessary_animation(void);

#ifdef __cplusplus
}
#endif

#endif /* HMIWEB_DISPLAY_H */
