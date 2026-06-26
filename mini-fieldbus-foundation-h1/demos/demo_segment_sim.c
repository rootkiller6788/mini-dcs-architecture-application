/**
 * demo_segment_sim.c ? Interactive H1 Segment Simulator
 *
 * Demonstrates H1 segment behavior: CRC-16 encoding/verification,
 * Manchester encoding round-trip, and segment power budget calculation
 * with user-provided parameters.
 *
 * Knowledge: L6 (Canonical), L7 (Application Demo)
 */
#include "ff_h1_physical.h"
#include "ff_h1_segment.h"
#include "ff_h1_datalink.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("??????????????????????????????????????????????????\n");
    printf("  Foundation Fieldbus H1 ? Interactive Demo       \n");
    printf("??????????????????????????????????????????????????\n\n");

    /* Demo 1: Manchester Encoding */
    printf("??? Demo 1: Manchester Encoding ???\n");
    uint8_t data[] = {0xA5, 0x5A, 0x00, 0xFF};
    uint8_t encoded[128];
    uint8_t decoded[128];
    size_t enc_bits, dec_bytes;

    printf("  Input bytes: ");
    for (size_t i = 0; i < 4; i++) printf("0x%02X ", data[i]);
    printf("\n");

    ff_manchester_encode(data, 4, encoded, &enc_bits);
    printf("  Encoded: %zu half-bits\n", enc_bits);

    ff_manchester_decode(encoded, enc_bits, decoded, &dec_bytes);
    printf("  Decoded: %zu bytes\n", dec_bytes);
    printf("  Round-trip: %s\n\n",
           ff_manchester_roundtrip_check(data, 4) ? "? PASS" : "? FAIL");

    /* Demo 2: CRC-16 */
    printf("??? Demo 2: CRC-16-CCITT ???\n");
    uint8_t msg[] = "Foundation Fieldbus H1 Frame Data";
    size_t msg_len = strlen((char*)msg);
    uint16_t crc = ff_crc16_ccitt(msg, msg_len);
    printf("  Message: \"%s\"\n", msg);
    printf("  CRC-16: 0x%04X\n", crc);

    uint8_t buffer[256];
    memcpy(buffer, msg, msg_len);
    size_t total = ff_crc16_append(buffer, msg_len);
    printf("  Appended FCS: 0x%02X 0x%02X\n",
           buffer[msg_len], buffer[msg_len + 1]);
    printf("  Verify (good): %s\n",
           ff_crc16_verify(buffer, total) ? "? PASS" : "? FAIL");

    buffer[2] ^= 0x80; /* corrupt one bit */
    printf("  Verify (corrupt): %s\n\n",
           ff_crc16_verify(buffer, total) ? "? FAIL (should detect)" : "? DETECTED");

    /* Demo 3: Segment Power Budget */
    printf("??? Demo 3: Quick Segment Power Budget ???\n");
    ff_segment_config_t config;
    memset(&config, 0, sizeof(config));
    config.power_supply.output_voltage_v = 24.0;
    config.power_supply.max_current_ma = 500.0;
    config.power_supply.conditioner_drop_v = 0.5;
    config.trunk_cable_type = FF_CABLE_TYPE_A;
    config.trunk_length_m = 400.0;
    config.num_devices = 8;
    config.temperature_c = 25.0;

    for (int i = 0; i < 8; i++) {
        config.device_current_ma[i] = 20.0;
        config.spur_cable_type[i] = FF_CABLE_TYPE_A;
        config.spur_length_m[i] = 40.0;
    }

    ff_power_budget_result_t result;
    ff_segment_power_budget(&config, &result);
    printf("  Trunk: %.0fm, Devices: %d\n", config.trunk_length_m, config.num_devices);
    printf("  Total current: %.1f mA\n", result.total_current_ma);
    printf("  Min device voltage: %.3f V (Device #%d)\n",
           result.min_device_voltage_v, result.worst_device_index + 1);
    printf("  Viable: %s\n", result.is_viable ? "? YES" : "? NO");

    /* Demo 4: LAS quick test */
    printf("\n??? Demo 4: LAS Quick Schedule ???\n");
    ff_las_context_t las;
    ff_las_init(&las, 0x10);
    las.state = FF_LAS_STATE_ACTIVE;
    las.cd_schedule.macrocycle_us = 100000;

    ff_cd_entry_t e = {0x20, 0, 1000, 500};
    ff_las_cd_add(&las, &e);
    ff_las_cd_add(&las, &e); /* duplicate, will fail ? non-monotonic */
    ff_live_list_add(&las.live_list, 0x20, 2);

    ff_las_run_macrocycle(&las);
    printf("  CD entries: %zu executed, %u overruns\n",
           las.cd_schedule.count, las.cd_overruns);

    printf("\n??????????????????????????????????????????????????\n");
    return 0;
}