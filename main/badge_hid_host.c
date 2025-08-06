/*
 * HID host library for gamepad and mouse input devices.
 * Contains low-level helpers for parsing raw USB HID input reports.

 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Badge.Team
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "badge_hid_host.h"
#include <stdio.h>
#include "badge_hid_drivers.h"
#include "bsp/input.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "usb/hid.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"
#include "usb/usb_host.h"

static char const    TAG[]           = "BADGE_HID_HOST";
static QueueHandle_t hid_event_queue = NULL;
static QueueHandle_t bsp_event_queue = NULL;

static const mouse_driver_t* active_mouse_driver = NULL;

static void print_report_descriptor(const uint8_t* const desc, const int desc_len) {
    if (!desc || desc_len == 0) {
        ESP_LOGW(TAG, "No HID report descriptor");
        return;
    }
    ESP_LOG_BUFFER_HEX(TAG, desc, desc_len);
}

static void send_navigation_event(bsp_input_navigation_key_t key, bool state, uint32_t modifiers) {
    bsp_input_event_t event = {
        .type                      = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation.key       = key,
        .args_navigation.modifiers = modifiers,
        .args_navigation.state     = state,
    };
    if (bsp_event_queue) {
        xQueueSend(bsp_event_queue, &event, 0);
    } else {
        ESP_LOGW(TAG, "No BSP event queue!");
    }
}

static void send_keyboard_event(char ascii, char const* utf8, uint32_t modifiers) {
    bsp_input_event_t event = {
        .type                    = INPUT_EVENT_TYPE_KEYBOARD,
        .args_keyboard.ascii     = ascii,
        .args_keyboard.utf8      = utf8,
        .args_keyboard.modifiers = modifiers,
    };
    if (bsp_event_queue) {
        xQueueSend(bsp_event_queue, &event, 0);
    } else {
        ESP_LOGW(TAG, "No BSP event queue!");
    }
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event,
                              void* arg) {
    const app_event_queue_t evt_queue = {.event_group            = APP_EVENT_HID_HOST,
                                         // HID Host Device related info
                                         .hid_host_device.handle = hid_device_handle,
                                         .hid_host_device.event  = event,
                                         .hid_host_device.arg    = arg};

    ESP_LOGI(TAG, "Event");

    if (hid_event_queue) {
        xQueueSend(hid_event_queue, &evt_queue, 0);
    } else {
        ESP_LOGW(TAG, "Event queue not found!");
    }
}

/**
 * @brief HID Protocol string names
 */
static const char* hid_proto_name_str[] = {"NONE", "KEYBOARD", "MOUSE"};

/**
 * @brief Sign-extends a 12-bit value to a 16-bit signed integer.
 *
 * Many HID mice encode high-resolution X/Y deltas using 12-bit signed values.
 * This function correctly extends them to usable 16-bit signed values.
 *
 * @param value A 12-bit unsigned value (lower 12 bits significant).
 * @return int16_t Signed version of the value.
 */
inline int16_t sign_extend_12bit(uint16_t value) {
    if (value & 0x800) {
        // If the 12th bit is set (negative number in 12-bit signed)
        return (int16_t)(value | 0xF000);  // Fill top 4 bits with 1s
    } else {
        return (int16_t)(value & 0x0FFF);  // Mask to 12 bits
    }
}

/**
 * @brief Parses a mouse input report into a structured format.
 *
 * Supports both boot protocol reports (4 bytes) and extended HID reports.
 *
 * @param data Raw pointer to HID report data.
 * @param length Length of the report in bytes.
 * @return mouse_report_t Parsed report with movement and button states.
 */
static mouse_report_t parse_mouse_event(const uint8_t* const data, const int length) {
    mouse_report_t mouse_report = {0};

    if (length <= 4) {
        hid_mouse_input_report_boot_t* boot_mouse_report = (hid_mouse_input_report_boot_t*)data;
        mouse_report.x_displacement                      = boot_mouse_report->x_displacement;
        mouse_report.y_displacement                      = boot_mouse_report->y_displacement;
        mouse_report.buttons.val                         = boot_mouse_report->buttons.val;
        if (length == 3) {
            mouse_report.scroll = data[4];
        }
    } else if (length == 5) {
        // Modern Logitech
        mouse_report.buttons.val    = data[0];
        mouse_report.x_displacement = (int8_t)data[1];
        mouse_report.y_displacement = (int8_t)data[2];
        mouse_report.scroll         = (int8_t)data[3];
        mouse_report.tilt           = (int8_t)data[4];
    } else if (length < 9) {
        mouse_report.buttons.val    = data[1];
        mouse_report.x_displacement = sign_extend_12bit((data[4] & 0x0F) << 8) | data[3];
        mouse_report.y_displacement = sign_extend_12bit(data[5] << 4) | (data[4] >> 4);
        mouse_report.scroll         = (int8_t)data[6];
        if (length == 8) {
            mouse_report.tilt = (int8_t)data[7];
        }
    } else {
        mouse_report.buttons.val    = data[1];
        mouse_report.x_displacement = (int16_t)((data[4] << 8) | data[3]);
        mouse_report.y_displacement = (int16_t)((data[6] << 8) | data[5]);
        mouse_report.scroll         = (int8_t)data[7];
        mouse_report.tilt           = (int8_t)data[8];
    }

    return mouse_report;
}

/**
 * @brief USB HID Host Mouse Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_mouse_report_callback(const uint8_t* const data, const int length) {
    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        ESP_LOGW(TAG, "Mouse report too short (%i)", length);
        return;
    }

    mouse_report_t mouse_report = parse_mouse_event(data, length);

    static int x_pos    = 0;
    static int y_pos    = 0;
    static int x_scroll = 0;
    static int y_scroll = 0;

    // Calculate absolute position from displacement
    x_pos += mouse_report.x_displacement;
    y_pos += mouse_report.y_displacement;

    x_scroll += mouse_report.tilt;
    y_scroll += mouse_report.scroll;

    if (mouse_report.x_displacement > 100) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_RIGHT, 1, 0);
    } else if (mouse_report.x_displacement < -100) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_LEFT, 1, 0);
    }

    if (mouse_report.y_displacement > 100) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_DOWN, 1, 0);
    } else if (mouse_report.y_displacement < -100) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_UP, 1, 0);
    }

    if (mouse_report.scroll > 1) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_PGUP, 1, 0);
    } else if (mouse_report.scroll < -1) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_PGDN, 1, 0);
    }

    if (mouse_report.buttons.button1) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A, 1, 0);
    }
    if (mouse_report.buttons.button2) {
        send_navigation_event(BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B, 1, 0);
    }

    ESP_LOGD(TAG, "Mouse X: %06d\tY: %06d\t|%c|%c|%c| Scroll: %03d Tilt: %03d", x_pos, y_pos,
             (mouse_report.buttons.button1 ? 'o' : ' '), (mouse_report.buttons.button3 ? 'o' : ' '),
             (mouse_report.buttons.button2 ? 'o' : ' '), x_scroll, y_scroll);
}

/**
 * @brief Parses a gamepad HID report into the standard format.
 *
 * This function should be implemented per controller type (e.g., PS4, Xbox).
 * It fills out the gamepad_report_t with button and axis values.
 *
 * @param data Raw HID report data.
 * @param length Report length in bytes.
 * @return gamepad_report_t
 */
gamepad_report_t parse_gamepad_report(const uint8_t* data, int length) {
    gamepad_report_t rpt = {0};

    if (length < 10) return rpt;

    rpt.report_id = data[0];

    uint8_t hat = data[1];
    uint8_t b1  = data[2];
    uint8_t b2  = data[3];

    rpt.buttons.val = 0;

    rpt.buttons.up    = (hat == 0x00 || hat == 0x01 || hat == 0x07);
    rpt.buttons.right = (hat == 0x01 || hat == 0x02 || hat == 0x03);
    rpt.buttons.down  = (hat == 0x03 || hat == 0x04 || hat == 0x05);
    rpt.buttons.left  = (hat == 0x05 || hat == 0x06 || hat == 0x07);

    // Face buttons
    rpt.buttons.a = (b2 >> 6) & 1;
    rpt.buttons.b = (b2 >> 5) & 1;
    rpt.buttons.x = (b2 >> 4) & 1;
    rpt.buttons.y = (b2 >> 3) & 1;

    // Thumbsticks
    rpt.buttons.l1 = (b2 >> 0) & 1;
    rpt.buttons.r1 = (b1 >> 7) & 1;

    // Shoulders and triggers
    rpt.buttons.l2 = (b2 >> 2) & 1;
    rpt.buttons.r2 = (b2 >> 1) & 1;
    rpt.buttons.l3 = (b1 >> 2) & 1;
    rpt.buttons.r3 = (b1 >> 3) & 1;

    // Extra buttons
    rpt.buttons.l4     = (b1 >> 1) & 1;
    rpt.buttons.r4     = (b1 >> 0) & 1;
    rpt.buttons.select = (b1 >> 6) & 1;
    rpt.buttons.start  = (b1 >> 5) & 1;
    rpt.buttons.home   = (b1 >> 4) & 1;

    rpt.lx = data[4];
    rpt.ly = data[5];
    rpt.rx = data[6];
    rpt.ry = data[7];
    rpt.lt = data[8];
    rpt.rt = data[9];

    return rpt;
}

static void print_gamepad_report(const gamepad_report_t* rpt, int length) {
    char line1[64], line2[64], button_line[128];

    snprintf(line1, sizeof(line1), "Report ID: 0x%02X | Length: %2d", rpt->report_id, length);
    snprintf(line2, sizeof(line2), "Axes: LX=%3d LY=%3d RX=%3d RY=%3d LT=%3d RT=%3d", rpt->lx, rpt->ly, rpt->rx,
             rpt->ry, rpt->lt, rpt->rt);

    const char* btn_labels[] = {"A",  "B",  "X",      "Y",     "L1",   "R1",   "L2",    "R2", "L3",  "R3",
                                "L4", "R4", "Select", "Start", "Home", "Left", "Right", "Up", "Down"};
    const bool  btn_states[] = {rpt->buttons.a,      rpt->buttons.b,     rpt->buttons.x,    rpt->buttons.y,
                                rpt->buttons.l1,     rpt->buttons.r1,    rpt->buttons.l2,   rpt->buttons.r2,
                                rpt->buttons.l3,     rpt->buttons.r3,    rpt->buttons.l4,   rpt->buttons.r4,
                                rpt->buttons.select, rpt->buttons.start, rpt->buttons.home, rpt->buttons.left,
                                rpt->buttons.right,  rpt->buttons.up,    rpt->buttons.down};

    button_line[0] = '\0';
    strcat(button_line, "Buttons:");
    for (int i = 0; i < sizeof(btn_labels) / sizeof(btn_labels[0]); i++) {
        if (btn_states[i]) {
            strcat(button_line, " ");
            strcat(button_line, btn_labels[i]);
        }
    }

    ESP_LOGD(TAG, "%s", button_line);
    ESP_LOGD(TAG, "%s", line1);
    ESP_LOGD(TAG, "%s", line2);
}

/**
 * @brief USB HID Host Generic Interface report callback handler
 *
 * 'generic' means anything else than mouse or keyboard
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_generic_report_callback(const uint8_t* const data, const int length) {
    ESP_LOGI(TAG, "Received generic report (%d bytes)", length);
    if (length >= 10) {
        gamepad_report_t rpt = parse_gamepad_report(data, length);

        int lx = ((int)rpt.lx - 128);
        int ly = ((int)rpt.ly - 128);

        if (lx > 50) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_RIGHT, 1, 0);
        } else if (lx < -50) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_LEFT, 1, 0);
        }
        if (ly > 50) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_UP, 1, 0);
        } else if (ly < -50) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_DOWN, 1, 0);
        }

        if (rpt.buttons.up) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_UP, 1, 0);
        }
        if (rpt.buttons.down) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_DOWN, 1, 0);
        }
        if (rpt.buttons.left) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_LEFT, 1, 0);
        }
        if (rpt.buttons.right) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_RIGHT, 1, 0);
        }

        if (rpt.buttons.a) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A, 1, 0);
        }
        if (rpt.buttons.b) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B, 1, 0);
        }
        if (rpt.buttons.x) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_GAMEPAD_X, 1, 0);
        }
        if (rpt.buttons.y) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y, 1, 0);
        }

        if (rpt.buttons.start) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_START, 1, 0);
        }
        if (rpt.buttons.select) {
            send_navigation_event(BSP_INPUT_NAVIGATION_KEY_SELECT, 1, 0);
        }

        print_gamepad_report(&rpt, length);
    } else {
        ESP_LOGW(TAG, "Received too-short report (%d bytes)", length);
    }
}

/**
 * @brief Scancode to ascii table
 */
const uint8_t keycode2ascii[57][2] = {
    {0, 0},                                               /* HID_KEY_NO_PRESS        */
    {0, 0},                                               /* HID_KEY_ROLLOVER        */
    {0, 0},                                               /* HID_KEY_POST_FAIL       */
    {0, 0},                                               /* HID_KEY_ERROR_UNDEFINED */
    {'a', 'A'},                                           /* HID_KEY_A               */
    {'b', 'B'},                                           /* HID_KEY_B               */
    {'c', 'C'},                                           /* HID_KEY_C               */
    {'d', 'D'},                                           /* HID_KEY_D               */
    {'e', 'E'},                                           /* HID_KEY_E               */
    {'f', 'F'},                                           /* HID_KEY_F               */
    {'g', 'G'},                                           /* HID_KEY_G               */
    {'h', 'H'},                                           /* HID_KEY_H               */
    {'i', 'I'},                                           /* HID_KEY_I               */
    {'j', 'J'},                                           /* HID_KEY_J               */
    {'k', 'K'},                                           /* HID_KEY_K               */
    {'l', 'L'},                                           /* HID_KEY_L               */
    {'m', 'M'},                                           /* HID_KEY_M               */
    {'n', 'N'},                                           /* HID_KEY_N               */
    {'o', 'O'},                                           /* HID_KEY_O               */
    {'p', 'P'},                                           /* HID_KEY_P               */
    {'q', 'Q'},                                           /* HID_KEY_Q               */
    {'r', 'R'},                                           /* HID_KEY_R               */
    {'s', 'S'},                                           /* HID_KEY_S               */
    {'t', 'T'},                                           /* HID_KEY_T               */
    {'u', 'U'},                                           /* HID_KEY_U               */
    {'v', 'V'},                                           /* HID_KEY_V               */
    {'w', 'W'},                                           /* HID_KEY_W               */
    {'x', 'X'},                                           /* HID_KEY_X               */
    {'y', 'Y'},                                           /* HID_KEY_Y               */
    {'z', 'Z'},                                           /* HID_KEY_Z               */
    {'1', '!'},                                           /* HID_KEY_1               */
    {'2', '@'},                                           /* HID_KEY_2               */
    {'3', '#'},                                           /* HID_KEY_3               */
    {'4', '$'},                                           /* HID_KEY_4               */
    {'5', '%'},                                           /* HID_KEY_5               */
    {'6', '^'},                                           /* HID_KEY_6               */
    {'7', '&'},                                           /* HID_KEY_7               */
    {'8', '*'},                                           /* HID_KEY_8               */
    {'9', '('},                                           /* HID_KEY_9               */
    {'0', ')'},                                           /* HID_KEY_0               */
    {KEYBOARD_ENTER_MAIN_CHAR, KEYBOARD_ENTER_MAIN_CHAR}, /* HID_KEY_ENTER           */
    {0, 0},                                               /* HID_KEY_ESC             */
    {'\b', 0},                                            /* HID_KEY_DEL             */
    {0, 0},                                               /* HID_KEY_TAB             */
    {' ', ' '},                                           /* HID_KEY_SPACE           */
    {'-', '_'},                                           /* HID_KEY_MINUS           */
    {'=', '+'},                                           /* HID_KEY_EQUAL           */
    {'[', '{'},                                           /* HID_KEY_OPEN_BRACKET    */
    {']', '}'},                                           /* HID_KEY_CLOSE_BRACKET   */
    {'\\', '|'},                                          /* HID_KEY_BACK_SLASH      */
    {'\\', '|'},
    /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
    {';', ':'},                    /* HID_KEY_COLON           */
    {'\'', '"'},                   /* HID_KEY_QUOTE           */
    {'`', '~'},                    /* HID_KEY_TILDE           */
    {',', '<'},                    /* HID_KEY_LESS            */
    {'.', '>'},                    /* HID_KEY_GREATER         */
    {'/', '?'}                     /* HID_KEY_SLASH           */
};

/**
 * @brief HID Keyboard modifier verification for capitalization application (right or left shift)
 *
 * @param[in] modifier
 * @return true  Modifier was pressed (left or right shift)
 * @return false Modifier was not pressed (left or right shift)
 *
 */
static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier) {
    if (((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) || ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT)) {
        return true;
    }
    return false;
}

/**
 * @brief HID Keyboard get char symbol from key code
 *
 * @param[in] modifier  Keyboard modifier data
 * @param[in] key_code  Keyboard key code
 * @param[in] key_char  Pointer to key char data
 *
 * @return true  Key scancode converted successfully
 * @return false Key scancode unknown
 */
static inline bool hid_keyboard_get_char(uint8_t modifier, uint8_t key_code, unsigned char* key_char) {
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;

    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
        *key_char = keycode2ascii[key_code][mod];
    } else {
        // All other key pressed
        return false;
    }

    return true;
}

/**
 * @brief Key Event. Key event with the key code, state and modifier.
 *
 * @param[in] key_event Pointer to Key Event structure
 *
 */
static void key_event_callback(key_event_t* key_event) {
    unsigned char key_char;

    if (KEY_STATE_PRESSED == key_event->state) {
        if (hid_keyboard_get_char(key_event->modifier, key_event->key_code, &key_char)) {

            ESP_LOGI(TAG, "Keyboard event %c (%02X)", key_char, key_event->key_code);

            // send_keyboard_event(key_char, NULL, key_event->modifier);
        }
    }
}

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t* const src, uint8_t key, unsigned int length) {
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

static void hid_host_keyboard_report_callback(const uint8_t* data, size_t length) {
    hid_keyboard_input_report_boot_t* kb_report = (hid_keyboard_input_report_boot_t*)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        ESP_LOGW(TAG, "Keyboard report too small!");
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = {0};
    key_event_t    key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {

        // key has been released verification
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED && !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state    = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        // key has been pressed verification
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state    = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
static void hid_host_interface_callback(hid_host_device_handle_t         hid_device_handle,
                                        const hid_host_interface_event_t event, void* arg) {
    uint8_t               data[64]    = {0};
    size_t                data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            ESP_LOGD(TAG, "HID Device, protocol '%s' INPUT_REPORT", hid_proto_name_str[dev_params.proto]);
            ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle, data, 64, &data_length));

            // print_report_descriptor(hid_device_handle);

            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    hid_host_keyboard_report_callback(data, data_length);
                } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                    hid_host_mouse_report_callback(data, data_length);
                }
            } else {
                hid_host_generic_report_callback(data, data_length);
            }

            break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED", hid_proto_name_str[dev_params.proto]);
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
            break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR", hid_proto_name_str[dev_params.proto]);
            break;
        default:
            ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event", hid_proto_name_str[dev_params.proto]);
            break;
    }
}

// Function to convert wide string to regular UTF-8 (if needed)
static void utf16le_to_ascii(char* dest, const wchar_t* src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len - 1 && src[i]; i++) {
        uint16_t ch = (uint16_t)src[i];
        dest[i]     = (ch < 128) ? (char)ch : '?';  // replace non-ASCII with '?'
    }
    dest[i] = '\0';
}

void badge_hid_register_mouse_driver(const mouse_driver_t* driver) {
    active_mouse_driver = driver;
}

void badge_hid_unregister_mouse_driver(void) {
    active_mouse_driver = NULL;
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
static void hid_host_device_event(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event,
                                  void* arg) {
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);

            const hid_host_device_config_t dev_config = {.callback = hid_host_interface_callback, .callback_arg = NULL};
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));

            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {  // Luxury mouse support
                    ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_REPORT));
                }
            }
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));

            hid_host_dev_info_t info;
            ESP_ERROR_CHECK(hid_host_get_device_info(hid_device_handle, &info));

            if (ESP_LOG_ENABLED(ESP_LOG_INFO)) {

                char manufacturer[HID_STR_DESC_MAX_LENGTH];
                char product[HID_STR_DESC_MAX_LENGTH];
                utf16le_to_ascii(manufacturer, info.iManufacturer, HID_STR_DESC_MAX_LENGTH);
                utf16le_to_ascii(product, info.iProduct, HID_STR_DESC_MAX_LENGTH);

                ESP_LOGI(TAG, "VID:PID %04X:%04X\tManufacturer: %s\tProduct: %s", info.VID, info.PID, manufacturer,
                         product);
            }

            size_t         desc_len = 0;
            const uint8_t* desc     = hid_host_get_report_descriptor(hid_device_handle, &desc_len);

            ESP_ERROR_CHECK(decode_descriptor_register_driver(desc, desc_len, dev_params.proto));

            if (ESP_LOG_ENABLED(ESP_LOG_DEBUG)) {
                print_report_descriptor(desc, desc_len);
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events
 *
 * @param[in] arg  Not used
 */
static void hid_client_task(void* arg) {
    app_event_queue_t evt_queue;

    xTaskNotifyGive(arg);

    ESP_LOGI(TAG, "HID Driver installed");

    while (true) {
        if (xQueueReceive(hid_event_queue, &evt_queue, portMAX_DELAY)) {
            if (APP_EVENT == evt_queue.event_group) {
                // User pressed button
                usb_host_lib_info_t lib_info;
                ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
                if (lib_info.num_devices == 0) {
                    // End while cycle
                    break;
                } else {
                    ESP_LOGW(TAG, "To shutdown HID, remove all USB devices and try again.");
                    // Keep polling
                }
            }

            if (APP_EVENT_HID_HOST == evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle, evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);
            }
        }
    }

    ESP_LOGI(TAG, "HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    xQueueReset(hid_event_queue);
    vQueueDelete(hid_event_queue);
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void* arg) {
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");
    // Clean up USB Host
    vTaskDelay(10);  // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 *
 */
esp_err_t badge_hid_init(QueueHandle_t event_queue) {
    bsp_event_queue = event_queue;
    BaseType_t task_created;

    /*
     * Create usb_lib_task to:
     * - initialize USB Host library
     */
    task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    assert(task_created == pdTRUE);

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, 1000);

    /*
     * HID host driver configuration
     * - create background task for handling low level event inside the HID driver
     * - provide the device callback to get new HID Device connection event
     */
    const hid_host_driver_config_t hid_host_driver_config = {.create_background_task = true,
                                                             .task_priority          = 5,
                                                             .stack_size             = 4096,
                                                             .core_id                = 0,
                                                             .callback               = hid_host_device_callback,
                                                             .callback_arg           = NULL};

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    hid_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

    task_created =
        xTaskCreatePinnedToCore(hid_client_task, "hid_events", 4096, xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Subsystem initialized");

    return ESP_OK;
}

esp_err_t badge_hid_deinit(void) {
    usb_host_lib_info_t lib_info;
    ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
    if (lib_info.num_devices == 0) {
        ESP_LOGW(TAG, "To shutdown driver, remove all USB devices and try again.");
    }
    // ESP_ERROR_CHECK(hid_host_unregister_callbacks());
    ESP_ERROR_CHECK(hid_host_uninstall());
    ESP_ERROR_CHECK(usb_host_uninstall());
    hid_event_queue = NULL;
    return ESP_OK;
}
