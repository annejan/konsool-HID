/*
 * HID host library for gamepad and mouse input devices.
 *
 * SPDX-FileCopyrightText: 2024-2025 chegewara
 * SPDX-FileCopyrightText: 2025 Badge.Team
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// HID Item Types
#define HID_TYPE_MAIN   0x0
#define HID_TYPE_GLOBAL 0x1
#define HID_TYPE_LOCAL  0x2

// HID Main Item Tags
#define HID_TAG_INPUT          0x8
#define HID_TAG_COLLECTION     0xA
#define HID_TAG_END_COLLECTION 0xC

// HID Global Item Tags
#define HID_TAG_USAGE_PAGE   0x0
#define HID_TAG_REPORT_SIZE  0x7
#define HID_TAG_REPORT_COUNT 0x9
#define HID_TAG_REPORT_ID    0x8

// HID Local Item Tags
#define HID_TAG_USAGE     0x0
#define HID_TAG_USAGE_MIN 0x1
#define HID_TAG_USAGE_MAX 0x2

// HID Special Item
#define HID_LONG_ITEM_PREFIX 0xFE

// Usage Pages
#define USAGE_PAGE_GENERIC_DESKTOP 0x01
#define USAGE_PAGE_BUTTON          0x09
#define USAGE_PAGE_CONSUMER        0x0C

// Usages
#define USAGE_X             0x30
#define USAGE_Y             0x31
#define USAGE_WHEEL         0x38
#define USAGE_TILT          0x48
#define USAGE_CONSUMER_TILT 0x0238

// HID Item Types
#define HID_TYPE_MAIN   0x0
#define HID_TYPE_GLOBAL 0x1
#define HID_TYPE_LOCAL  0x2

// HID Main Item Tags
#define HID_TAG_INPUT          0x8
#define HID_TAG_COLLECTION     0xA
#define HID_TAG_END_COLLECTION 0xC

// HID Global Item Tags
#define HID_TAG_USAGE_PAGE   0x0
#define HID_TAG_REPORT_SIZE  0x7
#define HID_TAG_REPORT_COUNT 0x9
#define HID_TAG_REPORT_ID    0x8

// HID Local Item Tags
#define HID_TAG_USAGE     0x0
#define HID_TAG_USAGE_MIN 0x1
#define HID_TAG_USAGE_MAX 0x2

// HID Special Item
#define HID_LONG_ITEM_PREFIX 0xFE

// Usage Pages
#define USAGE_PAGE_GENERIC_DESKTOP 0x01
#define USAGE_PAGE_SIMULATION      0x02
#define USAGE_PAGE_BUTTON          0x09
#define USAGE_PAGE_CONSUMER        0x0C

// Generic Desktop Usages
#define USAGE_X         0x30
#define USAGE_Y         0x31
#define USAGE_Z         0x32
#define USAGE_RX        0x33
#define USAGE_RY        0x34
#define USAGE_RZ        0x35
#define USAGE_HATSWITCH 0x39
#define USAGE_WHEEL     0x38
#define USAGE_TILT      0x48

// Simulation Control Usages
#define USAGE_ACCELERATOR 0xC4
#define USAGE_BRAKE       0xC5

// Consumer Control Usages
#define USAGE_CONSUMER_TILT 0x0238

// Button Usage Range
#define USAGE_BUTTON_MIN 0x01
#define USAGE_BUTTON_MAX 0x20

typedef struct {
    bool     present;
    uint16_t offset;
    uint8_t  size;
} field_info_t;

typedef struct {
    uint8_t      report_id;
    field_info_t dpad;
    field_info_t buttons;
    field_info_t lx, ly;
    field_info_t rx, ry;
    field_info_t lt, rt;
} gamepad_field_layout_t;

// Mouse layout: X, Y, Wheel, Buttons up to 8
// Buttons are bit-packed after axes

typedef struct {
    uint8_t      report_id;
    field_info_t x;
    field_info_t y;
    field_info_t scroll;
    field_info_t tilt;
    field_info_t buttons;  // count via size: number of bits
} mouse_field_layout_t;

esp_err_t analyze_mouse_layout(const uint8_t* desc, int desc_len, mouse_field_layout_t* layout_out);
esp_err_t analyze_gamepad_layout(const uint8_t* desc, int desc_len, gamepad_field_layout_t* layout_out);

esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto);