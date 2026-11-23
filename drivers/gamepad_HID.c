#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/cyw43_arch.h"

#include "btstack.h"


#include "ble/gatt-service/hids_device.h"
#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"

#include "gamepad_HID.h"

extern int btstack_main(int argc, const char * argv[]);   
void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static uint8_t battery = 100;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t protocol_mode = 1;

//D-pad not included in implementation
const uint8_t hid_descriptor_gamepad[] = {
    0x05, 0x01,                                      // Generic Desktop
    0x09, 0x05,                                      // Gamepad
    0xA1, 0x01,                                      // Collection

    // Buttons (includes bumpers)
    0x05, 0x09,   // Usage Page (Button)
    0x19, 0x01,   // Usage Minimum (Button 1)
    0x29, 0x08,   // Usage Maximum (Button 8)
    0x15, 0x00,   // Logical Minimum (0)
    0x25, 0x01,   // Logical Maximum (1)
    0x75, 0x01,   // Report Size (1 bit each)
    0x95, 0x08,   // Report Count
    0x81, 0x02,   // Input (Data, Var, Abs)

    // 2-axis potentiometers (joysticks)
    0x05, 0x01,   // Usage Page (Generic Desktop)
    0x09, 0x30,   // Usage (LX)
    0x09, 0x31,   // Usage (LY)
    0x09, 0x32,   // Usage (Z)
    0x09, 0x35,   // Usage (Rx)
    0x15, 0x81,   // Logical Minimum (-127)
    0x25, 0x7F,   // Logical Maximum (127)
    0x75, 0x08,   // Report Size (8 bits each)
    0x95, 0x04,   // Report Count (4 axes)
    0x81, 0x02,   // Input (Data, Var, Abs)

    // 1-axis potentiometers (triggers)
    0x05, 0x01,   // Usage Page (Generic Desktop)
    0x09, 0x34,   // Usage (Ry)
    0x09, 0x35,   // Usage (Rz)
    0x15, 0x81,   // Logical Minimum (-127)
    0x25, 0x7F,   // Logical Maximum (127)
    0x75, 0x08,   // Report Size (8 bits each)
    0x95, 0x02,   // Report Count (2 axes)
    0x81, 0x02,   // Input (Data, Var, Abs)
};

static const uint8_t adv_data[] = {
    0x02, 0x01, 0x06,
    0x03, 0x19, 0xC4, 0x03,                         // Denotes Gamepad
    0x09, 0x09, 'P','I','C','O',' ','H','I','D',
    0x03, 0x03, 0x12, 0x18
};

/*
    Packet handler code structure heavily influenced by V. Hunter Adams' "Building a Bluetooth GATT Server on the Pi Pico W"
    https://vanhunteradams.com/Pico/BLE/GATT_Server.html#A-packet-handler-for-setting-up-and-starting-GAP-advertisements

*/
void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;

    // Retrieve event type from HCI packet
    uint8_t event_type = hci_event_packet_get_type(packet);

    // Switch on event type . . .
    switch(event_type){
        // Setup GAP advertisement
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            bd_addr_t null_addr;
            memset(null_addr, 0, 6);
            gap_advertisements_set_params(0x0030, 0x0030, 0, 0, null_addr, 0x07, 0x00);
            assert(adv_data_len <= 31); // ble limitation
            gap_advertisements_set_data(sizeof(adv_data), (uint8_t*) adv_data);
            gap_advertisements_enable(1);

            break;
        // Disconnected from a client
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            break;
        // Ready to send ATT
        case ATT_EVENT_CAN_SEND_NOW:
            break;
        default:
            break;
    }
}

// Configuration influenced by pico SDK hog_keyboard_demo.c
void gamepad_init(void){
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return;
    }

    l2cap_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    att_server_init(adv_data, NULL, NULL);

    battery_service_server_init(battery);

    device_information_service_server_init();

    hids_device_init(0, hid_descriptor_gamepad, sizeof(hid_descriptor_gamepad));

    bd_addr_t null_addr;
    memset(null_addr, 0, 6);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // register for HIDS
    hids_device_register_packet_handler(packet_handler);

    hci_power_control(HCI_POWER_ON);
}