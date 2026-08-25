/*
 * HID host library for gamepad and mouse input devices.
 * Contains low-level helpers for parsing raw USB HID input reports.

 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Badge.Team
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include <stdio.h>
#include "badge_hid_drivers.h"
#include "esp_err.h"
#include "usb/hid_host.h"

/**
 * @brief APP event group
 *
 * Application logic can be different. There is a one among other ways to distinguish the
 * event by application event group.
 * In this example we have two event groups:
 * APP_EVENT            - General event, which is APP_QUIT_PIN press event (Generally, it is IO0).
 * APP_EVENT_HID_HOST   - HID Host Driver event, such as device connection/disconnection or input report.
 */
typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
    app_event_group_t event_group;
    /* HID Host - Device related info */
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t  event;
        void*                    arg;
    } hid_host_device;
} app_event_queue_t;

/**
 * @brief Key event
 */
typedef struct {
    enum key_state {
        KEY_STATE_PRESSED  = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

/* Main char symbol for ENTER key */
#define KEYBOARD_ENTER_MAIN_CHAR '\r'
/* When set to 1 pressing ENTER will be extending with LineFeed during serial debug output */
#define KEYBOARD_ENTER_LF_EXTEND 1

typedef struct {
    uint8_t report_id;
    mouse_report_t (*parse)(const uint8_t* data, int length);
} mouse_driver_t;

/// @brief The latest of what the connected devices reported
///
/// The events the BSP carries say which button was pressed, not where a stick is standing or how
/// far a mouse has travelled, so a display that wants to show that reads it from here.
typedef struct {
    uint32_t sequence;  ///< Bumped on every report, so a reader can tell that something changed

    bool     device_connected;
    uint16_t vid;
    uint16_t pid;
    char     protocol[16];  ///< "keyboard", "mouse" or what the device calls itself otherwise
    char     manufacturer[32];
    char     product[32];

    bool             gamepad_seen;
    gamepad_report_t gamepad;

    bool           mouse_seen;
    mouse_report_t mouse;
    int            mouse_x;  ///< Travelled since start, in whatever the mouse counts in
    int            mouse_y;
    int            mouse_scroll;
    int            mouse_tilt;
} badge_hid_state_t;

/// @brief Copy out the latest state
void badge_hid_get_state(badge_hid_state_t* out);

esp_err_t badge_hid_init(QueueHandle_t event_queue);
esp_err_t badge_hid_deinit(void);

void badge_hid_register_mouse_driver(const mouse_driver_t* driver);
void badge_hid_unregister_mouse_driver(void);