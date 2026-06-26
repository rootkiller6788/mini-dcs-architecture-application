/**
 * test_physical.c ? Tests for ff_h1_physical.h
 *
 * Covers: Manchester encode/decode round-trip, CRC-16 computation and
 * verification, delimiter generation/detection, frame assembly, and
 * physical layer calculations.
 */

#include "ff_h1_physical.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT_EQ_INT(a, b, msg) do { if ((a) != (b)) { FAIL(msg); return; } } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define ASSERT_DBL_NEAR(a, b, tol, msg) do { if (fabs((a)-(b)) > (tol)) { FAIL(msg); return; } } while(0)

/* --- Manchester Encoding --- */
static void test_manchester_encode_byte_zero(void) {
    TEST("Manchester encode 0x00");
    uint8_t input[] = {0x00};
    uint8_t output[16];
    size_t out_bits;

    int rc = ff_manchester_encode(input, 1, output, &out_bits);
    ASSERT_EQ_INT(rc, 0, "encode returned error");
    ASSERT_EQ_INT(out_bits, 16, "wrong bit count");

    /* 0x00 = 00000000 ? each bit 0 ? [0,1], [0,1], ..., [0,1] */
    for (int i = 0; i < 16; i += 2) {
        ASSERT_EQ_INT(output[i], 0, "expected half-bit 0");
        ASSERT_EQ_INT(output[i+1], 1, "expected half-bit 1");
    }
    PASS();
}

static void test_manchester_encode_byte_ff(void) {
    TEST("Manchester encode 0xFF");
    uint8_t input[] = {0xFF};
    uint8_t output[16];
    size_t out_bits;

    ff_manchester_encode(input, 1, output, &out_bits);
    ASSERT_EQ_INT(out_bits, 16, "wrong bit count");

    /* 0xFF = 11111111 ? each bit 1 ? [1,0], [1,0], ... */
    for (int i = 0; i < 16; i += 2) {
        ASSERT_EQ_INT(output[i], 1, "expected half-bit 1");
        ASSERT_EQ_INT(output[i+1], 0, "expected half-bit 0");
    }
    PASS();
}

static void test_manchester_roundtrip(void) {
    TEST("Manchester round-trip");
    uint8_t original[256];
    for (int i = 0; i < 256; i++) original[i] = (uint8_t)i;
    ASSERT_TRUE(ff_manchester_roundtrip_check(original, 256), "round-trip failed");
    PASS();
}

static void test_manchester_roundtrip_random(void) {
    TEST("Manchester round-trip random data");
    uint8_t data[] = {0xA5, 0x5A, 0xFF, 0x00, 0x81, 0x42, 0x3C, 0x7E};
    ASSERT_TRUE(ff_manchester_roundtrip_check(data, 8), "round-trip failed");
    PASS();
}

static void test_manchester_null_input(void) {
    TEST("Manchester encode NULL input");
    uint8_t output[16];
    size_t out_bits;
    ASSERT_EQ_INT(ff_manchester_encode(NULL, 1, output, &out_bits), -1, "should reject NULL input");
    PASS();
}

static void test_manchester_decode_odd_bits(void) {
    TEST("Manchester decode odd bit count");
    uint8_t input[] = {0, 1, 0}; /* 3 half-bits = odd */
    uint8_t output[16];
    size_t out_bytes;
    ASSERT_EQ_INT(ff_manchester_decode(input, 3, output, &out_bytes), -1, "should reject odd bits");
    PASS();
}

/* --- CRC-16 --- */
static void test_crc16_known_value(void) {
    TEST("CRC-16-CCITT known value");
    /* Test vector: "123456789" ? CRC-16-CCITT = 0x29B1 */
    uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t crc = ff_crc16_ccitt(data, 9);
    ASSERT_EQ_INT(crc, 0x29B1, "CRC-16 mismatch for known test vector");
    PASS();
}

static void test_crc16_zero_length(void) {
    TEST("CRC-16 zero length");
    uint16_t crc = ff_crc16_ccitt(NULL, 0);
    ASSERT_EQ_INT(crc, 0xFFFF, "CRC-16 of empty should be 0xFFFF");
    PASS();
}

static void test_crc16_append_and_verify(void) {
    TEST("CRC-16 append and verify cycle");
    uint8_t buffer[260];
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    memcpy(buffer, data, 5);
    size_t total = ff_crc16_append(buffer, 5);
    ASSERT_EQ_INT(total, 7, "append should add 2 FCS bytes");

    ASSERT_TRUE(ff_crc16_verify(buffer, total), "CRC verification should pass");
    PASS();
}

static void test_crc16_corruption_detected(void) {
    TEST("CRC-16 corruption detection");
    uint8_t buffer[260] = {0x10, 0x20, 0x30, 0x40, 0x50};
    size_t total = ff_crc16_append(buffer, 5);

    /* Corrupt one byte */
    buffer[2] ^= 0x01;
    ASSERT_TRUE(!ff_crc16_verify(buffer, total), "corruption should be detected");
    PASS();
}

/* --- Delimiters --- */
static void test_start_delimiter_detect(void) {
    TEST("Start delimiter detect");
    uint8_t buf[32] = {0};
    ff_start_delimiter_write(buf);
    ASSERT_TRUE(ff_start_delimiter_detect(buf, 0), "SD should be detected at pos 0");
    ASSERT_TRUE(!ff_start_delimiter_detect(buf, 1), "SD should NOT be detected at pos 1");
    PASS();
}

static void test_end_delimiter_detect(void) {
    TEST("End delimiter detect");
    uint8_t buf[32] = {0};
    ff_end_delimiter_write(buf);
    ASSERT_TRUE(ff_end_delimiter_detect(buf, 0), "ED should be detected at pos 0");
    memset(buf, 0xFF, 32);
    ASSERT_TRUE(!ff_end_delimiter_detect(buf, 0), "random data should not match ED");
    PASS();
}

/* --- Frame Assembly --- */
static void test_frame_init(void) {
    TEST("Frame init");
    ff_h1_frame_t frame;
    ff_h1_frame_init(&frame);
    ASSERT_EQ_INT(frame.preamble_len, FF_H1_MIN_PREAMBLE_OCTETS, "default preamble length");
    ASSERT_EQ_INT(frame.preamble[0], 0x00, "preamble byte should be 0x00");
    ASSERT_EQ_INT(frame.dlpdu_len, 0, "empty DLPDU");
    PASS();
}

static void test_frame_assemble(void) {
    TEST("Frame assemble");
    ff_h1_frame_t frame;
    ff_h1_frame_init(&frame);
    frame.dlpdu[0] = 0x40; /* FC = DT / data transfer */
    frame.dlpdu[1] = 0x10; /* dest */
    frame.dlpdu[2] = 0x20; /* src */
    frame.dlpdu_len = 3;

    uint8_t output[300];
    size_t out_len;
    int rc = ff_h1_frame_assemble(&frame, output, &out_len);
    ASSERT_EQ_INT(rc, 0, "assemble should succeed");

    /* Expected: preamble(8) + SD(1) + dlpdu(3) + ED(1) = 13 */
    ASSERT_EQ_INT(out_len, 13, "wrong assembled frame length");
    PASS();
}

/* --- Cable Specifications --- */
static void test_cable_spec_type_a(void) {
    TEST("Cable spec Type A");
    const ff_cable_spec_t *spec = ff_cable_spec(FF_CABLE_TYPE_A);
    ASSERT_TRUE(spec != NULL, "Type A spec should exist");
    ASSERT_DBL_NEAR(spec->resistance_per_km, 44.0, 0.1, "Type A resistance ~44 ?/km");
    ASSERT_DBL_NEAR(spec->max_length_m, 1900.0, 0.1, "Type A max 1900m");
    PASS();
}

static void test_cable_spec_null_for_invalid(void) {
    TEST("Cable spec NULL for invalid type");
    ASSERT_TRUE(ff_cable_spec((ff_cable_type_t)99) == NULL, "invalid type should return NULL");
    PASS();
}

/* --- Max Spur Length --- */
static void test_max_spur_length(void) {
    TEST("Max spur length vs device count");
    ASSERT_DBL_NEAR(ff_h1_max_spur_length(10), 120.0, 0.01, "10 devices ? 120m");
    ASSERT_DBL_NEAR(ff_h1_max_spur_length(13), 90.0, 0.01, "13 devices ? 90m");
    ASSERT_DBL_NEAR(ff_h1_max_spur_length(16), 60.0, 0.01, "16 devices ? 60m");
    ASSERT_DBL_NEAR(ff_h1_max_spur_length(20), 30.0, 0.01, "20 devices ? 30m");
    ASSERT_DBL_NEAR(ff_h1_max_spur_length(30), 1.0, 0.01, "30 devices ? 1m");
    PASS();
}

static void test_max_spur_length_invalid(void) {
    TEST("Max spur length invalid input");
    ASSERT_DBL_NEAR(ff_h1_max_spur_length(0), -1.0, 0.01, "0 devices ? invalid");
    ASSERT_DBL_NEAR(ff_h1_max_spur_length(33), -1.0, 0.01, "33 devices ? invalid");
    PASS();
}

/* --- Propagation Delay --- */
static void test_propagation_delay(void) {
    TEST("Propagation delay");
    double delay = ff_h1_propagation_delay_us(100.0, FF_CABLE_TYPE_A);
    /* Type A: vf=0.78, c=0.3 m/ns ? delay ? 0.427 ?s for 100m */
    ASSERT_TRUE(delay > 0.3 && delay < 0.6, "delay should be ~0.4 ?s for 100m Type A");
    PASS();
}

/* --- BER Estimate --- */
static void test_ber_estimate(void) {
    TEST("BER estimate");
    double ber = ff_h1_ber_estimate(1000000, 100);
    ASSERT_DBL_NEAR(ber, 0.0001, 1e-9, "BER = 100/1M = 1e-4");
    ASSERT_DBL_NEAR(ff_h1_ber_estimate(0, 0), -1.0, 0.01, "zero TX bits ? error");
    PASS();
}

int main(void) {
    printf("=== test_physical ===\n");

    test_manchester_encode_byte_zero();
    test_manchester_encode_byte_ff();
    test_manchester_roundtrip();
    test_manchester_roundtrip_random();
    test_manchester_null_input();
    test_manchester_decode_odd_bits();
    test_crc16_known_value();
    test_crc16_zero_length();
    test_crc16_append_and_verify();
    test_crc16_corruption_detected();
    test_start_delimiter_detect();
    test_end_delimiter_detect();
    test_frame_init();
    test_frame_assemble();
    test_cable_spec_type_a();
    test_cable_spec_null_for_invalid();
    test_max_spur_length();
    test_max_spur_length_invalid();
    test_propagation_delay();
    test_ber_estimate();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}