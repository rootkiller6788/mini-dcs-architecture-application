/**
 * @file c300_controller.h
 * @brief C300 Controller - Scan Cycle, I/O Processing, and Execution Model
 *
 * L1: C300 controller definition, I/O module types, scan phases
 * L2: Deterministic execution cycle, I/O forcing, peer-to-peer communication
 * L3: Engineering structure - C300 memory layout, I/O mapping, scan scheduling
 * L4: IEC 61131-3 runtime compliance, SIL2 capability with Safety Manager
 *
 * Reference: Honeywell C300 Controller Specification (EP03-500)
 * Course: RWTH Aachen PLC/SCADA Engineering, CMU 18-771 Linear Systems
 */

#ifndef C300_CONTROLLER_H
#define C300_CONTROLLER_H

#include "experion_system.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1 - C300 Hardware Definitions
 * ========================================================================== */

#define C300_MAX_IOSLOTS          16
#define C300_MAX_CHANNELS_PER_SLOT 32
#define C300_MAX_CM_POINTS        800
#define C300_MAX_PEER_CONNECTIONS 200
#define C300_EXEC_PERIOD_MS_MIN   5
#define C300_EXEC_PERIOD_MS_MAX   2000
#define C300_MEMORY_SIZE_KB       16384

/** C300 I/O module type enumeration.
 *  Each module type maps to a specific Honeywell Series-C I/O card. */
typedef enum {
    C3IO_AI_16CH_420MA    = 0x01,  /* 16-ch analog input, 4-20mA, HART */
    C3IO_AI_8CH_TC        = 0x02,  /* 8-ch thermocouple input */
    C3IO_AI_8CH_RTD       = 0x03,  /* 8-ch RTD input, 3-wire */
    C3IO_AO_8CH_420MA     = 0x11,  /* 8-ch analog output, 4-20mA */
    C3IO_AO_4CH_PULSE     = 0x12,  /* 4-ch pulse output */
    C3IO_DI_32CH_24VDC    = 0x21,  /* 32-ch digital input, 24VDC */
    C3IO_DI_16CH_SOE      = 0x22,  /* 16-ch Sequence of Events, 1ms res */
    C3IO_DO_32CH_RELAY    = 0x31,  /* 32-ch relay output */
    C3IO_DO_16CH_SSR      = 0x32,  /* 16-ch solid-state relay output */
    C3IO_MIXED_IO         = 0x40,  /* Universal I/O module (software config) */
    C3IO_HART_MUX         = 0x50,  /* HART multiplexer module */
    C3IO_PROFIBUS_DP      = 0x60,  /* PROFIBUS DP gateway module */
    C3IO_FOUNDATION_FB    = 0x70   /* Foundation Fieldbus H1 module */
} C300IOModuleType;

/** Single I/O channel configuration (L1: Definition).
 *  Each physical channel holds signal conditioning, alarm limits,
 *  and quality information. */
typedef struct {
    uint32_t        channel_number;
    char            tag[24];           /* EXN_TAG_NAME_MAX_LEN */
    ExperionEURange eu_range;
    double          raw_value;
    double          filtered_value;
    ExperionPointQuality quality;
    bool            alarm_enabled;
    double          alarm_lo;
    double          alarm_lolo;
    double          alarm_hi;
    double          alarm_hihi;
    double          roc_limit;
    uint8_t         filter_time;
    bool            scan_disabled;
} C300IOChannel;

/** C300 I/O card/slot descriptor. */
typedef struct {
    uint8_t         slot_number;
    C300IOModuleType module_type;
    uint8_t         channel_count;
    bool            present;
    bool            healthy;
    uint32_t        scan_interval_ms;
    C300IOChannel   channels[32];  /* C300_MAX_CHANNELS_PER_SLOT */
} C300IOSlot;

/* ==========================================================================
 * L2 - C300 Scan Cycle Phases (Core Concept)
 * ========================================================================== */

/**
 * C300 controller executes a deterministic 5-phase scan cycle every
 * configured execution period (typically 50ms, 100ms, 250ms, 500ms, or 1s).
 *
 * Phase 0: Input Scan    - Read all I/O channels, HART data, diagnostic words
 * Phase 1: Input Process - Filter, scale, check limits, detect alarms
 * Phase 2: Logic Solve   - Execute control blocks, SCM, logic
 * Phase 3: Output Process- Apply output limits, clamp, ramp
 * Phase 4: Output Scan   - Write to physical I/O, update peer data
 */
typedef enum {
    C3PHASE_INPUT_SCAN     = 0,
    C3PHASE_INPUT_PROCESS  = 1,
    C3PHASE_LOGIC_SOLVE    = 2,
    C3PHASE_OUTPUT_PROCESS = 3,
    C3PHASE_OUTPUT_SCAN    = 4,
    C3PHASE_IDLE           = 5
} C300ScanPhase;

/** Timing metrics for a single scan cycle - used for load monitoring. */
typedef struct {
    uint64_t    cycle_number;
    uint32_t    execution_period_ms;
    uint32_t    phase_duration_us[6];
    uint32_t    total_scan_us;
    uint32_t    idle_time_us;
    uint32_t    max_overrun_us;
    uint32_t    overrun_count;
    bool        overrun_occurred;
} C300CycleMetrics;

/* ==========================================================================
 * L2 - Peer-to-Peer Communication
 * ========================================================================== */

/** C300 Peer connection - direct controller-to-controller data sharing
 *  via the Control Firewall over FTE. No server mediation needed.
 *  This is a key DCS advantage over PLC architectures. */
typedef struct {
    uint32_t    source_controller_id;
    uint32_t    dest_controller_id;
    uint32_t    point_count;
    uint32_t    update_period_ms;
    uint32_t    stale_timeout_ms;
    bool        active;
} C300PeerConnection;

/* ==========================================================================
 * L3 - C300 Controller Runtime Structure
 * ========================================================================== */

typedef struct {
    uint32_t            controller_id;
    char                controller_name[32];
    bool                redundant;
    uint32_t            partner_id;
    uint32_t            execution_period_ms;
    uint32_t            fast_period_ms;
    uint32_t            slow_period_ms;
    C300ScanPhase       current_phase;
    C300CycleMetrics    metrics;
    C300IOSlot          io_slots[16];      /* C300_MAX_IOSLOTS */
    uint32_t            cm_point_count;
    uint32_t            peer_count;
    C300PeerConnection  peers[200];         /* C300_MAX_PEER_CONNECTIONS */
    uint32_t            uptime_cycles;
    bool                online;
    ExperionSystemMode  mode;
} C300Controller;

/* I/O forcing state */
typedef struct {
    bool        input_force_enabled;
    bool        output_force_enabled;
    uint32_t    forced_input_count;
    uint32_t    forced_output_count;
} C300ForceState;

/* ==========================================================================
 * API - C300 Controller Functions
 * ========================================================================== */

int  c300_init(C300Controller *ctrl, uint32_t id, const char *name, uint32_t period_ms);
int  c300_configure_io_slot(C300Controller *ctrl, uint8_t slot_num, C300IOModuleType mod_type);
int  c300_configure_channel(C300Controller *ctrl, uint8_t slot, uint8_t chan,
                            const char *tag, double range_lo, double range_hi, const char *units);
uint64_t c300_execute_scan(C300Controller *ctrl);
int  c300_read_analog_input(const C300Controller *ctrl, uint8_t slot, uint8_t chan, double *value);
int  c300_write_analog_output(C300Controller *ctrl, uint8_t slot, uint8_t chan, double value);
int  c300_apply_filter(C300Controller *ctrl, uint8_t slot, uint8_t chan, double filter_time_sec);
int  c300_evaluate_alarms(C300Controller *ctrl, uint8_t slot, uint8_t chan);
int  c300_add_peer_subscription(C300Controller *ctrl, uint32_t source_id, uint32_t point_count, uint32_t period_ms);
int  c300_get_cycle_metrics(const C300Controller *ctrl, C300CycleMetrics *metrics);
int  c300_set_force_mode(C300Controller *ctrl, bool force_on);
double c300_scale_to_eu(double raw, double raw_lo, double raw_hi, double eu_lo, double eu_hi);

#ifdef __cplusplus
}
#endif

#endif /* C300_CONTROLLER_H */
