#include <assert.h>
#include <stdio.h>
#include "badge_hid_drivers.h"
#include "esp_log.h"
#include "test_descriptors.h"
#include "test_reports.h"

static char const TAG[] = "HID Test";




int test_layouts(void) {
    mouse_field_layout_t mouse_layout = {0};
    gamepad_field_layout_t pad_layout = {0};

    mouse_report_t mouse_report = {0};
    gamepad_report_t pad_report = {0};
    

    assert(ESP_OK == analyze_mouse_layout(mouse1_desc, mouse1_len, &mouse_layout));

    assert(mouse_layout.buttons.present);
    assert(mouse_layout.x.present);
    assert(mouse_layout.y.present);
    assert(mouse_layout.scroll.present);
    assert(mouse_layout.tilt.present);

    assert(16 == mouse_layout.buttons.size);
    assert(8 == mouse_layout.buttons.offset);

    assert(12 == mouse_layout.x.size);
    assert(24 == mouse_layout.x.offset);

    assert(12 == mouse_layout.y.size);
    assert(36 == mouse_layout.y.offset);

    assert(8 == mouse_layout.scroll.size);
    assert(48 == mouse_layout.scroll.offset);

    assert(8 == mouse_layout.tilt.size);
    assert(56 == mouse_layout.tilt.offset);

    printf("Logitech layout M705 passed\n");

    mouse_report = parse_mouse_event(mouse1_reports[0], 8, &mouse_layout);
    assert(0 == mouse_report.buttons.button1);
    assert(0 == mouse_report.buttons.button2);
    assert(0 == mouse_report.buttons.button3);
    assert(0 == mouse_report.x_displacement);
    assert(0 == mouse_report.y_displacement);
    assert(-2 == mouse_report.scroll);
    assert(0 == mouse_report.tilt);
    mouse_report = parse_mouse_event(mouse1_reports[1], 8, &mouse_layout);
    assert(0 == mouse_report.buttons.button1);
    assert(0 == mouse_report.buttons.button2);
    assert(0 == mouse_report.buttons.button3);
    assert(-1 == mouse_report.x_displacement);
    assert(4 == mouse_report.y_displacement);
    assert(0 == mouse_report.scroll);
    assert(1 == mouse_report.tilt);
    mouse_report = parse_mouse_event(mouse1_reports[2], 8, &mouse_layout);
    assert(0 == mouse_report.buttons.button1);
    assert(0 == mouse_report.buttons.button2);
    assert(0 == mouse_report.buttons.button3);
    assert(66 == mouse_report.x_displacement);
    assert(40 == mouse_report.y_displacement);
    assert(0 == mouse_report.scroll);
    assert(-1 == mouse_report.tilt);

    printf("Logitech reports M705 passed\n");

    assert(ESP_OK == analyze_mouse_layout(mouse2_desc, mouse2_len, &mouse_layout));

    assert(mouse_layout.buttons.present);
    assert(mouse_layout.x.present);
    assert(mouse_layout.y.present);
    assert(mouse_layout.scroll.present);
    assert(!mouse_layout.tilt.present);  // Trust mouse lacks tilt

    assert(5 == mouse_layout.buttons.size);
    assert(8 == mouse_layout.buttons.offset);

    assert(8 == mouse_layout.x.size);
    assert(16 == mouse_layout.x.offset);

    assert(8 == mouse_layout.y.size);
    assert(24 == mouse_layout.y.offset);

    assert(8 == mouse_layout.scroll.size);
    assert(32 == mouse_layout.scroll.offset);

    printf("Trust Kuza layout passed\n");

    mouse_report = parse_mouse_event(mouse2_reports[0], 5, &mouse_layout);
    assert(0 == mouse_report.buttons.button1);
    assert(0 == mouse_report.buttons.button2);
    assert(0 == mouse_report.buttons.button3);
    assert(48 == mouse_report.x_displacement);
    assert(-5 == mouse_report.y_displacement);
    assert(-2 == mouse_report.scroll);
    assert(0 == mouse_report.tilt);
    mouse_report = parse_mouse_event(mouse2_reports[1], 5, &mouse_layout);
    assert(1 == mouse_report.buttons.button1);
    assert(0 == mouse_report.buttons.button2);
    assert(0 == mouse_report.buttons.button3);
    assert(-3 == mouse_report.x_displacement);
    assert(11 == mouse_report.y_displacement);
    assert(2 == mouse_report.scroll);
    assert(0 == mouse_report.tilt);
    mouse_report = parse_mouse_event(mouse2_reports[2], 5, &mouse_layout);
    assert(1 == mouse_report.buttons.button1);
    assert(1 == mouse_report.buttons.button2);
    assert(0 == mouse_report.buttons.button3);
    assert(0 == mouse_report.x_displacement);
    assert(0 == mouse_report.y_displacement);
    assert(0 == mouse_report.scroll);
    assert(0 == mouse_report.tilt);

    printf("Trust Kuza reports passed\n");

    assert(ESP_OK == analyze_mouse_layout(mouse3_desc, mouse3_len, &mouse_layout));

    assert(mouse_layout.buttons.present);
    assert(mouse_layout.x.present);
    assert(mouse_layout.y.present);
    assert(mouse_layout.scroll.present);
    assert(!mouse_layout.tilt.present);

    assert(3 == mouse_layout.buttons.size);
    assert(0 == mouse_layout.buttons.offset);

    assert(8 == mouse_layout.x.size);
    assert(8 == mouse_layout.x.offset);

    assert(8 == mouse_layout.y.size);
    assert(16 == mouse_layout.y.offset);

    assert(8 == mouse_layout.scroll.size);
    assert(24 == mouse_layout.scroll.offset);

    printf("Fujitsu M520 layout passed\n");

    mouse_report = parse_mouse_event(mouse3_reports[0], 4, &mouse_layout);
    assert(1 == mouse_report.buttons.button1);
    assert(0 == mouse_report.buttons.button2);
    assert(1 == mouse_report.buttons.button3);
    assert(16 == mouse_report.x_displacement);
    assert(-16 == mouse_report.y_displacement);
    assert(1 == mouse_report.scroll);
    assert(0 == mouse_report.tilt);
    mouse_report = parse_mouse_event(mouse3_reports[1], 4, &mouse_layout);
    assert(0 == mouse_report.buttons.button1);
    assert(0 == mouse_report.buttons.button2);
    assert(0 == mouse_report.buttons.button3);
    assert(-85 == mouse_report.x_displacement);
    assert(66 == mouse_report.y_displacement);
    assert(-5 == mouse_report.scroll);
    assert(0 == mouse_report.tilt);

    printf("Fujitsu M520 reports passed\n");

    assert(ESP_OK == analyze_gamepad_layout(gamepad1_desc, gamepad1_len, &pad_layout));

    assert(pad_layout.dpad.present);
    assert(pad_layout.buttons.present);
    assert(pad_layout.lx.present);
    assert(pad_layout.ly.present);
    assert(pad_layout.rx.present);
    assert(pad_layout.ry.present);
    assert(pad_layout.lt.present);
    assert(pad_layout.rt.present);

    assert(8 == pad_layout.dpad.offset);
    assert(4 == pad_layout.dpad.size);

    assert(16 == pad_layout.buttons.offset);
    assert(15 == pad_layout.buttons.size);

    assert(32 == pad_layout.lx.offset);
    assert(8 == pad_layout.lx.size);

    assert(40 == pad_layout.ly.offset);
    assert(8 == pad_layout.ly.size);

    assert(48 == pad_layout.rx.offset);
    assert(8 == pad_layout.rx.size);

    assert(56 == pad_layout.ry.offset);
    assert(8 == pad_layout.ry.size);

    assert(64 == pad_layout.lt.offset);
    assert(8 == pad_layout.lt.size);

    assert(72 == pad_layout.rt.offset);
    assert(8 == pad_layout.rt.size);

    printf("Stadia controller layout passed\n");

    pad_report = parse_gamepad_report(pad1_reports[0], 11, &pad_layout);
    assert(0 == pad_report.buttons.up);
    assert(0 == pad_report.buttons.down);
    assert(0 == pad_report.buttons.right);
    assert(1 == pad_report.buttons.left);

    assert(128 == pad_report.lx);
    assert(128 == pad_report.ly);
    assert(128 == pad_report.rx);
    assert(128 == pad_report.ry);

    assert(0 == pad_report.rt);
    assert(0 == pad_report.lt);

    pad_report = parse_gamepad_report(pad1_reports[1], 11, &pad_layout);
    assert(0 == pad_report.buttons.up);
    assert(0 == pad_report.buttons.down);
    assert(0 == pad_report.buttons.right);
    assert(0 == pad_report.buttons.left);

    assert(105 == pad_report.lx);
    assert(200 == pad_report.ly);
    assert(128 == pad_report.rx);
    assert(128 == pad_report.ry);

    assert(0 == pad_report.rt);
    assert(0 == pad_report.lt);

    printf("Stadia controller reports passed\n");

    return 0;
}

int main (void) {
    assert(0 == test_layouts());
}