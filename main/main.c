#include <inttypes.h>
#include <stdio.h>
#include "badge_hid_host.h"
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/power.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "portmacro.h"

// Constants
static char const TAG[] = "main";

// Global variables
static size_t                     display_h_res        = 0;
static size_t                     display_v_res        = 0;
static bsp_display_color_format_t display_color_format = BSP_DISPLAY_COLOR_FORMAT_16_565RGB;
static bsp_display_endianness_t   display_data_endian  = BSP_DISPLAY_ENDIAN_LITTLE;
static pax_buf_t                  fb                   = {0};
static QueueHandle_t              input_event_queue    = NULL;
static char                       device_name[32]      = {0};

// Paper and ink, with one accent that means "this is happening right now"
#define COL_BG     0xFFF3F3F0
#define COL_CARD   0xFFFFFFFF
#define COL_INK    0xFF1B1B1B
#define COL_SOFT   0xFF8C8C86
#define COL_LINE   0xFFD9D9D3
#define COL_ACCENT 0xFFE23A2E

// One line per kind of input event the BSP delivers, so a device that only speaks one of them
// still shows what the others are missing
enum {
    EVENT_KEYBOARD,
    EVENT_NAVIGATION,
    EVENT_ACTION,
    EVENT_SCANCODE,
    EVENT_COUNT,
};

static char const* const event_title[EVENT_COUNT] = {"Keyboard", "Navigation", "Action", "Scancode"};
static char              event_line[EVENT_COUNT][48];
static int               event_latest = -1;

void blit(void) {
    esp_err_t res = bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(&fb));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to blit to display: %s", esp_err_to_name(res));
    }
}

static char const* navigation_key_name(bsp_input_navigation_key_t key) {
    switch (key) {
        case BSP_INPUT_NAVIGATION_KEY_ESC:
            return "escape";
        case BSP_INPUT_NAVIGATION_KEY_LEFT:
            return "left";
        case BSP_INPUT_NAVIGATION_KEY_RIGHT:
            return "right";
        case BSP_INPUT_NAVIGATION_KEY_UP:
            return "up";
        case BSP_INPUT_NAVIGATION_KEY_DOWN:
            return "down";
        case BSP_INPUT_NAVIGATION_KEY_HOME:
            return "home";
        case BSP_INPUT_NAVIGATION_KEY_END:
            return "end";
        case BSP_INPUT_NAVIGATION_KEY_PGUP:
            return "page up";
        case BSP_INPUT_NAVIGATION_KEY_PGDN:
            return "page down";
        case BSP_INPUT_NAVIGATION_KEY_MENU:
            return "menu";
        case BSP_INPUT_NAVIGATION_KEY_START:
            return "start";
        case BSP_INPUT_NAVIGATION_KEY_SELECT:
            return "select";
        case BSP_INPUT_NAVIGATION_KEY_RETURN:
            return "return";
        case BSP_INPUT_NAVIGATION_KEY_SUPER:
            return "super";
        case BSP_INPUT_NAVIGATION_KEY_TAB:
            return "tab";
        case BSP_INPUT_NAVIGATION_KEY_BACKSPACE:
            return "backspace";
        case BSP_INPUT_NAVIGATION_KEY_SPACE_L:
            return "space (left)";
        case BSP_INPUT_NAVIGATION_KEY_SPACE_M:
            return "space";
        case BSP_INPUT_NAVIGATION_KEY_SPACE_R:
            return "space (right)";
        case BSP_INPUT_NAVIGATION_KEY_F1:
            return "F1";
        case BSP_INPUT_NAVIGATION_KEY_F2:
            return "F2";
        case BSP_INPUT_NAVIGATION_KEY_F3:
            return "F3";
        case BSP_INPUT_NAVIGATION_KEY_F4:
            return "F4";
        case BSP_INPUT_NAVIGATION_KEY_F5:
            return "F5";
        case BSP_INPUT_NAVIGATION_KEY_F6:
            return "F6";
        case BSP_INPUT_NAVIGATION_KEY_F7:
            return "F7";
        case BSP_INPUT_NAVIGATION_KEY_F8:
            return "F8";
        case BSP_INPUT_NAVIGATION_KEY_F9:
            return "F9";
        case BSP_INPUT_NAVIGATION_KEY_F10:
            return "F10";
        case BSP_INPUT_NAVIGATION_KEY_F11:
            return "F11";
        case BSP_INPUT_NAVIGATION_KEY_F12:
            return "F12";
        case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
            return "gamepad A";
        case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
            return "gamepad B";
        case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_X:
            return "gamepad X";
        case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y:
            return "gamepad Y";
        case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS:
            return "joystick press";
        case BSP_INPUT_NAVIGATION_KEY_VOLUME_UP:
            return "volume up";
        case BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN:
            return "volume down";
        case BSP_INPUT_NAVIGATION_KEY_NONE:
        default:
            return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

/// A card with a heading. Returns the top of the space left to draw in.
static float card(float x, float y, float w, float h, char const* title) {
    pax_draw_round_rect(&fb, COL_CARD, x, y, w, h, 10);
    pax_outline_round_rect(&fb, COL_LINE, x, y, w, h, 10);
    pax_draw_text(&fb, COL_SOFT, pax_font_saira_regular, 15, x + 14, y + 8, title);
    return y + 32;
}

/// A key, a button, a mouse button: filled when it is down, an outline when it is not
static void key_rect(float x, float y, float w, float h, float radius, bool down) {
    pax_draw_round_rect(&fb, down ? COL_ACCENT : COL_LINE, x, y, w, h, radius);
    if (!down) {
        pax_outline_round_rect(&fb, COL_SOFT, x, y, w, h, radius);
    }
}

static void key_circle(float cx, float cy, float r, bool down, char const* label) {
    pax_draw_circle(&fb, down ? COL_ACCENT : COL_LINE, cx, cy, r);
    if (!down) {
        pax_outline_circle(&fb, COL_SOFT, cx, cy, r);
    }
    if (label != NULL) {
        pax_vec2f size = pax_text_size(pax_font_saira_regular, 15, label);
        pax_draw_text(&fb, down ? COL_CARD : COL_INK, pax_font_saira_regular, 15, cx - size.x / 2, cy - size.y / 2,
                      label);
    }
}

/// One analog stick: a ring, a knob where the stick is standing, a dot where the centre is
static void draw_stick(float cx, float cy, float r, uint8_t ax, uint8_t ay, bool pressed, char const* label) {
    pax_draw_circle(&fb, COL_BG, cx, cy, r);
    pax_outline_circle(&fb, pressed ? COL_ACCENT : COL_SOFT, cx, cy, r);
    pax_draw_circle(&fb, COL_LINE, cx, cy, 2);

    float const reach = r - 10;
    float const dx    = ((float)ax - 128.0f) / 128.0f * reach;
    float const dy    = ((float)ay - 128.0f) / 128.0f * reach;
    pax_draw_circle(&fb, pressed ? COL_ACCENT : COL_INK, cx + dx, cy + dy, 8);

    pax_vec2f size = pax_text_size(pax_font_sky_mono, 12, label);
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 12, cx - size.x / 2, cy + r + 4, label);
}

/// A trigger, drawn as a bar that fills from the bottom
static void draw_trigger(float x, float y, float w, float h, uint8_t value, char const* label) {
    pax_draw_round_rect(&fb, COL_BG, x, y, w, h, 4);
    pax_outline_round_rect(&fb, COL_SOFT, x, y, w, h, 4);
    float const filled = h * (float)value / 255.0f;
    if (filled > 2) {
        pax_draw_round_rect(&fb, COL_ACCENT, x, y + h - filled, w, filled, 4);
    }
    pax_vec2f size = pax_text_size(pax_font_sky_mono, 12, label);
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 12, x + w / 2 - size.x / 2, y - 16, label);
}

static void draw_gamepad(float x, float y, float w, float h, badge_hid_state_t const* state) {
    float const top = card(x, y, w, h, state->gamepad_seen ? "Gamepad" : "Gamepad - nothing connected");

    gamepad_report_t const* g = &state->gamepad;

    // Shoulders and triggers along the top
    float const shoulder_y = top + 18;
    key_rect(x + 26, shoulder_y, 64, 16, 8, g->buttons.l1);
    key_rect(x + w - 90, shoulder_y, 64, 16, 8, g->buttons.r1);
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 12, x + 26, shoulder_y - 16, "L1");
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 12, x + w - 90, shoulder_y - 16, "R1");
    draw_trigger(x + 100, shoulder_y - 2, 16, 44, g->lt, "L2");
    draw_trigger(x + w - 116, shoulder_y - 2, 16, 44, g->rt, "R2");

    // The d-pad, which is whichever of the stick, the hat switch or four buttons the pad has
    float const dpad_cx = x + w * 0.20f;
    float const dpad_cy = top + h * 0.44f;
    float const arm     = 22;
    float const thick   = 20;
    key_rect(dpad_cx - thick / 2, dpad_cy - arm - thick / 2, thick, arm, 4, g->buttons.up);
    key_rect(dpad_cx - thick / 2, dpad_cy + thick / 2, thick, arm, 4, g->buttons.down);
    key_rect(dpad_cx - arm - thick / 2, dpad_cy - thick / 2, arm, thick, 4, g->buttons.left);
    key_rect(dpad_cx + thick / 2, dpad_cy - thick / 2, arm, thick, 4, g->buttons.right);
    pax_draw_round_rect(&fb, COL_LINE, dpad_cx - thick / 2, dpad_cy - thick / 2, thick, thick, 3);

    // Face buttons, in the diamond everybody expects
    float const face_cx = x + w * 0.80f;
    float const face_cy = dpad_cy;
    float const spread  = 30;
    key_circle(face_cx, face_cy - spread, 15, g->buttons.y, "Y");
    key_circle(face_cx - spread, face_cy, 15, g->buttons.x, "X");
    key_circle(face_cx + spread, face_cy, 15, g->buttons.b, "B");
    key_circle(face_cx, face_cy + spread, 15, g->buttons.a, "A");

    // Select, home and start in the middle
    float const mid_cx = x + w / 2;
    float const mid_y  = dpad_cy - 8;
    key_rect(mid_cx - 46, mid_y, 28, 12, 6, g->buttons.select);
    key_circle(mid_cx, mid_y + 6, 11, g->buttons.home, NULL);
    key_rect(mid_cx + 18, mid_y, 28, 12, 6, g->buttons.start);
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 11, mid_cx - 46, mid_y + 16, "select");
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 11, mid_cx + 18, mid_y + 16, "start");

    // The sticks, with their own presses
    float const stick_cy = y + h - 52;
    draw_stick(x + w * 0.33f, stick_cy, 30, g->lx, g->ly, g->buttons.l3, "L");
    draw_stick(x + w * 0.67f, stick_cy, 30, g->rx, g->ry, g->buttons.r3, "R");

    // Whatever else the pad reports, so nothing is invisible
    char extra[48];
    snprintf(extra, sizeof(extra), "L4 %s   R4 %s   report 0x%02X", g->buttons.l4 ? "down" : "up",
             g->buttons.r4 ? "down" : "up", g->report_id);
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 12, x + 14, y + h - 18, extra);
}

static void draw_mouse(float x, float y, float w, float h, badge_hid_state_t const* state) {
    float const top = card(x, y, w, h, state->mouse_seen ? "Mouse" : "Mouse - nothing connected");

    mouse_report_t const* m = &state->mouse;

    // The mouse itself: a body with three buttons and a wheel
    float const body_w = 84;
    float const body_h = 116;
    float const body_x = x + 20;
    float const body_y = top + 6;

    pax_draw_round_rect(&fb, COL_BG, body_x, body_y, body_w, body_h, 34);
    pax_outline_round_rect(&fb, COL_SOFT, body_x, body_y, body_w, body_h, 34);

    float const button_h = 44;
    if (m->buttons.button1) {
        pax_draw_round_rect(&fb, COL_ACCENT, body_x + 2, body_y + 2, body_w / 2 - 3, button_h, 20);
    }
    if (m->buttons.button2) {
        pax_draw_round_rect(&fb, COL_ACCENT, body_x + body_w / 2 + 1, body_y + 2, body_w / 2 - 3, button_h, 20);
    }
    pax_draw_line(&fb, COL_SOFT, body_x + body_w / 2, body_y + 4, body_x + body_w / 2, body_y + button_h);
    pax_draw_line(&fb, COL_SOFT, body_x + 4, body_y + button_h, body_x + body_w - 4, body_y + button_h);

    // Wheel, which is also the middle button
    key_rect(body_x + body_w / 2 - 6, body_y + 12, 12, 26, 6, m->buttons.button3);

    // Where it has travelled, and how far it moved just now
    float const pad_x = x + w - 132;
    float const pad_y = top + 10;
    float const pad_w = 112;
    float const pad_h = 100;
    pax_draw_round_rect(&fb, COL_BG, pad_x, pad_y, pad_w, pad_h, 6);
    pax_outline_round_rect(&fb, COL_LINE, pad_x, pad_y, pad_w, pad_h, 6);

    float const pad_cx = pad_x + pad_w / 2;
    float const pad_cy = pad_y + pad_h / 2;
    pax_draw_line(&fb, COL_LINE, pad_x + 6, pad_cy, pad_x + pad_w - 6, pad_cy);
    pax_draw_line(&fb, COL_LINE, pad_cx, pad_y + 6, pad_cx, pad_y + pad_h - 6);

    // Clamp the arrow to the pad, a fast swipe should not draw off the card
    float       dx    = (float)m->x_displacement;
    float       dy    = (float)m->y_displacement;
    float const reach = pad_w / 2 - 8;
    float const len   = (dx * dx + dy * dy > 0) ? sqrtf(dx * dx + dy * dy) : 0;
    if (len > reach) {
        dx = dx / len * reach;
        dy = dy / len * reach;
    }
    if (len > 0) {
        pax_draw_thick_line(&fb, COL_ACCENT, pad_cx, pad_cy, pad_cx + dx, pad_cy + dy, 3);
    }
    pax_draw_circle(&fb, COL_INK, pad_cx, pad_cy, 3);

    char text[48];
    snprintf(text, sizeof(text), "travelled %d, %d", state->mouse_x, state->mouse_y);
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 12, x + 14, y + h - 34, text);
    snprintf(text, sizeof(text), "scroll %d   tilt %d", state->mouse_scroll, state->mouse_tilt);
    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 12, x + 14, y + h - 18, text);
}

static void draw_events(float x, float y, float w, float h) {
    float const top = card(x, y, w, h, "Last event of each kind");

    float const row_h = (h - (top - y) - 10) / EVENT_COUNT;
    for (int i = 0; i < EVENT_COUNT; i++) {
        float const row_y = top + i * row_h;
        if (i == event_latest) {
            pax_draw_round_rect(&fb, COL_BG, x + 8, row_y - 2, w - 16, row_h - 2, 5);
            pax_draw_round_rect(&fb, COL_ACCENT, x + 8, row_y - 2, 3, row_h - 2, 2);
        }
        pax_draw_text(&fb, COL_INK, pax_font_saira_regular, 15, x + 18, row_y, event_title[i]);
        pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 13, x + 18, row_y + 17, event_line[i]);
    }
}

/// What is plugged in, or that nothing is
static void draw_device_strip(float x, float y, float w, float h, badge_hid_state_t const* state) {
    pax_draw_round_rect(&fb, COL_CARD, x, y, w, h, 8);
    pax_outline_round_rect(&fb, COL_LINE, x, y, w, h, 8);
    pax_draw_circle(&fb, state->device_connected ? COL_ACCENT : COL_LINE, x + 16, y + h / 2, 5);

    char text[96];
    if (state->device_connected) {
        char const* product = state->product[0] != '\0' ? state->product : "unnamed device";
        if (state->manufacturer[0] != '\0') {
            snprintf(text, sizeof(text), "%s  %s", state->manufacturer, product);
        } else {
            snprintf(text, sizeof(text), "%s", product);
        }
    } else {
        snprintf(text, sizeof(text), "Nothing connected, plug in a keyboard, mouse or gamepad");
    }
    pax_draw_text(&fb, COL_INK, pax_font_saira_regular, 16, x + 30, y + h / 2 - 10, text);

    if (state->device_connected) {
        snprintf(text, sizeof(text), "%s   %04X:%04X", state->protocol, state->vid, state->pid);
        pax_vec2f size = pax_text_size(pax_font_sky_mono, 13, text);
        pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 13, x + w - 14 - size.x, y + h / 2 - 8, text);
    }
}

static void redraw(void) {
    badge_hid_state_t state;
    badge_hid_get_state(&state);

    float const width    = pax_buf_get_width(&fb);
    float const height   = pax_buf_get_height(&fb);
    float const margin   = 10;
    float const header_h = 44;
    float const footer_h = 22;

    pax_background(&fb, COL_BG);

    // Header
    pax_draw_rect(&fb, COL_INK, 0, 0, width, header_h);
    pax_draw_text(&fb, COL_CARD, pax_font_saira_regular, 24, margin + 4, 9, "USB HID host");
    if (device_name[0] != '\0') {
        pax_vec2f size = pax_text_size(pax_font_sky_mono, 13, device_name);
        pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 13, width - margin - 4 - size.x, 17, device_name);
    }

    // What is plugged in, across the full width
    float const strip_h = 34;
    draw_device_strip(margin, header_h + margin, width - margin * 2, strip_h, &state);

    // The gamepad gets the room, the mouse and the event log share the rest
    float const body_y  = header_h + margin + strip_h + margin;
    float const body_h  = height - body_y - footer_h - margin;
    float const left_w  = (width - margin * 3) * 0.58f;
    float const right_x = margin + left_w + margin;
    float const right_w = width - right_x - margin;
    float const mouse_h = body_h * 0.44f;

    draw_gamepad(margin, body_y, left_w, body_h, &state);
    draw_mouse(right_x, body_y, right_w, mouse_h, &state);
    draw_events(right_x, body_y + mouse_h + margin, right_w, body_h - mouse_h - margin);

    pax_draw_text(&fb, COL_SOFT, pax_font_sky_mono, 13, margin + 4, height - footer_h,
                  "F1 launcher   F2 backlight off   F3 backlight on");

    blit();
}

// ---------------------------------------------------------------------------

static void events_reset(void) {
    for (int i = 0; i < EVENT_COUNT; i++) {
        snprintf(event_line[i], sizeof(event_line[i]), "nothing yet");
    }
}

void app_main(void) {
    // Start the GPIO interrupt service
    gpio_install_isr_service(0);

    // Initialize the Non Volatile Storage service
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);

    // Ask for the colour format the panel actually runs at. Handed no configuration the BSP falls
    // back to 16 bit, and the display stays dark.
    bsp_configuration_t const bsp_configuration = {
        .display =
            {
                .requested_color_format = BSP_DISPLAY_COLOR_FORMAT_24_888RGB,
                .num_fbs                = 1,
            },
    };
    ESP_ERROR_CHECK(bsp_device_initialize(&bsp_configuration));

    if (bsp_device_get_name(device_name, sizeof(device_name)) != ESP_OK) {
        device_name[0] = '\0';
    }

    // Get display parameters and rotation
    res = bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format, &display_data_endian);
    ESP_ERROR_CHECK(res);  // Check that the display parameters have been initialized
    bsp_display_rotation_t display_rotation = bsp_display_get_default_rotation();

    // Nothing turns the backlight on for us, and whatever ran before us may well have turned it off
    bsp_display_set_backlight_brightness(100);

    // Convert BSP color format into PAX buffer type
    pax_buf_type_t format = PAX_BUF_24_888RGB;
    switch (display_color_format) {
        case BSP_DISPLAY_COLOR_FORMAT_16_565RGB:
            format = PAX_BUF_16_565RGB;
            break;
        case BSP_DISPLAY_COLOR_FORMAT_24_888RGB:
            format = PAX_BUF_24_888RGB;
            break;
        default:
            break;
    }

    // Convert BSP display rotation format into PAX orientation type
    pax_orientation_t orientation = PAX_O_UPRIGHT;
    switch (display_rotation) {
        case BSP_DISPLAY_ROTATION_90:
            orientation = PAX_O_ROT_CCW;
            break;
        case BSP_DISPLAY_ROTATION_180:
            orientation = PAX_O_ROT_HALF;
            break;
        case BSP_DISPLAY_ROTATION_270:
            orientation = PAX_O_ROT_CW;
            break;
        case BSP_DISPLAY_ROTATION_0:
        default:
            orientation = PAX_O_UPRIGHT;
            break;
    }

    // Initialize graphics stack
    pax_buf_init(&fb, NULL, display_h_res, display_v_res, format);
    pax_buf_reversed(&fb, display_data_endian == BSP_DISPLAY_ENDIAN_BIG);
    pax_buf_set_orientation(&fb, orientation);

    // Get input event queue from BSP
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    // Put something on screen before bringing up USB, so the display says what this is even if no
    // device ever turns up
    events_reset();
    redraw();

    // Power to USB
    bsp_power_set_usb_host_boost_enabled(true);

    ESP_ERROR_CHECK(badge_hid_init(input_event_queue));

    ESP_LOGI(TAG, "Waiting for USB HID input on %s", device_name[0] != '\0' ? device_name : "this device");

    badge_hid_state_t state;
    badge_hid_get_state(&state);
    uint32_t drawn_sequence = state.sequence;

    while (1) {
        bsp_input_event_t event;
        bool              dirty = false;

        // Sticks and mouse movement do not turn into BSP events, so the queue is polled rather
        // than waited on and the reports are picked up in between
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(40)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_KEYBOARD: {
                    if (event.args_keyboard.ascii == '\b' || event.args_keyboard.ascii == '\t') {
                        continue;  // Ignore backspace & tab keyboard events
                    }
                    ESP_LOGI(TAG, "Keyboard event %c (%02x) %s", event.args_keyboard.ascii,
                             (uint8_t)event.args_keyboard.ascii, event.args_keyboard.utf8);
                    snprintf(event_line[EVENT_KEYBOARD], sizeof(event_line[EVENT_KEYBOARD]),
                             "'%c' 0x%02x  %s  mods 0x%" PRIX32, event.args_keyboard.ascii,
                             (uint8_t)event.args_keyboard.ascii, event.args_keyboard.utf8,
                             event.args_keyboard.modifiers);
                    event_latest = EVENT_KEYBOARD;
                    break;
                }

                case INPUT_EVENT_TYPE_NAVIGATION: {
                    char const* key_name = navigation_key_name(event.args_navigation.key);
                    ESP_LOGI(TAG, "Navigation event %s: %s", key_name,
                             event.args_navigation.state ? "pressed" : "released");

                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F1) {
                        // A restart resets the USB host anyway, so there is nothing to shut down
                        // first. Waiting for the driver to unwind would only hang here as long as
                        // a device is still plugged in.
                        bsp_power_set_usb_host_boost_enabled(false);
                        bsp_device_restart_to_launcher();
                    }
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F2) {
                        bsp_display_set_backlight_brightness(0);
                        bsp_input_set_backlight_brightness(0);
                    }
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F3) {
                        bsp_display_set_backlight_brightness(100);
                        bsp_input_set_backlight_brightness(100);
                    }

                    snprintf(event_line[EVENT_NAVIGATION], sizeof(event_line[EVENT_NAVIGATION]),
                             "%s %s  mods 0x%" PRIX32, key_name, event.args_navigation.state ? "pressed" : "released",
                             event.args_navigation.modifiers);
                    event_latest = EVENT_NAVIGATION;
                    break;
                }

                case INPUT_EVENT_TYPE_ACTION: {
                    ESP_LOGI(TAG, "Action event 0x%0" PRIX32 ": %s", (uint32_t)event.args_action.type,
                             event.args_action.state ? "yes" : "no");
                    snprintf(event_line[EVENT_ACTION], sizeof(event_line[EVENT_ACTION]), "type 0x%" PRIX32 "  %s",
                             (uint32_t)event.args_action.type, event.args_action.state ? "yes" : "no");
                    event_latest = EVENT_ACTION;
                    break;
                }

                case INPUT_EVENT_TYPE_SCANCODE: {
                    ESP_LOGI(TAG, "Scancode event 0x%0" PRIX32, (uint32_t)event.args_scancode.scancode);
                    snprintf(event_line[EVENT_SCANCODE], sizeof(event_line[EVENT_SCANCODE]), "0x%" PRIX32,
                             (uint32_t)event.args_scancode.scancode);
                    event_latest = EVENT_SCANCODE;
                    break;
                }

                default:
                    continue;
            }
            dirty = true;
        }

        badge_hid_get_state(&state);
        if (state.sequence != drawn_sequence) {
            drawn_sequence = state.sequence;
            dirty          = true;
        }

        if (dirty) {
            redraw();
        }
    }
}
