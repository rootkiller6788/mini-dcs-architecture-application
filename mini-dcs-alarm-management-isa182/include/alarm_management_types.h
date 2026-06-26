/**
 * @file alarm_management_types.h
 * @brief ISA-18.2 / IEC 62682 Alarm Management — Core Type Definitions (L1)
 *
 * Knowledge Points (each typedef/struct = one independent ISA-18.2 concept):
 *   alarm_priority_t          — ISA-18.2 alarm priority classification (L1)
 *   alarm_type_t              — ISA-18.2 alarm type taxonomy (L1)
 *   alarm_state_t             — ISA-18.2 alarm state machine states (L1)
 *   alarm_lifecycle_phase_t   — ISA-18.2 alarm lifecycle 9 stages (L1)
 *   alarm_config_t            — Per-alarm point configuration per ISA-18.2 §7 (L1)
 *   alarm_event_t             — Alarm occurrence record per ISA-18.2 §11 (L1)
 *   alarm_shelve_t            — ISA-18.2 shelving record per §12.5 (L1)
 *   alarm_kpi_counts_t        — ISA-18.2 KPI raw counters (L1)
 *   operator_response_t       — Operator action record per §11 (L1)
 *   rationalization_record_t  — ISA-18.2 rationalization decision record (L1)
 *   alarm_class_t             — ISA-18.2 alarm class per Table 7-1 (L1)
 *   consequence_severity_t    — Consequence-of-inaction severity per §9.3 (L1)
 *   chattering_detection_t    — Chattering alarm detection buffer (L3)
 *   alarm_flood_detector_t    — Alarm flood statistics buffer (L3)
 *
 * References:
 *   - ANSI/ISA-18.2-2016, Management of Alarm Systems for the Process Industries
 *   - IEC 62682:2014, Management of alarm systems for the process industries
 *   - EEMUA Publication 191, Alarm Systems: A Guide to Design, Management and Procurement
 *   - ISA-TR18.2.4-2012, Enhanced and Advanced Alarm Methods
 *   - ISA-TR18.2.5-2012, Alarm System Monitoring, Assessment, and Audit
 */

#ifndef ALARM_MANAGEMENT_TYPES_H
#define ALARM_MANAGEMENT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/*============================================================================
 * ISA-18.2 System Constants
 *============================================================================*/

/** Maximum alarm points managed by a single alarm system per ISA-18.2 */
#define ISA18_MAX_ALARM_POINTS       20000U

/** Maximum rationalization records in master alarm database */
#define ISA18_MAX_RATIONALIZATION     5000U

/** Maximum concurrent active alarms (before flood declaration) */
#define ISA18_MAX_CONCURRENT_ALARMS   1000U

/** Maximum shelved alarms at any time */
#define ISA18_MAX_SHELVED_ALARMS       500U

/** Maximum alarm groups for suppression by plant state */
#define ISA18_MAX_ALARM_GROUPS         128U

/** Maximum plant states for state-based suppression */
#define ISA18_MAX_PLANT_STATES          64U

/** EEMUA 191 recommended maximum: 150 alarms/day/operator (steady state) */
#define ISA18_ACCEPTABLE_ALARMS_PER_DAY   150U

/** EEMUA 191: manageable peak rate <= 10 alarms in any 10-minute window */
#define ISA18_ACCEPTABLE_PEAK_10MIN        10U

/** EEMUA 191: over-manageable threshold > 30 alarms in 10 min */
#define ISA18_OVERMANAGEABLE_PEAK_10MIN    30U

/** ISA-18.2 chattering alarm threshold (>=3 transitions in 1 min) */
#define ISA18_CHATTER_THRESHOLD_COUNT       3U

/** ISA-18.2 chattering detection window in seconds */
#define ISA18_CHATTER_WINDOW_SEC           60U

/** Maximum alarm description length */
#define ISA18_MAX_DESCRIPTION_LEN          128U

/** Maximum alarm message text length per ISA-18.2 */
#define ISA18_MAX_MESSAGE_LEN              256U

/** Maximum consequence-of-inaction description */
#define ISA18_MAX_CONSEQUENCE_LEN          256U

/** Maximum corrective action description */
#define ISA18_MAX_ACTION_LEN               256U

/** Maximum operator response time per ISA-18.2 (in seconds) */
#define ISA18_MAX_RESPONSE_TIME_DEFAULT    600U

/** Maximum shelving duration per ISA-18.2 (default 12 hours) */
#define ISA18_MAX_SHELVE_DURATION_SEC     43200U

/** Alarm annunciator - maximum alarm list display items */
#define ISA18_ANNUNCIATOR_MAX_ITEMS         500U

/*============================================================================
 * L1 — Alarm Priority Classification (ISA-18.2 Table 9-1)
 *
 * Priority is determined by consequence-of-inaction severity
 * and time-to-respond urgency. ISA-18.2 defines a priority
 * matrix mapping (severity, urgency) -> priority.
 *============================================================================*/

typedef enum {
    ISA18_PRIORITY_CRITICAL   = 0,
    ISA18_PRIORITY_HIGH       = 1,
    ISA18_PRIORITY_MEDIUM     = 2,
    ISA18_PRIORITY_LOW        = 3
} isa18_alarm_priority_t;

/*============================================================================
 * L1 — Alarm Type Taxonomy (ISA-18.2)
 *
 * ISA-18.2 recognizes multiple alarm types. Each type has specific
 * design requirements for setpoint, deadband, and on-delay.
 *============================================================================*/

typedef enum {
    ISA18_TYPE_HIGH        = 0,
    ISA18_TYPE_LOW         = 1,
    ISA18_TYPE_HI_HI       = 2,
    ISA18_TYPE_LO_LO       = 3,
    ISA18_TYPE_DEVIATION   = 4,
    ISA18_TYPE_RATE_OF_CHANGE = 5,
    ISA18_TYPE_BAD_MEASUREMENT = 6,
    ISA18_TYPE_SYSTEM_DIAGNOSTIC = 7,
    ISA18_TYPE_DISCREPANCY = 8,
    ISA18_TYPE_STATE       = 9,
    ISA18_TYPE_SCADA_OFFLINE = 10,
    ISA18_TYPE_INHIBIT_VIOLATION = 11,
    ISA18_TYPE_MAINTENANCE_INDICATOR = 12
} isa18_alarm_type_t;

/*============================================================================
 * L1 — Alarm State Machine (ISA-18.2 Figure 11-1)
 *
 * ISA-18.2 defines a 5-state model for each alarm:
 *   NORMAL -> ACTIVE_UNACK -> ACTIVE_ACK -> RTN_UNACK -> CLEARED
 *============================================================================*/

typedef enum {
    ISA18_ALARM_STATE_NORMAL        = 0,
    ISA18_ALARM_STATE_ACTIVE_UNACK  = 1,
    ISA18_ALARM_STATE_ACTIVE_ACK    = 2,
    ISA18_ALARM_STATE_RTN_UNACK     = 3,
    ISA18_ALARM_STATE_CLEARED       = 4
} isa18_alarm_state_t;

/*============================================================================
 * L1 — ISA-18.2 Alarm Lifecycle (ISA-18.2 Figure 5-1)
 *
 * The 9-stage lifecycle is the central framework of ISA-18.2.
 * Stages A-I cover the entire life of an alarm.
 *============================================================================*/

typedef enum {
    ISA18_LIFECYCLE_PHILOSOPHY     = 0,
    ISA18_LIFECYCLE_IDENTIFICATION = 1,
    ISA18_LIFECYCLE_RATIONALIZATION = 2,
    ISA18_LIFECYCLE_DETAILED_DESIGN = 3,
    ISA18_LIFECYCLE_IMPLEMENTATION = 4,
    ISA18_LIFECYCLE_OPERATION      = 5,
    ISA18_LIFECYCLE_MAINTENANCE    = 6,
    ISA18_LIFECYCLE_MONITORING     = 7,
    ISA18_LIFECYCLE_AUDIT          = 8
} isa18_lifecycle_phase_t;

/*============================================================================
 * L1 — Alarm Class (ISA-18.2 Table 7-1)
 *
 * Determines whether an alarm stays in the DCS alarm system
 * or is reclassified as an alert/prompt.
 *============================================================================*/

typedef enum {
    ISA18_CLASS_ALARM      = 0,
    ISA18_CLASS_ALERT      = 1,
    ISA18_CLASS_PROMPT     = 2,
    ISA18_CLASS_NO_ALARM   = 3
} isa18_alarm_class_t;

/*============================================================================
 * L1 — Consequence Severity (ISA-18.2)
 *
 * Four severity levels used in the priority assignment matrix.
 *============================================================================*/

typedef enum {
    ISA18_SEVERITY_CRITICAL = 0,
    ISA18_SEVERITY_SEVERE   = 1,
    ISA18_SEVERITY_MAJOR    = 2,
    ISA18_SEVERITY_MODERATE = 3
} isa18_consequence_severity_t;

/*============================================================================
 * L1 — Time to Respond Urgency (ISA-18.2)
 *============================================================================*/

typedef enum {
    ISA18_URGENCY_IMMEDIATE   = 0,
    ISA18_URGENCY_PROMPT      = 1,
    ISA18_URGENCY_RAPID       = 2,
    ISA18_URGENCY_NON_URGENT  = 3
} isa18_urgency_t;

/*============================================================================
 * L1 — Per-Alarm Configuration Structure (ISA-18.2)
 *
 * Each alarm point in the master alarm database (MAD).
 *============================================================================*/

typedef struct {
    uint32_t            alarm_id;
    char                tag_name[64];
    char                description[ISA18_MAX_DESCRIPTION_LEN];
    isa18_alarm_type_t  alarm_type;
    isa18_alarm_priority_t priority;
    isa18_alarm_class_t alarm_class;

    /* Setpoint configuration */
    double              setpoint;
    double              deadband;
    double              rate_of_change_limit;
    double              deviation_limit;

    /* Timing configuration */
    uint32_t            on_delay_ms;
    uint32_t            off_delay_ms;

    /* Operator guidance */
    char                consequence[ISA18_MAX_CONSEQUENCE_LEN];
    char                corrective_action[ISA18_MAX_ACTION_LEN];
    uint32_t            time_to_respond_sec;

    /* Rationalization metadata */
    time_t              rationalization_date;
    char                rationalized_by[32];
    bool                is_rationalized;

    /* Suppression configuration */
    uint32_t            plant_state_mask;
    uint32_t            alarm_group_id;

    /* Management of Change */
    uint32_t            revision;
    time_t              last_modified;
    char                modified_by[32];

    /* Dynamic state (populated at runtime) */
    isa18_alarm_state_t current_state;
    time_t              activation_time;
    time_t              acknowledgement_time;
    time_t              return_to_normal_time;
    time_t              clearing_time;
    bool                is_shelved;
    time_t              shelve_expiry;
    bool                is_suppressed;
} isa18_alarm_config_t;

/*============================================================================
 * L1 — Alarm Event Record (ISA-18.2)
 *============================================================================*/

typedef struct {
    uint64_t            event_id;
    uint32_t            alarm_id;
    time_t              timestamp;
    isa18_alarm_state_t from_state;
    isa18_alarm_state_t to_state;
    double              process_value;
    double              setpoint;
    char                message[ISA18_MAX_MESSAGE_LEN];
    char                operator_id[32];
    bool                is_operator_action;
    uint32_t            priority_number;
} isa18_alarm_event_t;

/*============================================================================
 * L1 — Shelving Record (ISA-18.2)
 *============================================================================*/

typedef struct {
    uint32_t            shelve_id;
    uint32_t            alarm_id;
    time_t              shelve_start;
    time_t              shelve_end;
    char                reason[ISA18_MAX_DESCRIPTION_LEN];
    char                operator_id[32];
    char                approver_id[32];
    bool                is_approved;
    bool                is_active;
    uint32_t            extension_count;
    time_t              last_extension;
} isa18_alarm_shelve_t;

/*============================================================================
 * L1 — Rationalization Record (ISA-18.2)
 *============================================================================*/

typedef struct {
    uint32_t            record_id;
    uint32_t            alarm_id;
    char                tag_name[64];
    isa18_alarm_class_t alarm_class;
    isa18_alarm_priority_t priority;
    isa18_consequence_severity_t severity;
    isa18_urgency_t     urgency;
    double              max_safe_response_time_sec;
    bool                is_justified;
    char                justification[ISA18_MAX_DESCRIPTION_LEN];
    time_t              rationalization_date;
    char                team_members[10][32];
    uint32_t            team_count;
    uint32_t            max_alarms_per_day;
} isa18_rationalization_record_t;

/*============================================================================
 * L3 — Chattering Detection Buffer (ISA-18.2)
 *============================================================================*/

typedef struct {
    time_t              transition_times[ISA18_CHATTER_THRESHOLD_COUNT + 2];
    uint32_t            transition_count;
    bool                is_chattering;
    time_t              chattering_start;
    uint32_t            total_chattering_events;
} isa18_chattering_detector_t;

/*============================================================================
 * L3 — Alarm Flood Detector (ISA-18.2 / EEMUA 191)
 *============================================================================*/

typedef struct {
    uint32_t            alarms_in_window;
    time_t              window_start;
    bool                is_flood_active;
    time_t              flood_start_time;
    time_t              flood_end_time;
    uint32_t            peak_alarms_in_flood;
    uint32_t            total_flood_events;
    uint32_t            flood_duration_sec;
} isa18_alarm_flood_detector_t;

/*============================================================================
 * L1 — KPI Raw Counters (ISA-18.2 / EEMUA 191)
 *============================================================================*/

typedef struct {
    uint32_t            alarms_per_day;
    uint32_t            critical_per_day;
    uint32_t            high_per_day;
    uint32_t            medium_per_day;
    uint32_t            low_per_day;
    uint32_t            peak_10min_rate;
    uint32_t            peak_60min_rate;
    uint32_t            stale_24h_count;
    uint32_t            stale_7d_count;
    uint32_t            chattering_alarms;
    double              avg_response_time_sec;
    double              max_response_time_sec;
    uint32_t            unacknowledged_duration_max;
    uint32_t            active_alarm_count;
    uint32_t            shelved_alarm_count;
    uint32_t            suppressed_alarm_count;
    uint32_t            out_of_service_count;
    uint32_t            standing_alarms;
    uint32_t            fleeting_alarms;
    uint32_t            nuisance_alarms_count;
    double              percent_contrib_top10;
    double              alarm_rationalization_coverage;
    time_t              period_start;
    time_t              period_end;
    uint32_t            number_of_operators;
} isa18_kpi_counts_t;

/*============================================================================
 * L1 — Operator Response Record (ISA-18.2)
 *============================================================================*/

typedef struct {
    uint64_t            response_id;
    uint32_t            alarm_id;
    time_t              alarm_time;
    time_t              ack_time;
    time_t              response_complete_time;
    char                operator_id[32];
    char                action_taken[ISA18_MAX_ACTION_LEN];
    bool                response_within_time;
    double              response_time_sec;
} isa18_operator_response_t;

/*============================================================================
 * L3 — Alarm History Ring Buffer Entry
 *============================================================================*/

typedef struct {
    time_t                   timestamp;
    uint32_t                 alarm_id;
    isa18_alarm_priority_t   priority;
    isa18_alarm_state_t      transition_to;
} isa18_history_entry_t;

/*============================================================================
 * L3 — Top-N Frequent Alarm Counter
 *============================================================================*/

typedef struct {
    uint32_t            alarm_id;
    uint32_t            occurrence_count;
    char                tag_name[64];
} isa18_frequent_alarm_entry_t;

/*============================================================================
 * L3 — Master Alarm Database (MAD) Structure (ISA-18.2)
 *============================================================================*/

typedef struct {
    isa18_alarm_config_t alarms[ISA18_MAX_ALARM_POINTS];
    uint32_t            alarm_count;
    uint32_t            rationalized_count;
    time_t              database_creation_time;
    time_t              last_update_time;
    char                site_name[64];
    char                philosophy_doc_ref[128];
    uint32_t            revision;
} isa18_master_alarm_database_t;

/*============================================================================
 * L3 — Runtime Alarm System State
 *============================================================================*/

typedef struct {
    isa18_alarm_config_t      *configs;
    uint32_t                   total_alarms;
    uint32_t                   active_alarms;
    isa18_alarm_event_t        recent_events[ISA18_ANNUNCIATOR_MAX_ITEMS];
    uint32_t                   event_count;
    isa18_alarm_flood_detector_t flood_detector;
    isa18_kpi_counts_t         kpi_counts;
    isa18_alarm_shelve_t       shelved[ISA18_MAX_SHELVED_ALARMS];
    uint32_t                   shelved_count;
    time_t                     last_scan_time;
    bool                       flood_suppression_active;
} isa18_alarm_system_runtime_t;

/*============================================================================
 * Function Declarations (from alarm_management_types.c)
 *============================================================================*/

const char *isa18_priority_to_string(isa18_alarm_priority_t priority);
const char *isa18_alarm_type_to_string(isa18_alarm_type_t type);
const char *isa18_alarm_state_to_string(isa18_alarm_state_t state);
const char *isa18_lifecycle_phase_to_string(isa18_lifecycle_phase_t phase);
const char *isa18_alarm_class_to_string(isa18_alarm_class_t alarm_class);
const char *isa18_severity_to_string(isa18_consequence_severity_t severity);
const char *isa18_urgency_to_string(isa18_urgency_t urgency);

void isa18_alarm_config_init(isa18_alarm_config_t *alarm, uint32_t alarm_id,
                              const char *tag_name);
void isa18_alarm_config_set_high(isa18_alarm_config_t *alarm,
                                  double setpoint, double deadband);
void isa18_alarm_config_set_low(isa18_alarm_config_t *alarm,
                                 double setpoint, double deadband);
uint32_t isa18_alarm_config_validate(const isa18_alarm_config_t *alarm,
                                      char error_buf[][ISA18_MAX_MESSAGE_LEN],
                                      uint32_t max_errors);
void isa18_alarm_config_copy(const isa18_alarm_config_t *src,
                              isa18_alarm_config_t *dst);

bool isa18_alarm_type_is_analog(isa18_alarm_type_t type);
bool isa18_alarm_type_is_discrete(isa18_alarm_type_t type);
int isa18_priority_color_code(isa18_alarm_priority_t priority);
int isa18_compare_alarms_by_priority(const isa18_alarm_config_t *a,
                                      const isa18_alarm_config_t *b);

bool isa18_alarm_state_is_rtn(isa18_alarm_state_t state);
bool isa18_alarm_state_is_active(isa18_alarm_state_t state);
bool isa18_alarm_state_is_acknowledged(isa18_alarm_state_t state);
uint32_t isa18_priority_to_numeric(isa18_alarm_priority_t priority);

time_t isa18_compute_on_delay_expiry(time_t condition_true_time,
                                      uint32_t on_delay_ms);
time_t isa18_compute_off_delay_expiry(time_t condition_false_time,
                                       uint32_t off_delay_ms);

bool isa18_alarm_can_activate(const isa18_alarm_config_t *alarm);

#endif /* ALARM_MANAGEMENT_TYPES_H */