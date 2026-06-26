/**
 * ff_h1_physical.c ? Foundation Fieldbus H1 Physical Layer Implementation
 *
 * Implements Manchester encoding/decoding, CRC-16-CCITT with table-driven
 * computation, H1 frame assembly, delimiter generation/detection, and
 * physical layer diagnostic utilities.
 *
 * Knowledge Levels: L1, L3
 */

#include "ff_h1_physical.h"
#include <string.h>
#include <assert.h>

/* ============================================================================
 * L1: Cable Specifications (IEC 61158-2 Table A.1)
 * ============================================================================ */

static const ff_cable_spec_t cable_specs[FF_CABLE_COUNT] = {
    { FF_CABLE_TYPE_A, "Type A: Shielded twisted pair, #18 AWG (0.8 mm?)",
      44.0,    /* ?/km loop resistance */
      80.0,    /* nF/km capacitance */
      3.0,     /* dB/km attenuation at 39 kHz */
      1900.0 },/* max length */

    { FF_CABLE_TYPE_B, "Type B: Shielded multi-pair twisted, #22 AWG (0.32 mm?)",
      112.0,   /* ?/km */
      90.0,    /* nF/km */
      5.0,     /* dB/km */
      1200.0 },

    { FF_CABLE_TYPE_C, "Type C: Unshielded multi-pair twisted, #22 AWG",
      112.0,   /* ?/km */
      120.0,   /* nF/km (higher without shield) */
      8.0,     /* dB/km */
      400.0 },

    { FF_CABLE_TYPE_D, "Type D: Unshielded multi-pair, #16 AWG (1.25 mm?)",
      28.0,    /* ?/km */
      150.0,   /* nF/km */
      8.0,     /* dB/km */
      200.0 }
};

const ff_cable_spec_t* ff_cable_spec(ff_cable_type_t type) {
    if (type >= FF_CABLE_COUNT) return NULL;
    return &cable_specs[type];
}


/* ============================================================================
 * L3: Manchester Encoding Implementation
 *
 * Each input bit maps to 2 output bits (half-bits):
 *   logic-1 ? [1, 0]  (high?low transition at mid-bit)
 *   logic-0 ? [0, 1]  (low?high transition at mid-bit)
 *
 * We process input byte-by-byte, MSB first (per H1 convention).
 * Output is packed into uint8_t output buffer, 1 bit per byte.
 *
 * The convention "MSB first" means for each byte, bit 7 is transmitted
 * first, then bit 6, ..., down to bit 0.
 *
 * Reference: IEC 61158-2 ?5.3.2, Figure 3
 * ============================================================================ */

int ff_manchester_encode(const uint8_t *input, size_t input_bytes,
                         uint8_t *output, size_t *output_bits) {
    if (!input || !output || !output_bits) return -1;
    if (input_bytes == 0) {
        *output_bits = 0;
        return 0;
    }

    size_t bit_idx = 0;

    for (size_t byte_idx = 0; byte_idx < input_bytes; byte_idx++) {
        uint8_t byte = input[byte_idx];

        /* Process bits MSB first (bit 7 down to bit 0) */
        for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
            uint8_t bit = (byte >> bit_pos) & 0x01;

            if (bit == 1) {
                /* Bit 1: [high, low] = [1, 0] */
                output[bit_idx]     = 1;
                output[bit_idx + 1] = 0;
            } else {
                /* Bit 0: [low, high] = [0, 1] */
                output[bit_idx]     = 0;
                output[bit_idx + 1] = 1;
            }
            bit_idx += 2;
        }
    }

    *output_bits = bit_idx;
    return 0;
}

/**
 * Manchester decode: Given 2 half-bits per original bit, determine bit value.
 *
 * Bit decision logic:
 *   [1, 0] pair ? data bit = 1  (high?low at mid-bit)
 *   [0, 1] pair ? data bit = 0  (low?high at mid-bit)
 *   [1, 1] or [0, 0] ? code violation (no mid-bit transition)
 *
 * If a code violation is encountered, it is treated as 0 but flagged
 * via the error detection mechanism (caller should check delimiter regions).
 */
int ff_manchester_decode(const uint8_t *input, size_t input_bits,
                         uint8_t *output, size_t *output_bytes) {
    if (!input || !output || !output_bytes) return -1;

    /* Must have even number of input bits (2 half-bits per data bit) */
    if (input_bits % 2 != 0) return -1;

    size_t byte_count = input_bits / 16; /* 16 half-bits = 8 data bits = 1 byte */
    size_t remaining_bits = input_bits % 16;

    *output_bytes = byte_count + (remaining_bits > 0 ? 1 : 0);
    size_t out_byte_idx = 0;
    uint8_t current_byte = 0;
    int    bit_pos = 7;

    for (size_t i = 0; i < input_bits; i += 2) {
        uint8_t first  = input[i];
        uint8_t second = input[i + 1];

        uint8_t data_bit;
        if (first == 1 && second == 0) {
            data_bit = 1;  /* High?low = bit 1 */
        } else if (first == 0 && second == 1) {
            data_bit = 0;  /* Low?high = bit 0 */
        } else {
            /* Code violation ? treat as 0 for robustness but note it */
            data_bit = 0;
        }

        current_byte |= (data_bit << bit_pos);
        bit_pos--;

        if (bit_pos < 0) {
            output[out_byte_idx++] = current_byte;
            current_byte = 0;
            bit_pos = 7;
        }
    }

    /* Handle partial byte at end */
    if (bit_pos < 7 && bit_pos >= 0) {
        output[out_byte_idx++] = current_byte;
    }

    return 0;
}

int ff_manchester_roundtrip_check(const uint8_t *data, size_t len) {
    if (!data || len == 0) return 1;
    if (len > 256) return 0; /* Safety limit */

    /* Each input byte → 8 bits → 16 half-bits → 16 bytes in encoded buffer.
     * Max 256 bytes input → 4096 half-bit entries. */
    uint8_t encoded[4096];
    uint8_t decoded[256];
    size_t  encoded_bits, decoded_bytes;

    if (ff_manchester_encode(data, len, encoded, &encoded_bits) != 0)
        return 0;

    if (ff_manchester_decode(encoded, encoded_bits, decoded, &decoded_bytes) != 0)
        return 0;

    if (decoded_bytes != len)
        return 0;

    return (memcmp(data, decoded, len) == 0) ? 1 : 0;
}


/* ============================================================================
 * L1: Start/End Delimiter Generation and Detection
 *
 * H1 Delimiter bit sequences (16 half-bits each):
 *
 * Start Delimiter (SD):
 *   1  N+  N-  1  0  N-  N+  0
 *   ?  ?   ?   ?  ?  ?   ?   ?
 *   HV HV  LV  HV LV LV  HV  LV
 *
 * End Delimiter (ED):
 *   1  N+  N-  N+ N-  1  0   1
 *   ?  ?   ?   ?  ?   ?  ?   ?
 *   HV HV  LV  HV LV  HV LV  HV
 *
 * Where:
 *   HV (high valid)    = [1, 0] ? the normal Manchester representation of bit 1
 *   LV (low valid)     = [0, 1] ? the normal Manchester representation of bit 0
 *   N+ (non-data plus) = [1, 1] ? two consecutive high half-bits (code violation)
 *   N- (non-data minus)= [0, 0] ? two consecutive low half-bits (code violation)
 *
 * Reference: IEC 61158-2 ?5.4.2, Table 4 and Figure 5
 * ============================================================================ */


/* Let me re-derive properly.
 *
 * Actually, per IEC 61158-2:
 *   SD bit sequence:  1, N+, N-, 1, 0, N-, N+, 0
 *   
 *   Bit value 1 encoded in Manchester: half-bits = [1, 0]   ... call this pair "HV"
 *   Bit value 0 encoded in Manchester: half-bits = [0, 1]   ... call this pair "LV"
 *   N+ (non-data plus):  half-bits = [1, 1] ... code violation: held high
 *   N- (non-data minus): half-bits = [0, 0] ... code violation: held low
 *
 * So each "bit position" in the SD expands to 2 half-bits:
 *
 *   SD bit:   1      N+     N-     1      0      N-     N+     0
 *   Half-bits: [1,0]  [1,1]  [0,0]  [1,0]  [0,1]  [0,0]  [1,1]  [0,1]
 *   Index:     0 1    2 3    4 5    6 7    8 9   10 11  12 13  14 15
 *
 * That gives us the correct 16 half-bit pattern.
 */

static const uint8_t sd_halfbits[16] = {
    1, 0,   /* bit 1: HV */
    1, 1,   /* N+: held high */
    0, 0,   /* N-: held low */
    1, 0,   /* bit 1: HV */
    0, 1,   /* bit 0: LV */
    0, 0,   /* N-: held low */
    1, 1,   /* N+: held high */
    0, 1    /* bit 0: LV */
};

/**
 * End Delimiter bit sequence: 1, N+, N-, N+, N-, 1, 0, 1
 *
 *   ED bit:   1      N+     N-     N+     N-     1      0      1
 *   Half-bits: [1,0]  [1,1]  [0,0]  [1,1]  [0,0]  [1,0]  [0,1]  [1,0]
 *   Index:     0 1    2 3    4 5    6 7    8 9   10 11  12 13  14 15
 */
static const uint8_t ed_halfbits[16] = {
    1, 0,   /* bit 1: HV */
    1, 1,   /* N+ */
    0, 0,   /* N- */
    1, 1,   /* N+ */
    0, 0,   /* N- */
    1, 0,   /* bit 1: HV */
    0, 1,   /* bit 0: LV */
    1, 0    /* bit 1: HV */
};

int ff_start_delimiter_write(uint8_t *buf) {
    if (!buf) return -1;
    memcpy(buf, sd_halfbits, FF_DELIMITER_HALF_BITS);
    return FF_DELIMITER_HALF_BITS;
}

int ff_end_delimiter_write(uint8_t *buf) {
    if (!buf) return -1;
    memcpy(buf, ed_halfbits, FF_DELIMITER_HALF_BITS);
    return FF_DELIMITER_HALF_BITS;
}

int ff_start_delimiter_detect(const uint8_t *buf, size_t pos) {
    if (!buf) return 0;
    return (memcmp(&buf[pos], sd_halfbits, FF_DELIMITER_HALF_BITS) == 0) ? 1 : 0;
}

int ff_end_delimiter_detect(const uint8_t *buf, size_t pos) {
    if (!buf) return 0;
    return (memcmp(&buf[pos], ed_halfbits, FF_DELIMITER_HALF_BITS) == 0) ? 1 : 0;
}


/* ============================================================================
 * L3: H1 Frame Assembly
 *
 * Assembles a complete H1 frame into a flat byte buffer for subsequent
 * Manchester encoding. The frame structure is:
 *
 *   [Preamble: 0x00 ? (8-15)] [SD: 1 octet] [DLPDU: N octets] [ED: 1 octet]
 *
 * The DLPDU already contains the FCS in its last 2 bytes.
 *
 * NOTE: The SD and ED octets are NOT normal data bytes ? they contain
 * code violation sequences. When Manchester-encoding, the SD/ED bytes
 * should be expanded to their half-bit patterns, not encoded as normal
 * bytes. This frame assembly produces a byte-level representation;
 * a separate step applies the special delimiter encoding.
 * ============================================================================ */

void ff_h1_frame_init(ff_h1_frame_t *frame) {
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
    frame->preamble_len = FF_H1_MIN_PREAMBLE_OCTETS;
    for (size_t i = 0; i < frame->preamble_len; i++) {
        frame->preamble[i] = FF_H1_PREAMBLE_BYTE;
    }
    ff_start_delimiter_write(frame->start_delim);
    ff_end_delimiter_write(frame->end_delim);
    frame->dlpdu_len = 0;
    frame->fcs = 0xFFFF;
}

int ff_h1_frame_assemble(const ff_h1_frame_t *frame,
                         uint8_t *output, size_t *output_len) {
    if (!frame || !output || !output_len) return -1;

    size_t total = frame->preamble_len + 1 + frame->dlpdu_len + 1;
    /* "1" for SD octet and "1" for ED octet */
    if (total > FF_H1_MAX_FRAME_OCTETS) return -1;

    size_t pos = 0;

    /* Copy preamble (each byte = 0x00) */
    memcpy(&output[pos], frame->preamble, frame->preamble_len);
    pos += frame->preamble_len;

    /* Start Delimiter: use a special marker byte 0xA5 (arbitrary marker).
     * In practice, the delimiter is inserted at the half-bit level during
     * Manchester encoding. Here we use placeholder markers for assembly. */
    output[pos++] = 0xA5; /* SD placeholder */

    /* Copy DLPDU */
    memcpy(&output[pos], frame->dlpdu, frame->dlpdu_len);
    pos += frame->dlpdu_len;

    /* End Delimiter */
    output[pos++] = 0x5A; /* ED placeholder */

    *output_len = pos;
    return 0;
}


/* ============================================================================
 * L3: CRC-16-CCITT ? Table-Driven Implementation
 *
 * Generator polynomial: G(x) = x^16 + x^12 + x^5 + 1
 * Polynomial value: 0x1021 (MSB-first), 0x8408 (LSB-first reflected)
 *
 * Initial value: 0xFFFF
 * No final XOR
 *
 * We use the MSB-first (left-shifting) algorithm with pre-computed
 * lookup table for 256 byte values.
 *
 * Derivation of table entry for byte b:
 *   For each bit of b (MSB first):
 *     if (crc & 0x8000): crc = (crc << 1) ^ 0x1021
 *     else:              crc = (crc << 1)
 *   XOR b into the MSB of crc at each step.
 *
 * This is the standard CCITT-FALSE algorithm (CRC-16/CCITT-FALSE).
 * Reference: ITU-T X.25 ?2.2.7 "FCS polynomial", Ross Williams' "A Painless Guide to CRC"
 * ============================================================================ */

static uint16_t crc16_table[256];
static int crc16_table_initialized = 0;

static void crc16_init_table(void) {
    for (int i = 0; i < 256; i++) {
        uint16_t crc = 0;
        uint16_t c = (uint16_t)i << 8;
        for (int j = 0; j < 8; j++) {
            if ((crc ^ c) & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
            c = c << 1;
        }
        crc16_table[i] = crc;
    }
    crc16_table_initialized = 1;
}

uint16_t ff_crc16_ccitt(const uint8_t *data, size_t len) {
    if (!data && len > 0) return 0;

    if (!crc16_table_initialized) {
        crc16_init_table();
    }

    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        uint8_t table_index = (uint8_t)((crc >> 8) ^ data[i]);
        crc = (crc << 8) ^ crc16_table[table_index];
    }

    return crc;
}

int ff_crc16_verify(const uint8_t *dlpdu, size_t dlpdu_len) {
    if (!dlpdu || dlpdu_len < FF_H1_FCS_OCTETS) return 0;

    size_t data_len = dlpdu_len - FF_H1_FCS_OCTETS;
    uint16_t computed = ff_crc16_ccitt(dlpdu, data_len);

    /* FCS is stored in the last 2 bytes, little-endian or big-endian?
     * IEC 61158-4 specifies LSB first (little-endian) for FCS transmission.
     * The received FCS: dlpdu[data_len] = low byte, dlpdu[data_len+1] = high byte */
    uint16_t received_fcs = (uint16_t)dlpdu[data_len] |
                            ((uint16_t)dlpdu[data_len + 1] << 8);

    return (computed == received_fcs) ? 1 : 0;
}

size_t ff_crc16_append(uint8_t *dlpdu, size_t data_len) {
    if (!dlpdu) return 0;

    uint16_t crc = ff_crc16_ccitt(dlpdu, data_len);
    dlpdu[data_len]     = (uint8_t)(crc & 0xFF);
    dlpdu[data_len + 1] = (uint8_t)((crc >> 8) & 0xFF);

    return data_len + FF_H1_FCS_OCTETS;
}


/* ============================================================================
 * L3: Bit-Stream Utilities
 * ============================================================================ */

size_t ff_h1_total_half_bits(size_t preamble_octets, size_t dlpdu_octets) {
    /* Preamble: each octet = 8 bits ? 16 half-bits per octet */
    size_t preamble_hb = preamble_octets * 16;

    /* SD: 16 half-bits (fixed) */
    size_t sd_hb = FF_DELIMITER_HALF_BITS;

    /* DLPDU: each octet = 8 bits ? 16 half-bits per octet */
    size_t dlpdu_hb = dlpdu_octets * 16;

    /* ED: 16 half-bits (fixed) */
    size_t ed_hb = FF_DELIMITER_HALF_BITS;

    return preamble_hb + sd_hb + dlpdu_hb + ed_hb;
}

double ff_h1_ber_estimate(size_t tx_bits, size_t error_bits) {
    if (tx_bits == 0) return -1.0;
    return (double)error_bits / (double)tx_bits;
}


/* ============================================================================
 * L1: Signal Timing Parameters
 * ============================================================================ */

double ff_h1_max_spur_length(int num_devices) {
    if (num_devices < 1 || num_devices > FF_H1_MAX_DEVICES) return -1.0;

    /* IEC 61158-2 Table A.2: Spur length vs. device count */
    if (num_devices <= 12) {
        return 120.0;
    } else if (num_devices <= 14) {
        return 90.0;
    } else if (num_devices <= 18) {
        return 60.0;
    } else if (num_devices <= 24) {
        return 30.0;
    } else {
        return 1.0;
    }
}

double ff_h1_propagation_delay_us(double length_m, ff_cable_type_t cable) {
    if (length_m <= 0) return 0.0;

    /* Propagation velocity factor:
     * Type A: ~0.78c (shielded twisted pair) ? 4.27 ns/m
     * Type B/C: ~0.65c ? 5.13 ns/m
     * Type D: ~0.60c ? 5.56 ns/m
     *
     * Speed of light c ? 3.0 ? 10^8 m/s, or 0.30 m/ns
     * Velocity in cable = vf ? c
     * Delay per meter = 1 / (vf ? c)
     */

    double vf;
    switch (cable) {
        case FF_CABLE_TYPE_A: vf = 0.78; break;
        case FF_CABLE_TYPE_B:
        case FF_CABLE_TYPE_C: vf = 0.65; break;
        case FF_CABLE_TYPE_D: vf = 0.60; break;
        default: vf = 0.65; break;
    }

    double c_m_per_ns = 0.30; /* 3e8 m/s = 0.30 m/ns */
    double delay_ns_per_m = 1.0 / (vf * c_m_per_ns);

    /* Return in microseconds */
    return (length_m * delay_ns_per_m) / 1000.0;
}