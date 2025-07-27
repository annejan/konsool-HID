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
#include "esp_err.h"
#include "usb/hid_host.h"

#define MAX_FIELDS 32
#define MAX_USAGES 32

#define USAGE_PAGE_GENERIC_DESKTOP 0x01
#define USAGE_PAGE_BUTTON          0x09
#define USAGE_PAGE_CONSUMER        0x0C

typedef struct {
    uint16_t usage_page;
    uint16_t usage_ids[MAX_USAGES];
    uint8_t  count;
    uint8_t  size;
    uint16_t offset;
    bool     relative;
} hid_field_info;

typedef struct {
    hid_field_info fields[MAX_FIELDS];
    size_t         num_fields;
} hid_report_descriptor;
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

esp_err_t badge_hid_init(QueueHandle_t event_queue);
esp_err_t badge_hid_deinit(void);
