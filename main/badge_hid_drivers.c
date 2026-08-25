/*
 * Turns what badgeteam/hid-host decodes into the reports this project prints and navigates by.
 *
 * SPDX-FileCopyrightText: 2024-2025 chegewara
 * SPDX-FileCopyrightText: 2025 Badge.Team
 *
 * SPDX-License-Identifier: MIT
 */
#include "badge_hid_drivers.h"
#include <string.h>
#include "esp_log.h"
#include "usb/hid.h"

static char const TAG[] = "BADGE_HID_DRIVER";

/// Buttons that fit in the named part of gamepad_report_t, before up/down/left/right
#define GAMEPAD_NAMED_BUTTONS 15

/// @brief Which button the descriptor's numbering means
///
/// HID numbers the buttons of a gamepad but never says what they are called, so this is read off
/// real hardware rather than out of a specification. It follows a Stadia controller, which names
/// all twenty of the numbers it uses and leaves no gaps. A pad that disagrees will land its
/// buttons in the wrong names; that is device knowledge, and the honest place for it is a quirk
/// keyed on vendor and product ID.
static const struct {
    uint8_t  usage;
    uint32_t bit;
} gamepad_button_names[] = {
    {1, 1u << 0},    // a
    {2, 1u << 1},    // b
    {4, 1u << 2},    // x
    {5, 1u << 3},    // y
    {11, 1u << 4},   // select
    {12, 1u << 5},   // start
    {14, 1u << 6},   // l1
    {15, 1u << 7},   // r1
    {7, 1u << 8},    // l2
    {8, 1u << 9},    // r2
    {20, 1u << 10},  // l3
    {19, 1u << 11},  // r3
    {13, 1u << 12},  // home
    {17, 1u << 13},  // l4
    {18, 1u << 14},  // r4
};

/// Mouse buttons that fit in mouse_report_t
#define MOUSE_BUTTONS 8

static hid_layout_t  mouse_layout = {0};
static hid_gamepad_t gamepad      = {0};

/// @brief Read an axis as a byte with 128 in the middle, whatever range the device reports in
///
/// Devices disagree about what a centered stick reads as: 128 on most, 0 on one that reports
/// signed. Both come out centered here, so the caller can subtract 128 and be done.
static uint8_t axis_to_byte(const uint8_t* data, int length, const hid_field_t* field) {
    if (!field->present) {
        return 128;
    }

    int32_t span = field->logical_max - field->logical_min;
    if (span <= 0) {
        return 128;
    }

    int32_t value = hid_layout_read(data, length, field);
    if (value < field->logical_min) {
        value = field->logical_min;
    }
    if (value > field->logical_max) {
        value = field->logical_max;
    }

    return (uint8_t)(((value - field->logical_min) * 255 + span / 2) / span);
}

/// @brief The right stick, which sits in z and rz on most pads and in rx and ry on the rest
static void right_stick(const hid_layout_t* layout, const hid_field_t** x, const hid_field_t** y) {
    *x = layout->z.present ? &layout->z : &layout->rx;
    *y = layout->rz.present ? &layout->rz : &layout->ry;
}

mouse_report_t parse_mouse_report(const uint8_t* const data, const int length, const hid_layout_t* layout) {
    mouse_report_t report = {0};

    if (layout == NULL || !layout->valid) {
        ESP_LOGW(TAG, "No layout for mouse!");
        return report;
    }

    const uint8_t* body = data;
    int            left = length;
    if (!hid_layout_strip_report_id(layout, &body, &left)) {
        // A report for some other part of the device, a battery level or a media key
        return report;
    }

    for (uint16_t button = 0; button < layout->button_count && button < MOUSE_BUTTONS; button++) {
        if (hid_layout_read_button(body, left, layout, button)) {
            report.buttons.val |= (uint8_t)(1u << button);
        }
    }

    report.x_displacement = (int16_t)hid_layout_read(body, left, &layout->x);
    report.y_displacement = (int16_t)hid_layout_read(body, left, &layout->y);
    report.scroll         = (int8_t)hid_layout_read(body, left, &layout->wheel);

    return report;
}

gamepad_report_t parse_gamepad_report(const uint8_t* const data, const int length, const hid_gamepad_t* pad) {
    gamepad_report_t report = {0};

    if (!hid_gamepad_is_open(pad)) {
        ESP_LOGW(TAG, "No layout for gamepad!");
        return report;
    }

    hid_gamepad_state_t state;
    if (!hid_gamepad_decode(pad, data, length, &state)) {
        // A report for some other part of the device
        return report;
    }

    report.report_id = pad->layout.report_id;

    // Buttons are named by the usage the descriptor gave them rather than by where they sit in the
    // report. A pad is free to list them in any order, and a Stadia controller lists them
    // backwards, so reading by position calls its Capture button "button one".
    report.buttons.val = 0;
    for (size_t i = 0; i < sizeof(gamepad_button_names) / sizeof(gamepad_button_names[0]); i++) {
        uint8_t usage = gamepad_button_names[i].usage;
        if (state.usage_buttons & ((uint32_t)1 << (usage - 1))) {
            report.buttons.val |= gamepad_button_names[i].bit;
        }
    }

    report.usage_buttons = state.usage_buttons;

    report.buttons.up    = state.up;
    report.buttons.down  = state.down;
    report.buttons.left  = state.left;
    report.buttons.right = state.right;

    report.dpad_up    = state.dpad_up;
    report.dpad_down  = state.dpad_down;
    report.dpad_left  = state.dpad_left;
    report.dpad_right = state.dpad_right;

    const uint8_t* body = data;
    int            left = length;
    if (!hid_layout_strip_report_id(&pad->layout, &body, &left)) {
        return report;
    }

    const hid_field_t* rx = NULL;
    const hid_field_t* ry = NULL;
    right_stick(&pad->layout, &rx, &ry);

    report.lx = axis_to_byte(body, left, &pad->layout.x);
    report.ly = axis_to_byte(body, left, &pad->layout.y);
    report.rx = axis_to_byte(body, left, rx);
    report.ry = axis_to_byte(body, left, ry);

    // Analog triggers, which a pad that names them reports as simulation controls
    report.lt = axis_to_byte(body, left, &pad->layout.brake);
    report.rt = axis_to_byte(body, left, &pad->layout.accelerator);

    return report;
}

mouse_report_t handle_mouse_event(const uint8_t* const data, const int length) {
    return parse_mouse_report(data, length, &mouse_layout);
}

gamepad_report_t handle_gamepad_event(const uint8_t* const data, const int length) {
    return parse_gamepad_report(data, length, &gamepad);
}

const hid_gamepad_quirk_t* badge_hid_gamepad_quirk(void) {
    return hid_gamepad_is_open(&gamepad) ? gamepad.quirk : NULL;
}

/// @brief Say where the controls of a mouse were found, for the log
static void log_mouse_layout(const hid_layout_t* layout) {
    ESP_LOGI(TAG, "Mouse layout: report id %d, %d buttons", layout->report_id, layout->button_count);
    if (layout->x.present) {
        ESP_LOGI(TAG, "  X: offset %u bits, size %u bits", layout->x.bit_offset, layout->x.bit_size);
    }
    if (layout->y.present) {
        ESP_LOGI(TAG, "  Y: offset %u bits, size %u bits", layout->y.bit_offset, layout->y.bit_size);
    }
    if (layout->wheel.present) {
        ESP_LOGI(TAG, "  Scroll: offset %u bits, size %u bits", layout->wheel.bit_offset, layout->wheel.bit_size);
    }
}

esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto,
                                            const uint16_t vid, const uint16_t pid) {
    if (HID_PROTOCOL_KEYBOARD == proto) {
        ESP_LOGI(TAG, "Keyboard uses generic (boot) driver");
        return ESP_OK;
    }

    if (HID_PROTOCOL_MOUSE == proto) {
        ESP_LOGI(TAG, "Mouse driver analysing");
        ESP_LOG_BUFFER_HEX(TAG, desc, desc_len);
        if (hid_layout_parse(desc, (size_t)desc_len, &mouse_layout)) {
            log_mouse_layout(&mouse_layout);
        } else {
            ESP_LOGW(TAG, "Could not parse mouse layout");
        }
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Gamepad driver analysing");
    ESP_LOG_BUFFER_HEX(TAG, desc, desc_len);
    if (!hid_gamepad_open(&gamepad, desc, (size_t)desc_len, vid, pid)) {
        ESP_LOGW(TAG, "Could not identify gamepad-compatible descriptor");
    }

    return ESP_OK;
}
