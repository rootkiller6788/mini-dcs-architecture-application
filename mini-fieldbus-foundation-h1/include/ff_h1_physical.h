/**
 * ff_h1_physical.h ? Foundation Fieldbus H1 Physical Layer (IEC 61158-2 Type 1)
 *
 * Defines the H1 physical layer: 31.25 kbit/s, Manchester-encoded, bus-powered
 * twisted-pair communication. This is the lowest layer of the FF H1 protocol stack,
 * carrying framed data over a DC-powered segment with intrinsic safety provisions.
 *
 * Course Mapping:
 *   MIT 2.171  ? Digital communication encoding schemes
 *   CMU 24-677 ? Fieldbus physical layer topologies
 *   ISA/IEC    ? IEC 61158-2: Physical Layer Specification and Service Definition
 *   RWTH Aachen ? Industrial Communication Systems, Feldbussysteme
 *
 * Knowledge Levels: L1 (Definitions), L3 (Engineering Structures)
 */

#ifndef FF_H1_PHYSICAL_H
#define FF_H1_PHYSICAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Physical Layer Constants (IEC 61158-2 Type 1)
 * ============================================================================ */

/** H1 bit rate: 31.25 kbit/s ?0.2% */
#define FF_H1_BITRATE_HZ         31250.0

/** Bit period in microseconds (1 / 31250 = 32.0 ?s) */
#define FF_H1_BIT_PERIOD_US      32.0

/** Maximum number of devices per H1 segment (IEC 61158-2 A.3.2, non-IS) */
#define FF_H1_MAX_DEVICES        32

/** Typical practical device count per segment (with power margin) */
#define FF_H1_TYPICAL_DEVICES    12

/** Maximum trunk length for Type A cable (shielded twisted pair, #18 AWG) */
#define FF_H1_MAX_TRUNK_LENGTH_M 1900.0

/** Maximum spur length: 120m at up to 32 devices, derated for more */
#define FF_H1_MAX_SPUR_LENGTH_M  120.0

/** Characteristic impedance of the bus (IEC 61158-2) */
#define FF_H1_IMPEDANCE_OHM      100.0

/** Number of terminators required per segment (one at each end of trunk) */
#define FF_H1_TERMINATOR_COUNT   2

/** Terminator resistance value in ohms */
#define FF_H1_TERMINATOR_R_OHM   100.0

/** Terminator series capacitance: 1 ?F (DC blocking, as specified) */
#define FF_H1_TERMINATOR_C_UF    1.0

/** Manchester signal level ? current-mode modulation on DC power bus (?10 mA) */
#define FF_H1_SIGNAL_CURRENT_MA  10.0

/** Minimum device supply voltage at the terminals (9 V DC) */
#define FF_H1_MIN_VOLTAGE_V      9.0

/** Typical segment supply voltage (24 V DC, industrial standard) */
#define FF_H1_SUPPLY_VOLTAGE_V   24.0

/** Preamble byte value: repeated 0x00 for at least 8 octets */
#define FF_H1_PREAMBLE_BYTE      0x00

/** Minimum preamble octets for receiver synchronization */
#define FF_H1_MIN_PREAMBLE_OCTETS 8

/** Maximum preamble octets */
#define FF_H1_MAX_PREAMBLE_OCTETS 15


/* ============================================================================
 * L1: Cable Types (IEC 61158-2 A.4)
 * ============================================================================ */

/** Cable type enumeration per IEC 61158-2 Table A.1 */
typedef enum {
    FF_CABLE_TYPE_A = 0,  /**< Shielded twisted pair, #18 AWG (0.8 mm?), preferred */
    FF_CABLE_TYPE_B = 1,  /**< Shielded multi-pair twisted pair, #22 AWG (0.32 mm?) */
    FF_CABLE_TYPE_C = 2,  /**< Unshielded multi-pair twisted pair, #22 AWG */
    FF_CABLE_TYPE_D = 3,  /**< Unshielded multi-pair, #16 AWG (1.25 mm?), non-twisted */
    FF_CABLE_COUNT  = 4
} ff_cable_type_t;

/** Cable electrical parameters */
typedef struct {
    ff_cable_type_t type;
    const char     *description;
    double          resistance_per_km;
    double          capacitance_per_km;
    double          attenuation_db_per_km;
    double          max_length_m;
} ff_cable_spec_t;

/** Get cable specification for a given cable type */
const ff_cable_spec_t* ff_cable_spec(ff_cable_type_t type);


/* ============================================================================
 * L1: Manchester Encoding ? Self-Clocking Code
 *
 * Rules (IEC 61158-2 5.3):
 *   Bit '1'  transition: high (first half), low (second half)  mid-bit falling edge
 *   Bit '0'  transition: low (first half), high (second half)  mid-bit rising edge
 *
 * The mid-bit transition guarantees a clock-edge in every bit cell, enabling
 * the receiver to recover the clock from the data stream without a separate
 * clock line.
 *
 *              +---+   +---+
 *   Bit '1':   |   |   |   |     high low  at mid-bit (falling edge)
 *         -----+   +---+   +-----
 *
 *          +---+       +---+
 *   Bit '0': |   |       |   |   low high  at mid-bit (rising edge)
 *         ---+   +-------+   +---
 *           0   0.5   1.0
 *             bit period 
 * ============================================================================ */

/**
 * Manchester encode a byte array into a bit-level output buffer.
 * Each input bit produces 2 output bits:
 *   logic-1  [1, 0]  (high low at mid-bit)
 *   logic-0  [0, 1]  (low high at mid-bit)
 *
 * Complexity: O(n) in input byte count
 * Reference: IEC 61158-2 5.3.2
 */
int ff_manchester_encode(const uint8_t *input, size_t input_bytes,
                         uint8_t *output, size_t *output_bits);

/**
 * Manchester decode a bit-level input buffer back to bytes.
 * Input length must be even.
 * Complexity: O(n) in input bits
 */
int ff_manchester_decode(const uint8_t *input, size_t input_bits,
                         uint8_t *output, size_t *output_bytes);

/**
 * Verify Manchester encoding round-trip: encode then decode must recover input.
 */
int ff_manchester_roundtrip_check(const uint8_t *data, size_t len);


/* ============================================================================
 * L1: Start/End Delimiters ? Frame Boundary Markers
 *
 * H1 uses "code violation" sequences that violate normal Manchester rules.
 * N+ = two consecutive high half-bits (no mid-bit transition, held high)
 * N- = two consecutive low half-bits  (no mid-bit transition, held low)
 *
 * Start Delimiter: 1 N+ N- 1 0 N- N+ 0  (8 bits / 16 half-bits)
 * End Delimiter:   1 N+ N- N+ N- 1 0 1  (8 bits / 16 half-bits)
 *
 * Reference: IEC 61158-2 5.4.2, Figure 5
 * ============================================================================ */

/** Half-bit symbol codes for delimiter construction */
#define FF_HALF_HIGH   1
#define FF_HALF_LOW    0
#define FF_HALF_NPLUS  2   /**< Non-data N+ symbol: two consecutive high half-bits */
#define FF_HALF_NMINUS 3   /**< Non-data N- symbol: two consecutive low half-bits  */

#define FF_DELIMITER_HALF_BITS  16

int ff_start_delimiter_write(uint8_t *buf);
int ff_end_delimiter_write(uint8_t *buf);
int ff_start_delimiter_detect(const uint8_t *buf, size_t pos);
int ff_end_delimiter_detect(const uint8_t *buf, size_t pos);


/* ============================================================================
 * L3: H1 Frame Structure
 *
 * Complete H1 frame (IEC 61158-2 5.4):
 *   [Preamble] [SD] [Data Link PDU] [ED]
 *
 * DL-PDU (IEC 61158-4 6): FC (1 octet) + DL-Address + User Data + FCS (2 octets)
 * ============================================================================ */

#define FF_H1_MAX_DLPDU_OCTETS   255
#define FF_H1_FCS_OCTETS         2
#define FF_H1_MIN_FRAME_OCTETS   14
#define FF_H1_MAX_FRAME_OCTETS   (FF_H1_MAX_PREAMBLE_OCTETS + 1 + FF_H1_MAX_DLPDU_OCTETS + 1)

typedef struct {
    uint8_t  preamble[FF_H1_MAX_PREAMBLE_OCTETS];
    size_t   preamble_len;
    uint8_t  start_delim[FF_DELIMITER_HALF_BITS];
    uint8_t  dlpdu[FF_H1_MAX_DLPDU_OCTETS];
    size_t   dlpdu_len;
    uint8_t  end_delim[FF_DELIMITER_HALF_BITS];
    uint16_t fcs;
} ff_h1_frame_t;

void ff_h1_frame_init(ff_h1_frame_t *frame);
int ff_h1_frame_assemble(const ff_h1_frame_t *frame,
                         uint8_t *output, size_t *output_len);


/* ============================================================================
 * L3: CRC-16-CCITT ? Frame Check Sequence
 *
 * Generator polynomial: G(x) = x^16 + x^12 + x^5 + 1 = 0x1021
 * Initial value: 0xFFFF, no XOR-out.
 * Reference: ITU-T X.25, IEC 61158-4 6.2.9
 * ============================================================================ */

uint16_t ff_crc16_ccitt(const uint8_t *data, size_t len);
int ff_crc16_verify(const uint8_t *dlpdu, size_t dlpdu_len);
size_t ff_crc16_append(uint8_t *dlpdu, size_t data_len);


/* ============================================================================
 * L3: Manchester Bit-Stream Utilities
 * ============================================================================ */

size_t ff_h1_total_half_bits(size_t preamble_octets, size_t dlpdu_octets);
double ff_h1_ber_estimate(size_t tx_bits, size_t error_bits);


/* ============================================================================
 * L1: H1 Signal Timing Parameters
 * ============================================================================ */

double ff_h1_max_spur_length(int num_devices);
double ff_h1_propagation_delay_us(double length_m, ff_cable_type_t cable);


#ifdef __cplusplus
}
#endif

#endif /* FF_H1_PHYSICAL_H */