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

typedef struct {
    bool has_buttons;
    bool has_x;
    bool has_y;
    bool has_scroll;
    bool has_tilt;

    uint8_t report_id;

    uint16_t button_bit_offset;
    uint8_t  button_bit_count;

    uint16_t x_bit_offset;
    uint8_t  x_bit_size;

    uint16_t y_bit_offset;
    uint8_t  y_bit_size;

    uint16_t scroll_bit_offset;
    uint8_t  scroll_bit_size;

    uint16_t tilt_bit_offset;
    uint8_t  tilt_bit_size;
} mouse_field_layout_t;

typedef struct {
    bool has_report_id;

    bool     has_dpad;
    uint16_t dpad_bit_offset;
    uint8_t  dpad_bit_size;

    bool     has_buttons;
    uint16_t button_bit_offset;
    uint8_t  button_bit_count;

    bool     has_lx;
    uint16_t lx_bit_offset;
    uint8_t  lx_bit_size;

    bool     has_ly;
    uint16_t ly_bit_offset;
    uint8_t  ly_bit_size;

    bool     has_rx;
    uint16_t rx_bit_offset;
    uint8_t  rx_bit_size;

    bool     has_ry;
    uint16_t ry_bit_offset;
    uint8_t  ry_bit_size;

    bool     has_lt;
    uint16_t lt_bit_offset;
    uint8_t  lt_bit_size;

    bool     has_rt;
    uint16_t rt_bit_offset;
    uint8_t  rt_bit_size;
} gamepad_field_layout_t;

// Structure to hold parsed HID field information
typedef struct {
    const char* name;           ///< Field name (e.g., "button", "axis")
    uint8_t     report_id;      ///< Report ID associated with this field
    uint8_t*    data;           ///< Pointer to first byte of value in report data
    uint16_t    offset;         ///< Bit offset in report (0-n)
    uint8_t     size;           ///< Size in bits (1-32)
    uint8_t     count;          ///< Number of items in an array (e.g., 5 buttons)
    uint16_t    usage_page;     ///< Usage page ID
    uint16_t    usage_ids[32];  ///< Store up to 32 usages per field
    uint16_t    usage_id;       ///< Usage ID within the page
    const char* usage_name;     ///< Human-readable name of the usage

    union {
        struct {
            uint8_t data      : 1;  ///< Constant value (1) or variable (0)
            uint8_t array     : 1;  ///< Array of values (1) or single value (0)
            uint8_t relative  : 1;  ///< Relative value
            uint8_t wrap      : 1;  ///< Wraps around (e.g., hat switch)
            uint8_t nonlinear : 1;  ///< Non-linear mapping
            uint8_t preferred : 1;  ///< Preferred state
            uint8_t null      : 1;  ///< No null position
            uint8_t dummy     : 1;  ///< Unused bit
        };
        uint8_t val;
    } flags;

    struct {
        int32_t min;
        int32_t max;
    } logic_range;

    struct {
        uint32_t min;
        uint32_t max;
    } usage_range;
} hid_field_info_t;

typedef struct {
    size_t            num_fields;
    hid_field_info_t* fields;
} hid_report_descriptor_t;

esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto);