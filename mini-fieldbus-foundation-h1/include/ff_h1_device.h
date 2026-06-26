/**
 * ff_h1_device.h ? Foundation Fieldbus H1 Device Types & Interoperability
 *
 * Defines H1 device classes (Basic, Link Master, Bridge), Device Description
 * (DD) language, CFF (Common File Format) capabilities, ITK (Interoperability
 * Test Kit) profiles, and device identity management.
 *
 * Course Mapping:
 *   MIT 2.171    ? Device-level embedded system design
 *   Stanford EE392 ? Industrial IoT device architecture
 *   ISA/IEC      ? FF-831 Device Description, FF-103 Common File Format
 *   Purdue ME575 ? Field device integration
 *
 * Knowledge Levels: L1 (Definitions), L2 (Core Concepts)
 */

#ifndef FF_H1_DEVICE_H
#define FF_H1_DEVICE_H

#include <stdint.h>
#include <stddef.h>
#include "ff_h1_application.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Device Classes
 *
 * Foundation Fieldbus defines three device classes:
 *
 *   Basic Device:      Minimal FF stack. Cannot become LAS.
 *   Link Master:       Full FF stack. Can become LAS. Required for redundancy.
 *   Bridge:            Connects H1 segment to HSE (High Speed Ethernet).
 *                      Translates between H1 and HSE protocols.
 *
 * A segment must have at least one Link Master (to act as LAS).
 * Redundancy: multiple LMs on a segment for LAS failover.
 * ============================================================================ */

typedef enum {
    FF_DEVICE_CLASS_BASIC       = 0x01,
    FF_DEVICE_CLASS_LINK_MASTER = 0x02,
    FF_DEVICE_CLASS_BRIDGE      = 0x03
} ff_device_class_t;

const char* ff_device_class_name(ff_device_class_t cls);


/* ============================================================================
 * L1: Device Identity
 *
 * Every FF H1 device has a globally unique 32-byte device ID, structured as:
 *   Bytes 0-5:   Manufacturer ID (OUI-like, assigned by Fieldbus Foundation)
 *   Bytes 6-19:  Device serial number (manufacturer-assigned, unique per manufacturer)
 *   Bytes 20-31: Device type/revision info
 *
 * The Device ID is burned into non-volatile memory at manufacturing.
 * ============================================================================ */

#define FF_DEVICE_ID_LENGTH  32

/** Device identity structure */
typedef struct {
    uint8_t  manufacturer_id[6];  /**< Fieldbus Foundation assigned manufacturer code */
    uint8_t  serial_number[14];   /**< Manufacturer-assigned unique serial */
    uint8_t  device_type[2];      /**< Device type code (e.g., 0x0001 = pressure transmitter) */
    uint8_t  device_revision;     /**< Hardware revision */
    uint8_t  dd_revision;         /**< Device Description revision */
    uint8_t  itk_version;         /**< ITK profile version (5=ITK 5, 6=ITK 6) */
    uint8_t  reserved[7];
} ff_device_identity_t;

/**
 * Parse device identity from raw 32-byte ID array.
 */
void ff_device_identity_parse(const uint8_t raw_id[FF_DEVICE_ID_LENGTH],
                               ff_device_identity_t *identity);

/**
 * Compare two device identities for equality.
 * @return 1 if identical, 0 otherwise
 */
int ff_device_identity_equal(const ff_device_identity_t *a,
                              const ff_device_identity_t *b);

/**
 * Format device ID as human-readable hex string.
 * Output format: "MMMMMM-SSSSSSSSSSSSSS-DDDDRR" (dash-separated groups)
 * Output buffer must be at least 40 bytes.
 */
int ff_device_id_format(const uint8_t device_id[FF_DEVICE_ID_LENGTH],
                        char *buf, size_t buf_size);


/* ============================================================================
 * L2: ITK (Interoperability Test Kit) Profiles
 *
 * ITK defines conformance classes that devices must pass to earn the
 * "FF Registered" mark. Each ITK version adds new mandatory features.
 *
 *   ITK 4.x:  Basic FF stack, AI/AO/DI/DO blocks, simple LAS
 *   ITK 5.x:  Enhanced blocks (PID, RA, etc.), multi-VCR, alerting
 *   ITK 6.x:  Advanced features (FOUNDATION Fieldbus Safety, block instantiation)
 *   ITK 7.x:  FDI (Field Device Integration) support, enhanced diagnostics
 * ============================================================================ */

typedef enum {
    FF_ITK_4_0  = 40,
    FF_ITK_4_5  = 45,
    FF_ITK_4_6  = 46,
    FF_ITK_5_0  = 50,
    FF_ITK_5_1  = 51,
    FF_ITK_5_2  = 52,
    FF_ITK_6_0  = 60,
    FF_ITK_6_1  = 61,
    FF_ITK_6_2  = 62,
    FF_ITK_7_0  = 70
} ff_itk_version_t;

/** ITK feature bit flags */
#define FF_ITK_FEATURE_LAS             0x0001 /**< Link Active Scheduler capable */
#define FF_ITK_FEATURE_MULTI_VCR       0x0002 /**< Multiple simultaneous VCRs */
#define FF_ITK_FEATURE_ALERTS          0x0004 /**< Alert and event reporting */
#define FF_ITK_FEATURE_TRENDS          0x0008 /**< Trend data capture */
#define FF_ITK_FEATURE_BLOCK_INSTANT   0x0010 /**< Block instantiation (dynamic blocks) */
#define FF_ITK_FEATURE_SAFETY          0x0020 /**< FF-Safety (IEC 61508) */
#define FF_ITK_FEATURE_FDI             0x0040 /**< FDI Device Package support */
#define FF_ITK_FEATURE_ADV_DIAG        0x0080 /**< Advanced diagnostics (NE 107) */
#define FF_ITK_FEATURE_TIME_SYNC       0x0100 /**< Enhanced time synchronization */
#define FF_ITK_FEATURE_BRIDGE          0x0200 /**< H1-HSE bridging */

/**
 * Get the set of mandatory features for a given ITK version.
 *
 * @param version  ITK version
 * @return bitmask of FF_ITK_FEATURE_* flags
 */
uint16_t ff_itk_mandatory_features(ff_itk_version_t version);

/**
 * Check if a device supports all mandatory features of a given ITK version.
 */
int ff_itk_check_compliance(uint16_t device_features, ff_itk_version_t version);


/* ============================================================================
 * L2: Device Description (DD) ? EDDL
 *
 * Device Description (DD) files describe a device's capabilities, parameters,
 * and methods to host systems. DD is written in Electronic Device Description
 * Language (EDDL, IEC 61804-3).
 *
 * DD contents:
 *   - Variable declarations (parameters with data types, ranges, units)
 *   - Menu structures (for host system displays)
 *   - Methods (step-by-step procedures like calibration, trim)
 *   - Help text (multi-language)
 *
 * A DD file is compiled into a binary DD tokenized format (.ffo file)
 * for efficient loading by host systems.
 * ============================================================================ */

/** DD variable attribute flags */
typedef enum {
    FF_DD_ATTR_READABLE    = 0x01,
    FF_DD_ATTR_WRITABLE    = 0x02,
    FF_DD_ATTR_STATIC      = 0x04, /**< Survives power cycle */
    FF_DD_ATTR_NONVOLATILE = 0x08  /**< Stored in NVRAM */
} ff_dd_attr_t;

/** DD variable definition */
typedef struct {
    uint16_t       var_index;     /**< DD variable index */
    const char     *var_name;     /**< Variable name */
    ff_data_type_t data_type;     /**< FF data type */
    uint16_t       data_size;     /**< Size in bytes */
    uint8_t        attributes;    /**< ff_dd_attr_t flags */
    double         min_value;     /**< Minimum valid value */
    double         max_value;     /**< Maximum valid value */
    const char     *units;        /**< Engineering units string */
    const char     *help_text;    /**< Help description */
} ff_dd_variable_t;

/** DD method ? a step-by-step procedure (e.g., calibration) */
typedef struct {
    const char     *method_name;
    const char     *description;
    uint8_t        total_steps;
    const char     **step_descriptions; /**< Array of step instruction strings */
    uint8_t        is_interactive;      /**< 1 if requires user confirmation at each step */
} ff_dd_method_t;

/**
 * Validate a DD variable's value against its declared limits.
 *
 * @return 1 if value is within [min_value, max_value], 0 otherwise
 */
int ff_dd_validate_value(const ff_dd_variable_t *var, double value);


/* ============================================================================
 * L2: CFF ? Common File Format (Capabilities File)
 *
 * The CFF file is a structured text file describing the device's communication
 * and function block capabilities. It is used by configuration tools to:
 *   - Know which blocks the device supports
 *   - Know VCR limits (how many BNU, QUB)
 *   - Determine resource consumption
 *   - Auto-generate tag assignments
 *
 * Reference: FF-103 "Common File Format Specification"
 * ============================================================================ */

/** CFF device capabilities */
typedef struct {
    ff_device_class_t device_class;
    ff_itk_version_t  itk_version;
    uint8_t           max_vcr_bnu;    /**< Max Publisher/Subscriber VCRs */
    uint8_t           max_vcr_qub;    /**< Max Client/Server VCRs */
    uint8_t           max_link_objects; /**< Max inter-block links */
    uint16_t          max_fb_count;    /**< Max function block instances */
    uint16_t          max_tb_count;    /**< Max transducer block instances */
    uint32_t          min_cycle_time_us; /**< Minimum execution cycle */
    uint32_t          max_cycle_time_us; /**< Maximum execution cycle */
    uint16_t          supported_fb_types; /**< Bitmask of ff_fb_type_t */
    uint16_t          supported_features; /**< Bitmask of FF_ITK_FEATURE_* */
    const char        *manufacturer_name;
    const char        *device_model;
    const char        *dd_revision;
} ff_cff_capabilities_t;

/**
 * Check if a CFF declares support for a specific function block type.
 */
int ff_cff_supports_fb(const ff_cff_capabilities_t *cff, ff_fb_type_t fb_type);

/**
 * Check if two CFF capabilities files are compatible for a function block
 * link (i.e., at least one common VCR type is supported by both devices).
 */
int ff_cff_link_compatible(const ff_cff_capabilities_t *a,
                            const ff_cff_capabilities_t *b);


/* ============================================================================
 * L1: Manufacturer Codes ? Well-Known FF Manufacturer IDs
 * ============================================================================ */

/** Look up manufacturer name by the first 3 bytes of manufacturer_id */
const char* ff_manufacturer_name(const uint8_t manufacturer_id[3]);

/** Check if manufacturer_id matches a known vendor */
int ff_manufacturer_is_known(const uint8_t manufacturer_id[3]);


#ifdef __cplusplus
}
#endif

#endif /* FF_H1_DEVICE_H */