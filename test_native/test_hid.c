// Runs recorded devices through the mapping layer and checks the reports that come out.
//
// Working out where the controls sit in a report is the hid-host component's job and is tested
// there. What is checked here is the part that belongs to this project: that the fields land in
// the right places of gamepad_report_t and mouse_report_t.

#include <assert.h>
#include <stdio.h>
#include "badge_hid_drivers.h"
#include "esp_log.h"
#include "test_descriptors.h"
#include "test_reports.h"
#include "usb/hid.h"

static char const TAG[] = "HID Test";

// A device nothing is known about beyond its report descriptor
#define UNKNOWN_VID 0x0000
#define UNKNOWN_PID 0x0000

static void test_logitech_m705(void) {
    hid_layout_t layout;
    assert(hid_layout_parse(mouse1_desc, mouse1_len, &layout));

    // Its input reports carry a report ID, and the offsets do not count it
    assert(2 == layout.report_id);
    assert(16 == layout.button_count);
    assert(0 == layout.buttons.bit_offset);
    assert(16 == layout.x.bit_offset && 12 == layout.x.bit_size);
    assert(28 == layout.y.bit_offset && 12 == layout.y.bit_size);
    assert(40 == layout.wheel.bit_offset && 8 == layout.wheel.bit_size);

    ESP_LOGI(TAG, "Logitech M705 layout passed");

    mouse_report_t report = parse_mouse_report(mouse1_reports[0], 8, &layout);
    assert(0 == report.buttons.val);
    assert(0 == report.x_displacement);
    assert(0 == report.y_displacement);
    assert(-2 == report.scroll);

    report = parse_mouse_report(mouse1_reports[1], 8, &layout);
    assert(0 == report.buttons.val);
    assert(-1 == report.x_displacement);
    assert(4 == report.y_displacement);
    assert(0 == report.scroll);

    report = parse_mouse_report(mouse1_reports[2], 8, &layout);
    assert(0 == report.buttons.val);
    assert(66 == report.x_displacement);
    assert(40 == report.y_displacement);
    assert(0 == report.scroll);

    // Both axes hard against their stops, twelve bits of them
    report = parse_mouse_report(mouse1_reports[3], 8, &layout);
    assert(-2047 == report.x_displacement);
    assert(-2047 == report.y_displacement);

    report = parse_mouse_report(mouse1_reports[4], 8, &layout);
    assert(-1 == report.x_displacement);
    assert(-1 == report.y_displacement);

    report = parse_mouse_report(mouse1_reports[5], 8, &layout);
    assert(1 == report.x_displacement);
    assert(1 == report.y_displacement);
    assert(-127 == report.scroll);

    report = parse_mouse_report(mouse1_reports[6], 8, &layout);
    assert(2047 == report.x_displacement);
    assert(2047 == report.y_displacement);
    assert(127 == report.scroll);

    ESP_LOGI(TAG, "Logitech M705 reports passed");
}

static void test_trust_wireless(void) {
    hid_layout_t layout;
    assert(hid_layout_parse(mouse2_desc, mouse2_len, &layout));

    assert(1 == layout.report_id);
    assert(5 == layout.button_count);
    assert(8 == layout.x.bit_offset && 8 == layout.x.bit_size);
    assert(16 == layout.y.bit_offset);
    assert(24 == layout.wheel.bit_offset);

    mouse_report_t report = parse_mouse_report(mouse2_reports[0], 5, &layout);
    assert(0 == report.buttons.button1);
    assert(48 == report.x_displacement);
    assert(-5 == report.y_displacement);
    assert(-2 == report.scroll);

    report = parse_mouse_report(mouse2_reports[1], 5, &layout);
    assert(1 == report.buttons.button1);
    assert(0 == report.buttons.button2);
    assert(-3 == report.x_displacement);
    assert(11 == report.y_displacement);
    assert(2 == report.scroll);

    report = parse_mouse_report(mouse2_reports[2], 5, &layout);
    assert(1 == report.buttons.button1);
    assert(1 == report.buttons.button2);
    assert(0 == report.buttons.button3);
    assert(0 == report.x_displacement);
    assert(0 == report.y_displacement);

    ESP_LOGI(TAG, "Trust Kuza passed");
}

static void test_fujitsu_m520(void) {
    hid_layout_t layout;
    assert(hid_layout_parse(mouse3_desc, mouse3_len, &layout));

    // This one puts no report ID in front of its reports
    assert(0 == layout.report_id);
    assert(3 == layout.button_count);
    assert(0 == layout.buttons.bit_offset);
    assert(8 == layout.x.bit_offset);
    assert(16 == layout.y.bit_offset);
    assert(24 == layout.wheel.bit_offset);

    mouse_report_t report = parse_mouse_report(mouse3_reports[0], 4, &layout);
    assert(1 == report.buttons.button1);
    assert(0 == report.buttons.button2);
    assert(1 == report.buttons.button3);
    assert(16 == report.x_displacement);
    assert(-16 == report.y_displacement);
    assert(1 == report.scroll);

    report = parse_mouse_report(mouse3_reports[1], 4, &layout);
    assert(0 == report.buttons.val);
    assert(-85 == report.x_displacement);
    assert(66 == report.y_displacement);
    assert(-5 == report.scroll);

    report = parse_mouse_report(mouse3_reports[2], 4, &layout);
    assert(1 == report.buttons.button1);
    assert(1 == report.buttons.button2);
    assert(1 == report.buttons.button3);
    assert(127 == report.x_displacement);
    assert(-127 == report.y_displacement);
    assert(0 == report.scroll);

    ESP_LOGI(TAG, "Fujitsu M520 passed");
}

static void test_stadia(void) {
    hid_gamepad_t pad;
    assert(hid_gamepad_open(&pad, gamepad1_desc, gamepad1_len, UNKNOWN_VID, UNKNOWN_PID));
    assert(NULL == pad.quirk);

    // Hat at 6 is left, both sticks centered
    gamepad_report_t report = parse_gamepad_report(pad1_reports[0], 11, &pad);
    assert(3 == report.report_id);
    assert(0 == report.buttons.up);
    assert(0 == report.buttons.down);
    assert(1 == report.buttons.left);
    assert(0 == report.buttons.right);
    assert(128 == report.lx && 128 == report.ly);
    assert(128 == report.rx && 128 == report.ry);

    // Hat centered, left stick pushed down. A stick counts as a direction too, so this reads as
    // down even though nothing on the d-pad moved.
    report = parse_gamepad_report(pad1_reports[1], 11, &pad);
    assert(1 == report.buttons.down);
    assert(0 == report.buttons.up);
    // Its axes report 1..255 rather than 0..255, so the byte they scale to is a shade lower
    assert(104 == report.lx);
    assert(200 == report.ly);
    assert(128 == report.rx && 128 == report.ry);

    // Hat out of range, so centered, and the sticks are near enough the middle
    report = parse_gamepad_report(pad1_reports[2], 11, &pad);
    assert(0 == report.buttons.val);
    assert(126 == report.lx && 126 == report.ly);

    // Hat up while the stick is pushed down and left: both are reported, the caller decides
    report = parse_gamepad_report(pad1_reports[3], 11, &pad);
    assert(1 == report.buttons.a);
    assert(1 == report.buttons.b);
    assert(0 == report.buttons.x);
    assert(0 == report.buttons.y);
    assert(1 == report.buttons.up);
    assert(1 == report.buttons.down);
    assert(1 == report.buttons.left);
    assert(0 == report.buttons.right);
    assert(63 == report.lx && 192 == report.ly);
    assert(31 == report.rx && 224 == report.ry);

    // Every one of its fifteen buttons held, hat centered
    report = parse_gamepad_report(pad1_reports[4], 11, &pad);
    assert(1 == report.buttons.a);
    assert(1 == report.buttons.b);
    assert(1 == report.buttons.x);
    assert(1 == report.buttons.y);
    assert(1 == report.buttons.select);
    assert(1 == report.buttons.start);
    assert(1 == report.buttons.l1);
    assert(1 == report.buttons.l2);
    assert(1 == report.buttons.l3);
    assert(1 == report.buttons.l4);
    assert(1 == report.buttons.r1);
    assert(1 == report.buttons.r2);
    assert(1 == report.buttons.r3);
    assert(1 == report.buttons.r4);
    assert(1 == report.buttons.home);
    assert(0 == report.buttons.up);
    assert(0 == report.buttons.down);
    assert(0 == report.buttons.left);
    assert(0 == report.buttons.right);

    hid_gamepad_close(&pad);
    ESP_LOGI(TAG, "Stadia controller passed");
}

static void test_dualshock4_clone(void) {
    hid_gamepad_t pad;
    assert(hid_gamepad_open(&pad, gamepad2_desc, gamepad2_len, UNKNOWN_VID, UNKNOWN_PID));

    // Sticks centered, hat at 8 meaning centered
    gamepad_report_t report = parse_gamepad_report(pad2_reports[0], 64, &pad);
    assert(1 == report.report_id);
    assert(0 == report.buttons.val);
    assert(128 == report.lx && 128 == report.ly);
    assert(128 == report.rx && 128 == report.ry);

    // The same, with the first button held
    report = parse_gamepad_report(pad2_reports[1], 64, &pad);
    assert(1 == report.buttons.a);
    assert(0 == report.buttons.b);
    assert(0 == report.buttons.x);
    assert(0 == report.buttons.y);

    // Left stick hard up and to the left, right stick hard the other way
    report = parse_gamepad_report(pad2_reports[2], 64, &pad);
    assert(0 == report.lx);
    assert(15 == report.ly);
    assert(225 == report.rx);
    assert(0 == report.ry);
    assert(1 == report.buttons.up);
    assert(1 == report.buttons.left);
    assert(0 == report.buttons.down);
    assert(0 == report.buttons.right);

    hid_gamepad_close(&pad);
    ESP_LOGI(TAG, "Fake DualShock controller passed");
}

/// A mouse is not a gamepad, and asking for one back leaves the report empty
static void test_a_mouse_is_not_a_gamepad(void) {
    hid_gamepad_t pad;
    assert(!hid_gamepad_open(&pad, mouse1_desc, mouse1_len, UNKNOWN_VID, UNKNOWN_PID));

    gamepad_report_t report = parse_gamepad_report(mouse1_reports[1], 8, &pad);
    assert(0 == report.buttons.val);

    ESP_LOGI(TAG, "Mice are left alone");
}

/// The path the firmware itself takes: hand over a descriptor, then hand over reports
static void test_registered_driver(void) {
    assert(ESP_OK == decode_descriptor_register_driver(mouse3_desc, (int)mouse3_len, HID_PROTOCOL_MOUSE, UNKNOWN_VID,
                                                       UNKNOWN_PID));
    mouse_report_t mouse = handle_mouse_event(mouse3_reports[0], 4);
    assert(1 == mouse.buttons.button1);
    assert(16 == mouse.x_displacement);

    assert(ESP_OK == decode_descriptor_register_driver(gamepad1_desc, (int)gamepad1_len, HID_PROTOCOL_NONE, UNKNOWN_VID,
                                                       UNKNOWN_PID));
    assert(NULL == badge_hid_gamepad_quirk());

    gamepad_report_t pad = handle_gamepad_event(pad1_reports[3], 11);
    assert(1 == pad.buttons.a);
    assert(63 == pad.lx);

    ESP_LOGI(TAG, "Registered drivers passed");
}

int main(void) {
    test_logitech_m705();
    test_trust_wireless();
    test_fujitsu_m520();
    test_stadia();
    test_dualshock4_clone();
    test_a_mouse_is_not_a_gamepad();
    test_registered_driver();

    printf("All HID tests passed\n");
    return 0;
}
