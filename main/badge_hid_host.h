#pragma once

#include <stdbool.h>
#include <stdio.h>

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

mouse_report_t parse_mouse_event(const uint8_t* const data, const int length);

gamepad_report_t parse_gamepad_report(const uint8_t* data, int length);
