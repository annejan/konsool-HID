// badge_hid_host.c
//
// HID host report parser for gamepad and mouse input devices.
// Contains low-level helpers for parsing raw USB HID input reports.

#include "badge_hid_host.h"
#include "usb/hid_usage_mouse.h"

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
mouse_report_t parse_mouse_event(const uint8_t* const data, const int length) {
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
 * @brief Parses a gamepad HID report into the standard format.
 *
 * This function should be implemented per controller type (e.g., PS4, Xbox).
 * It fills out the gamepad_report_t with button and axis values.
 *
 * @param rpt Pointer to the report struct to populate.
 * @param data Raw HID report data.
 * @param length Report length in bytes.
 * @return true if parsing was successful; false otherwise.
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