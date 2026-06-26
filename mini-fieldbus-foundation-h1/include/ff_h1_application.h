/**
 * ff_h1_application.h ? Foundation Fieldbus H1 Application Layer
 *
 * Implements Function Block Application Process (FBAP), Fieldbus Message
 * Specification (FMS), Object Dictionary, function block parameter access,
 * and inter-device function block linking.
 *
 * Course Mapping:
 *   MIT 2.171    ? Distributed control application architecture
 *   Stanford ENGR205 ? Process control function block design
 *   ISA/IEC      ? IEC 61158-5/6: Application Layer, FF-890 Function Block Spec
 *   Purdue ME575 ? Industrial control application structuring
 *
 * Knowledge Levels: L1 (Definitions), L2 (Core Concepts), L3 (Structures)
 */

#ifndef FF_H1_APPLICATION_H
#define FF_H1_APPLICATION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Function Block Types (FF-890 Specification)
 *
 * Standard function blocks defined by the Fieldbus Foundation.
 * Each block type has a specific processing algorithm and a standard
 * set of parameters accessible via the Object Dictionary.
 * ============================================================================ */

typedef enum {
    FF_FB_AI   = 0,   /**< Analog Input block ? reads transducer, scales to engineering units */
    FF_FB_AO   = 1,   /**< Analog Output block ? writes scaled value to transducer */
    FF_FB_DI   = 2,   /**< Discrete Input block */
    FF_FB_DO   = 3,   /**< Discrete Output block */
    FF_FB_PID  = 4,   /**< PID Control block ? ISA standard form */
    FF_FB_RA   = 5,   /**< Ratio block ? ratio control with gain + bias */
    FF_FB_CS   = 6,   /**< Control Selector block ? select/min/max/mid among inputs */
    FF_FB_ML   = 7,   /**< Manual Loader block ? manual/bumpless transfer to AUTO */
    FF_FB_BG   = 8,   /**< Bias/Gain block ? y = gain * x + bias */
    FF_FB_INT  = 9,   /**< Integrator block ? totalize flow, accumulation */
    FF_FB_AL   = 10,  /**< Analog Alarm block ? high/low/deviation alarming */
    FF_FB_IS   = 11,  /**< Input Selector block ? select first good, average */
    FF_FB_LL   = 12,  /**< Lead-Lag block ? dynamic compensation */
    FF_FB_DT   = 13,  /**< Deadtime block ? pure time delay */
    FF_FB_SP   = 14,  /**< Setpoint Ramp Generator */
    FF_FB_OS   = 15,  /**< Output Splitter ? split one output to two actuators */
    FF_FB_CHAR = 16,  /**< Signal Characterizer ? piecewise linear lookup */
    FF_FB_AR   = 17,  /**< Arithmetic block ? configurable arithmetic expression */
    FF_FB_COUNT = 18
} ff_fb_type_t;

/** Get the standard name string for a function block type */
const char* ff_fb_type_name(ff_fb_type_t type);

/** Get the number of standard parameters for a function block type */
int ff_fb_type_param_count(ff_fb_type_t type);


/* ============================================================================
 * L1: Block Modes (MODE_BLK parameter ? FF-890)
 *
 * Every function block has a MODE_BLK parameter that controls execution:
 *   Target  ? the mode the user wants
 *   Actual  ? the mode the block can actually achieve
 *   Permitted ? the modes allowed for this block
 *   Normal   ? the normal operating mode
 * ============================================================================ */

typedef enum {
    FF_MODE_OOS  = 0x80, /**< Out of Service ? block not executing */
    FF_MODE_IMAN = 0x10, /**< Initialization Manual ? cascade initialization */
    FF_MODE_LO   = 0x08, /**< Local Override ? fault/override by transducer */
    FF_MODE_MAN  = 0x04, /**< Manual ? operator sets output directly */
    FF_MODE_AUTO = 0x02, /**< Automatic ? block algorithm runs, setpoint from operator */
    FF_MODE_CAS  = 0x01, /**< Cascade ? setpoint from upstream block via CAS_IN */
    FF_MODE_RCAS = 0x40, /**< Remote Cascade ? setpoint from host application */
    FF_MODE_ROUT = 0x20  /**< Remote Output ? output from host application */
} ff_block_mode_t;

/** MODE_BLK structure */
typedef struct {
    ff_block_mode_t target;
    ff_block_mode_t actual;
    ff_block_mode_t permitted;
    ff_block_mode_t normal;
} ff_mode_blk_t;

/** Check if a mode transition is allowed (actual  target) */
int ff_mode_transition_allowed(ff_block_mode_t from, ff_block_mode_t to,
                                ff_block_mode_t permitted);

/** Get next valid actual mode given target and current conditions */
ff_block_mode_t ff_mode_determine_actual(ff_block_mode_t target,
                                          ff_block_mode_t permitted,
                                          int cascade_ready,
                                          int fault_active);


/* ============================================================================
 * L1: Standard Function Block Parameters
 *
 * Every function block has a standard parameter set accessible via FMS
 * Read/Write services. Parameters are identified by relative index (1-based
 * per FF specification).
 * ============================================================================ */

/** Common parameters present in all function blocks (indices 1-7) */
#define FF_PARAM_ST_REV        1   /**< Static Revision (unsigned16) */
#define FF_PARAM_TAG_DESC      2   /**< Tag Description (octet string) */
#define FF_PARAM_STRATEGY      3   /**< Strategy (unsigned16) */
#define FF_PARAM_ALERT_KEY     4   /**< Alert Key (unsigned8) */
#define FF_PARAM_MODE_BLK      5   /**< Mode Block (MODE_BLK structure) */
#define FF_PARAM_BLOCK_ERR     6   /**< Block Error (bit string) */
#define FF_PARAM_RS_STATE      7   /**< Resource State (unsigned8, Resource Block only) */

/** Block Error bit flags */
#define FF_BLKERR_OTHER           0x0001
#define FF_BLKERR_BLOCK_CONFIG    0x0002
#define FF_BLKERR_LINK_CONFIG     0x0004
#define FF_BLKERR_SIMULATE_ACTIVE 0x0008
#define FF_BLKERR_LOCAL_OVERRIDE  0x0010
#define FF_BLKERR_DEVICE_FAULT    0x0020
#define FF_BLKERR_DEVICE_MAINT    0x0040
#define FF_BLKERR_INPUT_FAILURE   0x0080
#define FF_BLKERR_OUTPUT_FAILURE  0x0100
#define FF_BLKERR_MEMORY_FAILURE  0x0200
#define FF_BLKERR_LOST_STATIC     0x0400
#define FF_BLKERR_LOST_NV         0x0800
#define FF_BLKERR_READBACK_FAIL   0x1000
#define FF_BLKERR_MAINT_NEEDED    0x2000
#define FF_BLKERR_POWER_UP        0x4000
#define FF_BLKERR_OOS             0x8000


/* ============================================================================
 * L2: Function Block Application Process (FBAP) Model
 *
 * The FBAP consists of:
 *   - Resource Block (1 per device): device hardware/software characteristics
 *   - Transducer Blocks (1+ per device): interface to physical I/O
 *   - Function Blocks (0+ per device): control/calculation algorithms
 *
 * Blocks are linked via Link Objects, which map an output parameter of one block
 * to an input parameter of another block (possibly in a different device).
 * ============================================================================ */

/** Maximum number of blocks per device */
#define FF_MAX_BLOCKS_PER_DEVICE   32

/** Generic parameter value ? union for all FF data types */
typedef union {
    uint8_t   u8;
    int8_t    i8;
    uint16_t  u16;
    int16_t   i16;
    uint32_t  u32;
    int32_t   i32;
    float     f;
    double    d;
    uint8_t   str[32];  /**< Octet string (padded) */
    uint8_t   bs[4];    /**< Bit string (up to 32 bits) */
    uint32_t  time_val; /**< Time value (seconds since epoch) */
} ff_param_value_t;

/** FF data type enumeration */
typedef enum {
    FF_DTYPE_BOOL      = 1,
    FF_DTYPE_INT8      = 2,
    FF_DTYPE_INT16     = 3,
    FF_DTYPE_INT32     = 4,
    FF_DTYPE_UINT8     = 5,
    FF_DTYPE_UINT16    = 6,
    FF_DTYPE_UINT32    = 7,
    FF_DTYPE_FLOAT     = 8,
    FF_DTYPE_DOUBLE    = 9,
    FF_DTYPE_OCTET_STR = 10,
    FF_DTYPE_BIT_STR   = 11,
    FF_DTYPE_TIME      = 12,
    FF_DTYPE_MODE_BLK  = 13
} ff_data_type_t;

/** Single parameter descriptor */
typedef struct {
    uint16_t       index;        /**< 1-based parameter index */
    ff_data_type_t type;
    uint16_t       size;         /**< Size in bytes */
    uint8_t        storage;      /**< 0=dynamic, 1=static, 2=non-volatile */
    uint8_t        read_access;  /**< 1 if readable */
    uint8_t        write_access; /**< 1 if writable */
    ff_param_value_t value;      /**< Current value */
} ff_parameter_t;

/** Function block instance */
typedef struct {
    ff_fb_type_t   type;
    uint16_t       block_tag;    /**< 16-bit block tag identifier */
    ff_parameter_t *params;
    size_t         param_count;
    ff_mode_blk_t  mode;
    uint16_t       block_err;
    uint32_t       exec_time_us; /**< Execution time in microseconds */
    uint8_t        is_executing;
} ff_function_block_t;

/** Resource block ? device-level attributes */
typedef struct {
    uint32_t       device_id;
    uint8_t        manufacturer_id[4];
    uint8_t        device_type[2];
    uint8_t        device_rev;
    uint8_t        dd_rev;
    uint8_t        itk_version;
    uint32_t       cycle_time_us;
    uint32_t       cycle_counter;
    uint8_t        rs_state;
    uint16_t       rs_error;
    uint32_t       free_memory;
    uint32_t       free_time_us;
} ff_resource_block_t;

/** Transducer block ? I/O interface */
typedef struct {
    uint16_t       transducer_type;
    uint16_t       transducer_error;
    uint8_t        xd_state;
    double         primary_value;
    double         secondary_value;
    uint8_t        sensor_type;
    uint8_t        calibration_status;
    double         lo_range;
    double         hi_range;
    char           units[16];
} ff_transducer_block_t;

/** Complete FBAP for one device */
typedef struct {
    ff_resource_block_t    resource;
    ff_transducer_block_t *transducers;
    size_t                 transducer_count;
    ff_function_block_t   *function_blocks;
    size_t                 fb_count;
} ff_fbap_device_t;


/* ============================================================================
 * L2: Link Object ? Inter-Block Communication
 *
 * Link Objects connect function block outputs to inputs. Each link specifies:
 *   - Source: (device_addr, block_tag, param_index)
 *   - Destination: (device_addr, block_tag, param_index)
 *
 * Links can be local (same device) or remote (across the H1 bus via BNU VCR).
 * ============================================================================ */

typedef struct {
    uint8_t  src_device;      /**< Source device DL-address (0 = this device) */
    uint16_t src_block_tag;
    uint16_t src_param_index;
    uint8_t  dst_device;      /**< Destination device DL-address (0 = this device) */
    uint16_t dst_block_tag;
    uint16_t dst_param_index;
    uint8_t  link_type;       /**< 0=local, 1=BNU remote, 2=QUB remote */
    uint32_t stale_limit_us;  /**< Maximum age of linked data before fault */
} ff_link_object_t;

/** Validate link object configuration ? checks for invalid self-loops, etc. */
int ff_link_validate(const ff_link_object_t *link);


/* ============================================================================
 * L3: FMS ? Fieldbus Message Specification Services
 *
 * FMS defines the application-layer services for reading/writing device
 * parameters, managing VCRs, and handling events.
 *
 * Key services (confirmed = request + response):
 *   Read       ? read a parameter by index (confirmed)
 *   Write      ? write a parameter by index (confirmed)
 *   Information Report ? unconfirmed data push
 *   Event Notification ? alarm/event reporting
 * ============================================================================ */

/** FMS service types */
typedef enum {
    FF_FMS_READ               = 0,
    FF_FMS_WRITE              = 1,
    FF_FMS_INFO_REPORT        = 2,
    FF_FMS_EVENT_NOTIFY       = 3,
    FF_FMS_ACKNOWLEDGE_EVENT  = 4,
    FF_FMS_GET_OD             = 5,  /**< Get Object Dictionary listing */
    FF_FMS_INITIATE           = 6,  /**< Establish communication */
    FF_FMS_ABORT              = 7   /**< Abort service */
} ff_fms_service_t;

/** FMS PDU header */
typedef struct {
    ff_fms_service_t service;
    uint16_t         invoke_id;  /**< Matches request with response */
    uint16_t         od_index;   /**< Object Dictionary index */
    uint8_t          sub_index;  /**< Sub-index within OD entry */
} ff_fms_header_t;

/** FMS response status codes */
typedef enum {
    FF_FMS_OK              = 0,
    FF_FMS_ERR_ACCESS      = 1,   /**< Access denied */
    FF_FMS_ERR_RANGE       = 2,   /**< Index out of range */
    FF_FMS_ERR_TYPE        = 3,   /**< Type conflict */
    FF_FMS_ERR_HARDWARE    = 4,   /**< Hardware fault */
    FF_FMS_ERR_BUSY        = 5,   /**< Resource busy */
    FF_FMS_ERR_NO_OBJECT   = 6,   /**< Object does not exist */
    FF_FMS_ERR_READ_ONLY   = 7    /**< Object is read-only */
} ff_fms_status_t;


/* ============================================================================
 * L3: Object Dictionary (OD)
 *
 * The Object Dictionary is the structured catalog of all accessible objects
 * in a device. Each OD entry maps a logical index to a physical parameter
 * or block.
 *
 * OD structure:
 *   Index 0:         OD Header (list of all entries)
 *   Index 1-255:     Data Type definitions
 *   Index 256-511:   Static data types
 *   Index 512-1023:  Dynamic list of block parameters
 *   Index 1024-16383:FB parameters by (block_tag, param_index) mapping
 * ============================================================================ */

/** Object Dictionary entry descriptor */
typedef struct {
    uint16_t       od_index;
    ff_data_type_t data_type;
    uint8_t        object_code;   /**< 1=Simple Var, 2=Array, 7=Record, 8=Event, 9=Domain */
    uint8_t        local_addr;    /**< Internal memory address */
    uint16_t       size;
    uint8_t        read_access;   /**< 1 if readable */
    uint8_t        write_access;  /**< 1 if writable */
    const char     *description;
} ff_od_entry_t;

/** Object Dictionary ? full device OD */
typedef struct {
    ff_od_entry_t *entries;
    size_t         count;
    size_t         capacity;
} ff_object_dictionary_t;

/**
 * Look up OD entry by index.
 * @return pointer to entry, or NULL if not found
 */
const ff_od_entry_t* ff_od_lookup(const ff_object_dictionary_t *od, uint16_t index);

/**
 * Read a parameter value from device FBAP via OD.
 * @return 0 on success, FMS error code on failure
 */
int ff_fms_read_parameter(const ff_fbap_device_t *device,
                          const ff_object_dictionary_t *od,
                          uint16_t od_index, ff_param_value_t *value);

/**
 * Write a parameter value to device FBAP via OD.
 * @return 0 on success, FMS error code on failure
 */
int ff_fms_write_parameter(ff_fbap_device_t *device,
                           const ff_object_dictionary_t *od,
                           uint16_t od_index, const ff_param_value_t *value);


/* ============================================================================
 * L2: Function Block Execution Engine
 *
 * Each function block contains an algorithm that executes once per
 * macrocycle (or at a configured rate). The execution engine:
 *   1. Reads all input parameters (from links or local values)
 *   2. Executes the block algorithm
 *   3. Writes output parameters
 *   4. Updates MODE_BLK.Actual, BLOCK_ERR
 *
 * Reference: FF-890 5.2 "Block Algorithm"
 * ============================================================================ */

/**
 * Execute a single function block.
 *
 * @param block  the function block to execute
 * @param device parent device (for resource state, transducer values)
 * @return 0 on success, -1 if block is OOS or faulted
 */
int ff_fb_execute(ff_function_block_t *block, const ff_fbap_device_t *device);

/**
 * Execute the PID function block algorithm (ISA standard form).
 *
 *   CO = Kp * (E + (1/Ti) E?dt + Td ? dPV/dt) + BIAS
 *
 * where CO = control output, E = SP - PV (error).
 * Uses incremental (velocity) form for bumpless transfer.
 *
 * @param block  PID block with parameters set
 * @param dt_sec time step in seconds
 * @return 0 on success
 *
 * Reference: FF-890 8.3 "PID Block Algorithm", Astrom & Hagglund (1995)
 */
int ff_fb_pid_algorithm(ff_function_block_t *block, double dt_sec);

/**
 * Execute the Analog Input block: read transducer, linear scaling, filtering,
 * square root extraction (if configured), and alarm limit checking.
 *
 * @param block  AI block
 * @param transducer  source transducer block
 * @return 0 on success
 */
int ff_fb_ai_algorithm(ff_function_block_t *block,
                        const ff_transducer_block_t *transducer);

/**
 * Execute the Ratio block: OUT = GAIN * IN * RATIO + BIAS
 */
int ff_fb_ratio_algorithm(ff_function_block_t *block, double dt_sec);


#ifdef __cplusplus
}
#endif

#endif /* FF_H1_APPLICATION_H */