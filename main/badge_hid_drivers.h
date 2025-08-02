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

bool analyze_gamepad_layout(const uint8_t* desc, int desc_len, gamepad_field_layout_t* layout_out);
bool analyze_mouse_layout(const uint8_t* desc, int desc_len, mouse_field_layout_t* layout_out);

esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto);
