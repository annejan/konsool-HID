/*
 * Turns what badgeteam/hid-host decodes into the reports this project prints and navigates by.
 *
 * The report descriptor parsing itself lives in the hid-host component. What is left here is the
 * part that is about this project: named buttons, byte sized axes and a report the rest of the
 * firmware already knows how to read.
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
#include "hid_gamepad.h"
#include "hid_layout.h"

/// @brief One gamepad report, with the buttons named the way this project uses them
///
/// Buttons come out of the device in the order it reports them, so button one lands in a, button
/// two in b and so on. What they are labelled on the pad is anyone's guess; the names are what
/// this firmware calls them. Buttons past the fifteenth do not fit and are dropped.
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

    /// The directions with the sticks left out, so a drawing of the pad can light the d-pad only
    /// when the d-pad is what moved. The bitfield above merges all three sources, which is what
    /// navigating wants.
    bool dpad_up, dpad_down, dpad_left, dpad_right;

    /// Sticks, scaled to a byte with 128 in the middle whatever range the device reports in.
    /// An axis the device does not have reads as centered rather than as pushed hard one way.
    uint8_t lx, ly;
    uint8_t rx, ry;

    /// Analog triggers. The Simulation page they live on is not parsed yet, so these read zero
    /// on every device for now.
    uint8_t lt, rt;

    /// Every button that is down, numbered the way the descriptor numbers them: bit n is set when
    /// the pad calls that one Button n+1. Handy for working out what an unfamiliar pad calls its
    /// buttons, and for anything the names above have no room for.
    uint32_t usage_buttons;
} gamepad_report_t;

/// @brief One mouse report
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
    /// Horizontal wheel. Mice report it through Consumer AC Pan, which is not parsed yet, so
    /// this reads zero for now.
    int8_t  tilt;
} mouse_report_t;

/// @brief Decode one report with the layout that was worked out when the device connected
mouse_report_t   handle_mouse_event(const uint8_t* const data, const int length);
gamepad_report_t handle_gamepad_event(const uint8_t* const data, const int length);

/// @brief Decode one report with a layout of your own, which is what the tests do
mouse_report_t   parse_mouse_report(const uint8_t* const data, const int length, const hid_layout_t* layout);
gamepad_report_t parse_gamepad_report(const uint8_t* const data, const int length, const hid_gamepad_t* gamepad);

/// @brief Work out how to read the reports of a device that just connected
///
/// The vendor and product ID are what the quirk table is keyed on, so a device that needs a nudge
/// before it says anything can be recognised. Pass zero for both when they are not known.
esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto,
                                            const uint16_t vid, const uint16_t pid);

/// @brief The quirks of the gamepad that is connected, NULL when it needs none or none is
///
/// The caller holds the USB handle, so sending the feature report that gets a device talking is
/// its job rather than this one's.
const hid_gamepad_quirk_t* badge_hid_gamepad_quirk(void);
