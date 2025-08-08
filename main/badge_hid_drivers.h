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

typedef struct {
    uint8_t      report_id;
    field_info_t x;
    field_info_t y;
    field_info_t scroll;
    field_info_t tilt;
    field_info_t buttons;
} mouse_field_layout_t;

typedef struct {
    uint8_t report_id;

    union {
        struct {
            uint32_t a : 1;
            uint32_t b : 1;
            uint32_t x : 1;
            uint32_t y : 1;

            uint32_t select : 1;
            uint32_t start  : 1;

            uint32_t l1 : 1;
            uint32_t r1 : 1;
            uint32_t l2 : 1;
            uint32_t r2 : 1;
            uint32_t l3 : 1;
            uint32_t r3 : 1;

            uint32_t home : 1;

            uint32_t l4 : 1;
            uint32_t r4 : 1;

            uint32_t up    : 1;
            uint32_t down  : 1;
            uint32_t left  : 1;
            uint32_t right : 1;

            uint32_t _reserved : 13;  // Up to 32 bits total
        };
        uint32_t val;
    } buttons;

    uint8_t lx, ly;
    uint8_t rx, ry;
    uint8_t lt, rt;
} gamepad_report_t;

typedef struct {
    union {
        struct {
            uint8_t button1  : 1;
            uint8_t button2  : 1;
            uint8_t button3  : 1;
            uint8_t reserved : 5;
        };
        uint8_t val;
    } buttons;
    int16_t x_displacement;
    int16_t y_displacement;
    int8_t  scroll;
    int8_t  tilt;
} mouse_report_t;

mouse_report_t   handle_mouse_event(const uint8_t* const data, const int length);
gamepad_report_t handle_gamepad_event(const uint8_t* const data, const int length);

mouse_report_t   parse_mouse_report(const uint8_t* const data, const int length, mouse_field_layout_t* layout);
gamepad_report_t parse_gamepad_report(const uint8_t* const data, const int length, gamepad_field_layout_t* layout);

esp_err_t analyze_mouse_layout(const uint8_t* desc, const int desc_len, mouse_field_layout_t* layout_out);
esp_err_t analyze_gamepad_layout(const uint8_t* desc, const int desc_len, gamepad_field_layout_t* layout_out);

esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto);