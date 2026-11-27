
/*
    gamepad_HID.c

    This module... 
        * configures the access profile and HID descriptor of the device
        * advertises the device over BLE 
        * manages input reports

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btstack.h"

#include "hog_gamepad.h"
#include "gamepad_hid.h"

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static uint8_t battery = 100;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t protocol_mode = 1;

/*
    Used a couple sources to help understand HID descriptors:
    https://www.freebsddiary.org/APC/usb_hid_usages.php
    https://docs.kernel.org/hid/hidintro.html
    https://eleccelerator.com/usbdescreqparser/

    D-pad not included in implementation
*/
const uint8_t hid_descriptor_gamepad[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)

        0xA1, 0x01,        // Collection (Application)

        0x85, 0x01,        // Report ID (1) -- Congruent with hids.h

        // Buttons (includes bumpers)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x08,        //   Usage Maximum (0x08)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x08,        //   Report Count (8)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        // Joysticks
        0x05, 0x01,        //   Usage Page (Generic Desktop)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x09, 0x32,        //   Usage (Z)
        0x09, 0x33,        //   Usage (Rx)
        0x15, 0x81,        //   Logical Minimum (-127)
        0x25, 0x7F,        //   Logical Maximum (127)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x04,        //   Report Count (4)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        // Triggers
        0x05, 0x01,        // Usage Page (Generic Desktop)
        0x09, 0x34,        // Usage (Ry)
        0x09, 0x35,        // Usage (Rz)
        0x15, 0x00,        // Logical Minimum (0)
        0x26, 0xFF, 0x00,  // Logical Maximum (255)
        0x75, 0x08,        // Report Size (8)
        0x95, 0x02,        // Report Count (2)
        0x81, 0x02,        // Input (Data, Variable, Absolute)

    0xC0,              // End Collection

// 59 bytes

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

    Code had been tweaked and built upon to suit gamepad usage. Extra logging had been added to troubleshoot
    flakiness in bluetooth connection.
*/
void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event_type = hci_event_packet_get_type(packet);

    printf("[BTSTACK] event: 0x%02X\n", event_type);

    switch(event_type){
        case BTSTACK_EVENT_STATE:

            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;

            gap_local_bd_addr(local_addr);
            printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            bd_addr_t null_addr;
            memset(null_addr, 0, 6);

            gap_advertisements_set_params(0x0030, 0x0030, 0, 0, null_addr, 0x07, 0x00);
            assert(sizeof(adv_data) <= 31); // ble limitation
            gap_advertisements_set_data(sizeof(adv_data), (uint8_t*) adv_data);
            gap_advertisements_enable(1);

            printf("[BTSTACK] advertising...\n");

            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:

            printf("[HCI] Disconnected\n");
            printf("[HCI] Status: 0x%02X\n", hci_event_disconnection_complete_get_status(packet));
            printf("[HCI] Reason: 0x%02X\n", hci_event_disconnection_complete_get_reason(packet));

            con_handle = HCI_CON_HANDLE_INVALID;

            gap_advertisements_enable(1);

            break;

        case HCI_EVENT_LE_META:

            uint8_t le_event = hci_event_le_meta_get_subevent_code(packet);
            printf("[BLE] LE Meta subevent: 0x%02X\n", le_event);

            switch(le_event){

                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:{

                    uint8_t status = hci_subevent_le_connection_complete_get_status(packet);

                    printf("[BLE] status: 0x%02X\n", status);

                    if (status == ERROR_CODE_SUCCESS){
                        con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                        printf("[BLE] handle: 0x%04X\n", con_handle);
                        printf("[BLE] Connection established!\n");
                    }
                    else{
                        printf("[BLE] connection failure...\n");
                        con_handle = HCI_CON_HANDLE_INVALID;
                    }

                    break;}
                case 0x0A: {  // LE Enhanced Connection Complete

                    uint8_t status = packet[3];

                    con_handle = little_endian_read_16(packet, 4);

                    printf("[BLE] Connection status: 0x%02X\n", status);

                    if (status == ERROR_CODE_SUCCESS) {
                        printf("[BLE] Connection established!\n");
                        printf("[BLE] Handle: 0x%04X\n", con_handle);
                    } else {
                        printf("[BLE] Connection failed!\n");
                        con_handle = HCI_CON_HANDLE_INVALID;
                    }

                    break;}
                case HCI_SUBEVENT_LE_ADVERTISING_REPORT:
                    printf("[BLE] Advertising report received\n");
                    break;

                default:
                    printf("[BLE] Other LE subevent: 0x%02X\n", le_event);
                    break;
            }
            break;

        case ATT_EVENT_CAN_SEND_NOW:
            break;
        default:
            break;
    }
}

/*
    Configuration influenced by btstack hog_keyboard_demo.c
    https://github.com/bluekitchen/btstack/blob/master/example/hog_keyboard_demo.c
*/
void gamepad_init(void){
    
    l2cap_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    att_server_init(profile_data, NULL, NULL);

    //battery_service_server_init(battery);

    //device_information_service_server_init();

    hids_device_init(0, hid_descriptor_gamepad, sizeof(hid_descriptor_gamepad));

    printf("HIDS initialized\n");

    bd_addr_t null_addr;
    memset(null_addr, 0, 6);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    //sm_set_authentication_requirements(0);

    att_server_register_packet_handler(packet_handler);

    
    hids_device_register_packet_handler(packet_handler);

    //hci_dump_init();

    hci_power_control(HCI_POWER_ON);
}