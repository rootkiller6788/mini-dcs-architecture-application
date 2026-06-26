/**
 * ff_h1_device.c ? Foundation Fieldbus H1 Device Types Implementation
 *
 * Implements device identity parsing/comparison/formatting, ITK profile
 * compliance checking, device class naming, DD variable validation,
 * CFF capability operations, and manufacturer code lookups.
 *
 * Knowledge Levels: L1, L2
 */

#include "ff_h1_device.h"
#include "ff_h1_application.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ============================================================================
 * L1: Device Class Names
 * ============================================================================ */

const char* ff_device_class_name(ff_device_class_t cls) {
    switch (cls) {
        case FF_DEVICE_CLASS_BASIC:       return "Basic Device";
        case FF_DEVICE_CLASS_LINK_MASTER: return "Link Master";
        case FF_DEVICE_CLASS_BRIDGE:      return "Bridge";
        default:                          return "Unknown";
    }
}


/* ============================================================================
 * L1: Device Identity Operations
 * ============================================================================ */

void ff_device_identity_parse(const uint8_t raw_id[FF_DEVICE_ID_LENGTH],
                               ff_device_identity_t *identity) {
    if (!raw_id || !identity) return;
    memset(identity, 0, sizeof(*identity));

    memcpy(identity->manufacturer_id, &raw_id[0], 6);
    memcpy(identity->serial_number,  &raw_id[6], 14);
    identity->device_type[0] = raw_id[20];
    identity->device_type[1] = raw_id[21];
    identity->device_revision = raw_id[22];
    identity->dd_revision     = raw_id[23];
    identity->itk_version     = raw_id[24];
    memcpy(identity->reserved, &raw_id[25], 7);
}

int ff_device_identity_equal(const ff_device_identity_t *a,
                              const ff_device_identity_t *b) {
    if (!a || !b) return 0;

    if (memcmp(a->manufacturer_id, b->manufacturer_id, 6) != 0) return 0;
    if (memcmp(a->serial_number, b->serial_number, 14) != 0) return 0;
    if (a->device_type[0] != b->device_type[0]) return 0;
    if (a->device_type[1] != b->device_type[1]) return 0;
    if (a->device_revision != b->device_revision) return 0;
    if (a->dd_revision != b->dd_revision) return 0;

    return 1;
}

int ff_device_id_format(const uint8_t device_id[FF_DEVICE_ID_LENGTH],
                        char *buf, size_t buf_size) {
    if (!device_id || !buf || buf_size < 40) return -1;

    /* Format: MMMMMM-SSSSSSSSSSSSSS-DDDDRR
     * M = manufacturer (6 hex bytes = 12 chars)
     * S = serial (14 hex bytes = 28 chars)
     * D = device type (2 bytes = 4 chars)
     * R = revision (2 bytes = 4 chars)
     */

    /* Manufacturer: bytes 0-5 */
    char mfg[13];
    snprintf(mfg, sizeof(mfg), "%02X%02X%02X%02X%02X%02X",
             device_id[0], device_id[1], device_id[2],
             device_id[3], device_id[4], device_id[5]);

    /* Serial: bytes 6-19 */
    char ser[29];
    snprintf(ser, sizeof(ser),
             "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
             device_id[6],  device_id[7],  device_id[8],  device_id[9],
             device_id[10], device_id[11], device_id[12], device_id[13],
             device_id[14], device_id[15], device_id[16], device_id[17],
             device_id[18], device_id[19]);

    /* Device type + revision: bytes 20-24 */
    char dev[9];
    snprintf(dev, sizeof(dev), "%02X%02X%02X%02X",
             device_id[20], device_id[21],
             device_id[22], device_id[23]);

    snprintf(buf, buf_size, "%s-%s-%s", mfg, ser, dev);

    return 0;
}


/* ============================================================================
 * L2: ITK Compliance Checking
 *
 * Each ITK version mandates a set of features. A device is compliant
 * if it supports ALL mandatory features for that ITK version.
 *
 * Feature requirements by ITK version:
 *   ITK 4.0: Basic FF stack (LAS not required for basic devices)
 *   ITK 5.0: + Multi-VCR, Alerts, Time Sync
 *   ITK 6.0: + Trends, Block Instantiation, Advanced Diagnostics
 *   ITK 7.0: + FDI, Safety (for safety devices)
 * ============================================================================ */

uint16_t ff_itk_mandatory_features(ff_itk_version_t version) {
    switch (version) {
        case FF_ITK_4_0:
        case FF_ITK_4_5:
        case FF_ITK_4_6:
            return FF_ITK_FEATURE_TIME_SYNC;

        case FF_ITK_5_0:
        case FF_ITK_5_1:
        case FF_ITK_5_2:
            return FF_ITK_FEATURE_TIME_SYNC |
                   FF_ITK_FEATURE_MULTI_VCR |
                   FF_ITK_FEATURE_ALERTS;

        case FF_ITK_6_0:
        case FF_ITK_6_1:
        case FF_ITK_6_2:
            return FF_ITK_FEATURE_TIME_SYNC |
                   FF_ITK_FEATURE_MULTI_VCR |
                   FF_ITK_FEATURE_ALERTS |
                   FF_ITK_FEATURE_TRENDS |
                   FF_ITK_FEATURE_BLOCK_INSTANT |
                   FF_ITK_FEATURE_ADV_DIAG;

        case FF_ITK_7_0:
            return FF_ITK_FEATURE_TIME_SYNC |
                   FF_ITK_FEATURE_MULTI_VCR |
                   FF_ITK_FEATURE_ALERTS |
                   FF_ITK_FEATURE_TRENDS |
                   FF_ITK_FEATURE_BLOCK_INSTANT |
                   FF_ITK_FEATURE_ADV_DIAG |
                   FF_ITK_FEATURE_FDI;

        default:
            return 0;
    }
}

int ff_itk_check_compliance(uint16_t device_features, ff_itk_version_t version) {
    uint16_t mandatory = ff_itk_mandatory_features(version);
    /* Device must have ALL mandatory bits set */
    return ((device_features & mandatory) == mandatory) ? 1 : 0;
}


/* ============================================================================
 * L2: DD Variable Value Validation
 *
 * Validates that a value falls within the declared range of a DD variable.
 * This is used by host systems to prevent out-of-range writes and by
 * configuration tools to validate user input.
 * ============================================================================ */

int ff_dd_validate_value(const ff_dd_variable_t *var, double value) {
    if (!var) return 0;
    return (value >= var->min_value && value <= var->max_value) ? 1 : 0;
}


/* ============================================================================
 * L2: CFF Capability Operations
 * ============================================================================ */

int ff_cff_supports_fb(const ff_cff_capabilities_t *cff, ff_fb_type_t fb_type) {
    if (!cff) return 0;
    if (fb_type >= FF_FB_COUNT) return 0;
    return (cff->supported_fb_types & (1 << (uint16_t)fb_type)) ? 1 : 0;
}

int ff_cff_link_compatible(const ff_cff_capabilities_t *a,
                            const ff_cff_capabilities_t *b) {
    if (!a || !b) return 0;

    /* Two devices are link-compatible if they share at least one VCR type.
     * BNU (Publisher/Subscriber) is the most common.
     * QUB (Client/Server) is also widely supported.
     *
     * Compatibility check:
     *   - Both must have at least 1 BNU VCR capacity AND
     *   - At least one device supports publishing and the other subscribing
     *
     * Simplified check: both have > 0 BNU VCRs available.
     */
    if (a->max_vcr_bnu > 0 && b->max_vcr_bnu > 0) {
        return 1;
    }

    /* If no BNU, check QUB */
    if (a->max_vcr_qub > 0 && b->max_vcr_qub > 0) {
        return 1;
    }

    return 0;
}


/* ============================================================================
 * L1: Manufacturer Code Lookup
 *
 * Foundation Fieldbus assigns 3-byte manufacturer IDs.
 * Well-known IDs included below for diagnostic and display purposes.
 * ============================================================================ */

typedef struct {
    uint8_t     id[3];
    const char *name;
} ff_mfg_entry_t;

static const ff_mfg_entry_t known_manufacturers[] = {
    {{0x00, 0x01, 0x01}, "Emerson / Rosemount"},
    {{0x00, 0x01, 0x02}, "Honeywell"},
    {{0x00, 0x01, 0x03}, "Yokogawa"},
    {{0x00, 0x01, 0x04}, "ABB"},
    {{0x00, 0x01, 0x05}, "Siemens"},
    {{0x00, 0x01, 0x06}, "Endress+Hauser"},
    {{0x00, 0x01, 0x07}, "Schneider Electric / Foxboro"},
    {{0x00, 0x01, 0x08}, "Pepperl+Fuchs"},
    {{0x00, 0x01, 0x09}, "Rockwell Automation"},
    {{0x00, 0x01, 0x0A}, "Metso / Valmet"},
    {{0x00, 0x01, 0x0B}, "Krohne"},
    {{0x00, 0x01, 0x0C}, "VEGA"},
    {{0x00, 0x01, 0x0D}, "Smar"},
    {{0x00, 0x01, 0x0E}, "Samson"},
    {{0x00, 0x01, 0x0F}, "GE / Baker Hughes"},
    {{0x00, 0x01, 0x10}, "SUPCON"},
    {{0x00, 0x01, 0x11}, "HollySys"},
    {{0x00, 0xFF, 0xFF}, "Unknown Manufacturer"}
};

#define KNOWN_MFG_COUNT (sizeof(known_manufacturers) / sizeof(known_manufacturers[0]))

const char* ff_manufacturer_name(const uint8_t manufacturer_id[3]) {
    if (!manufacturer_id) return "Unknown Manufacturer";

    for (size_t i = 0; i < KNOWN_MFG_COUNT - 1; i++) {
        if (memcmp(manufacturer_id, known_manufacturers[i].id, 3) == 0) {
            return known_manufacturers[i].name;
        }
    }
    return "Unknown Manufacturer";
}

int ff_manufacturer_is_known(const uint8_t manufacturer_id[3]) {
    if (!manufacturer_id) return 0;

    for (size_t i = 0; i < KNOWN_MFG_COUNT - 1; i++) {
        if (memcmp(manufacturer_id, known_manufacturers[i].id, 3) == 0) {
            return 1;
        }
    }
    return 0;
}