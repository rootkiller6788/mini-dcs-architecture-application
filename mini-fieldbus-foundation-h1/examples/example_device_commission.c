/**
 * example_device_commission.c ? H1 Device Commissioning Simulation
 *
 * Simulates the FF H1 device commissioning sequence:
 *   1. Device powers on (UNINITIALIZED)
 *   2. Device detects bus activity (INITIALIZING)
 *   3. LAS probes and discovers device
 *   4. LAS reads device ID
 *   5. LAS assigns PD-TAG and permanent address via Set Address
 *   6. Device transitions to OPERATIONAL
 *   7. Time synchronization via TD messages
 *
 * Knowledge: L6 (Canonical Problem ? Device Commissioning)
 */

#include "ff_h1_system_mgmt.h"
#include "ff_h1_device.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("??????????????????????????????????????????????????\n");
    printf("  H1 Device Commissioning Simulation               \n");
    printf("  Device: Rosemount 3051S Pressure Transmitter    \n");
    printf("  PD-TAG: PT-2001A                                 \n");
    printf("??????????????????????????????????????????????????\n\n");

    /* Device identity (burned at factory) */
    uint8_t device_id[32] = {0};
    /* Emerson/Rosemount manufacturer ID: 00-01-01 */
    device_id[0] = 0x00; device_id[1] = 0x01; device_id[2] = 0x01;
    /* Serial number: unique per device */
    for (int i = 6; i < 20; i++) device_id[i] = (uint8_t)(i + 0xA0);
    /* Device type: 0x000A = pressure transmitter */
    device_id[20] = 0x00; device_id[21] = 0x0A;
    /* Rev 2, DD Rev 5, ITK 6.1 */
    device_id[22] = 0x02;
    device_id[23] = 0x05;
    device_id[24] = 0x61; /* ITK 6.1 */

    /* Parse device identity */
    ff_device_identity_t identity;
    ff_device_identity_parse(device_id, &identity);

    printf("??? Device Identity ???\n");
    char id_str[40];
    ff_device_id_format(device_id, id_str, sizeof(id_str));
    printf("  Device ID: %s\n", id_str);
    printf("  Manufacturer: %s\n",
           ff_manufacturer_name(identity.manufacturer_id));
    printf("  Device Type: 0x%02X%02X (Pressure Transmitter)\n",
           identity.device_type[0], identity.device_type[1]);
    printf("  ITK Version: %d.%d\n",
           identity.itk_version / 10, identity.itk_version % 10);

    /* Step 1: Power On ? UNINITIALIZED */
    ff_sm_agent_t sm;
    ff_sm_init(&sm, device_id);
    printf("\n??? Step 1: Power On ???\n");
    printf("  SM State: %s\n", ff_sm_state_name(sm.state));
    printf("  DL-Address: 0x%02X (temporary)\n", sm.dl_address);
    printf("  Permanent Address: none\n");

    /* Step 2: Bus Activity Detected ? INITIALIZING */
    ff_sm_start_initialization(&sm);
    printf("\n??? Step 2: Bus Activity Detected ???\n");
    printf("  SM State: %s\n", ff_sm_state_name(sm.state));

    /* Step 3: LAS probes and discovers device */
    printf("\n??? Step 3: LAS Discovery (Probe Node) ???\n");
    printf("  LAS sends PN to 0x%02X\n", sm.dl_address);
    printf("  Device sends PR (Probe Response)\n");
    printf("  LAS reads device ID via FMS Read\n");

    /* Step 4: LAS assigns PD-TAG and permanent address */
    printf("\n??? Step 4: Set Address ???\n");
    printf("  LAS assigns PD-TAG: PT-2001A\n");
    printf("  LAS assigns permanent address: 0x25\n");

    int rc = ff_sm_process_set_address(&sm, device_id, 0x25);
    if (rc == 0) {
        printf("  ? Device accepted new address: 0x%02X\n", sm.permanent_addr);
    } else {
        printf("  ? Set Address rejected (code: %d)\n", rc);
    }

    /* Now fully set operational */
    ff_sm_set_operational(&sm, "PT-2001A", 0x25);
    printf("  SM State: %s\n", ff_sm_state_name(sm.state));
    printf("  PD-TAG: %s\n", sm.pd_tag);
    printf("  DL-Address: 0x%02X\n", sm.dl_address);

    /* Step 5: Time Synchronization */
    printf("\n??? Step 5: Time Synchronization ???\n");
    ff_td_message_t td;
    td.las_time = 1680000000; /* Some epoch time */
    td.las_time_ns = 0;
    td.macrocycle_count = 1;

    for (int i = 0; i < 5; i++) {
        td.td_sequence = (uint16_t)(i + 1);
        td.macrocycle_count = (uint32_t)(i + 1);

        ff_time_sync_quality_t q = ff_sm_process_td(&sm, &td, 50);
        printf("  TD #%d: Sync Quality = ", i + 1);
        switch (q) {
            case FF_TIME_SYNC_NONE:   printf("NONE\n"); break;
            case FF_TIME_SYNC_COARSE: printf("COARSE\n"); break;
            case FF_TIME_SYNC_FINE:   printf("FINE\n"); break;
            case FF_TIME_SYNC_LOCKED: printf("LOCKED\n"); break;
        }
    }

    /* Display SMIB information */
    printf("\n??? SMIB Summary ???\n");
    uint8_t buf[64];
    size_t sz;

    sz = 1;
    ff_smib_read(&sm, FF_SMIB_DEVICE_STATE, buf, &sz);
    printf("  Device State: %s\n", ff_sm_state_name((ff_sm_state_t)buf[0]));

    sz = 1;
    ff_smib_read(&sm, FF_SMIB_DEVICE_ADDRESS, buf, &sz);
    printf("  DL-Address: 0x%02X\n", buf[0]);

    sz = 1;
    ff_smib_read(&sm, FF_SMIB_TIME_SYNC, buf, &sz);
    const char *sync_str;
    switch ((ff_time_sync_quality_t)buf[0]) {
        case FF_TIME_SYNC_NONE: sync_str = "NONE"; break;
        case FF_TIME_SYNC_COARSE: sync_str = "COARSE"; break;
        case FF_TIME_SYNC_FINE: sync_str = "FINE"; break;
        case FF_TIME_SYNC_LOCKED: sync_str = "LOCKED"; break;
        default: sync_str = "?"; break;
    }
    printf("  Time Sync: %s\n", sync_str);

    /* Find Tag Query check */
    printf("\n??? Find Tag Query Test ???\n");
    const char *test_tags[] = {"PT-2001A", "PT-2001B", "FT-1001", "PT-2001a"};
    for (int i = 0; i < 4; i++) {
        int match = ff_sm_find_tag_match(&sm, test_tags[i]);
        printf("  Query '%s': %s\n", test_tags[i], match ? "MATCH" : "NO MATCH");
    }

    printf("\n??? Commissioning Status ???\n");
    if (ff_sm_has_permanent_address(&sm) &&
        sm.state == FF_SM_STATE_OPERATIONAL &&
        ff_sm_time_sync_quality(&sm) >= FF_TIME_SYNC_COARSE) {
        printf("  ? Device fully commissioned and operational\n");
    } else {
        printf("  ??  Device not yet fully commissioned\n");
    }

    printf("\n??????????????????????????????????????????????????\n");
    return 0;
}