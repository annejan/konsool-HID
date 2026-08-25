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

#define BLACK 0xFF000000
#define WHITE 0xFFFFFFFF
#define RED   0xFFFF0000

// A panel per kind of input event the BSP delivers, so a device that only speaks one of them still
// shows what the others are missing.
enum {
    PANEL_KEYBOARD,
    PANEL_NAVIGATION,
    PANEL_ACTION,
    PANEL_SCANCODE,
    PANEL_COUNT,
};

#define PANEL_LINES 3

static char const* const panel_title[PANEL_COUNT] = {"Keyboard", "Navigation", "Action", "Scancode"};
static char              panel_text[PANEL_COUNT][PANEL_LINES][40];
static int               panel_latest = -1;  // Panel that received the most recent event

void blit(void) {
    esp_err_t res = bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(&fb));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "blit failed: %s", esp_err_to_name(res));
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

static void panel_reset(void) {
    for (int panel = 0; panel < PANEL_COUNT; panel++) {
        snprintf(panel_text[panel][0], sizeof(panel_text[panel][0]), "nothing yet");
        panel_text[panel][1][0] = '\0';
        panel_text[panel][2][0] = '\0';
    }
}

static void redraw(void) {
    float const width    = pax_buf_get_width(&fb);
    float const height   = pax_buf_get_height(&fb);
    float const margin   = 8;
    float const header_h = 40;
    float const footer_h = 20;

    pax_background(&fb, WHITE);

    // Header
    pax_simple_rect(&fb, BLACK, 0, 0, width, header_h);
    pax_draw_text(&fb, WHITE, pax_font_saira_regular, 22, margin, 8, "USB HID host");
    if (device_name[0] != '\0') {
        pax_vec2f size = pax_text_size(pax_font_sky_mono, 14, device_name);
        pax_draw_text(&fb, WHITE, pax_font_sky_mono, 14, width - margin - size.x, 14, device_name);
    }

    // The panels, two by two
    float const grid_y = header_h + margin;
    float const cell_w = (width - margin * 3) / 2;
    float const cell_h = (height - grid_y - footer_h - margin * 2) / 2;

    for (int panel = 0; panel < PANEL_COUNT; panel++) {
        float const x = margin + (panel % 2) * (cell_w + margin);
        float const y = grid_y + (panel / 2) * (cell_h + margin);

        pax_outline_rect(&fb, BLACK, x, y, cell_w, cell_h);
        if (panel == panel_latest) {
            // Mark where the event now on screen came from
            pax_simple_rect(&fb, RED, x, y, cell_w, 3);
        }
        pax_draw_text(&fb, BLACK, pax_font_saira_regular, 18, x + 8, y + 6, panel_title[panel]);
        for (int line = 0; line < PANEL_LINES; line++) {
            pax_draw_text(&fb, BLACK, pax_font_sky_mono, 14, x + 8, y + 30 + line * 16, panel_text[panel][line]);
        }
    }

    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 14, margin, height - footer_h,
                  "F1 launcher   F2 backlight off   F3 backlight on");

    blit();
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

    // Put the panels on screen before bringing up USB, so the display says something even if no
    // device ever turns up
    panel_reset();
    redraw();

    // Power to USB
    bsp_power_set_usb_host_boost_enabled(true);

    ESP_ERROR_CHECK(badge_hid_init(input_event_queue));

    ESP_LOGI(TAG, "Waiting for USB HID input on %s", device_name[0] != '\0' ? device_name : "this device");

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
            case INPUT_EVENT_TYPE_KEYBOARD: {
                if (event.args_keyboard.ascii == '\b' || event.args_keyboard.ascii == '\t') {
                    continue;  // Ignore backspace & tab keyboard events
                }
                ESP_LOGI(TAG, "Keyboard event %c (%02x) %s", event.args_keyboard.ascii,
                         (uint8_t)event.args_keyboard.ascii, event.args_keyboard.utf8);
                snprintf(panel_text[PANEL_KEYBOARD][0], sizeof(panel_text[PANEL_KEYBOARD][0]), "ascii  %c (0x%02x)",
                         event.args_keyboard.ascii, (uint8_t)event.args_keyboard.ascii);
                snprintf(panel_text[PANEL_KEYBOARD][1], sizeof(panel_text[PANEL_KEYBOARD][1]), "utf-8  %s",
                         event.args_keyboard.utf8);
                snprintf(panel_text[PANEL_KEYBOARD][2], sizeof(panel_text[PANEL_KEYBOARD][2]), "mods   0x%" PRIX32,
                         event.args_keyboard.modifiers);
                panel_latest = PANEL_KEYBOARD;
                break;
            }

            case INPUT_EVENT_TYPE_NAVIGATION: {
                char const* key_name = navigation_key_name(event.args_navigation.key);
                ESP_LOGI(TAG, "Navigation event %s: %s", key_name,
                         event.args_navigation.state ? "pressed" : "released");

                if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F1) {
                    ESP_ERROR_CHECK(badge_hid_deinit());
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

                snprintf(panel_text[PANEL_NAVIGATION][0], sizeof(panel_text[PANEL_NAVIGATION][0]), "key    %s",
                         key_name);
                snprintf(panel_text[PANEL_NAVIGATION][1], sizeof(panel_text[PANEL_NAVIGATION][1]), "state  %s",
                         event.args_navigation.state ? "pressed" : "released");
                snprintf(panel_text[PANEL_NAVIGATION][2], sizeof(panel_text[PANEL_NAVIGATION][2]), "mods   0x%" PRIX32,
                         event.args_navigation.modifiers);
                panel_latest = PANEL_NAVIGATION;
                break;
            }

            case INPUT_EVENT_TYPE_ACTION: {
                ESP_LOGI(TAG, "Action event 0x%0" PRIX32 ": %s", (uint32_t)event.args_action.type,
                         event.args_action.state ? "yes" : "no");
                snprintf(panel_text[PANEL_ACTION][0], sizeof(panel_text[PANEL_ACTION][0]), "type   0x%" PRIX32,
                         (uint32_t)event.args_action.type);
                snprintf(panel_text[PANEL_ACTION][1], sizeof(panel_text[PANEL_ACTION][1]), "state  %s",
                         event.args_action.state ? "yes" : "no");
                panel_text[PANEL_ACTION][2][0] = '\0';
                panel_latest                   = PANEL_ACTION;
                break;
            }

            case INPUT_EVENT_TYPE_SCANCODE: {
                ESP_LOGI(TAG, "Scancode event 0x%0" PRIX32, (uint32_t)event.args_scancode.scancode);
                snprintf(panel_text[PANEL_SCANCODE][0], sizeof(panel_text[PANEL_SCANCODE][0]), "code   0x%" PRIX32,
                         (uint32_t)event.args_scancode.scancode);
                panel_text[PANEL_SCANCODE][1][0] = '\0';
                panel_text[PANEL_SCANCODE][2][0] = '\0';
                panel_latest                     = PANEL_SCANCODE;
                break;
            }

            default:
                continue;
        }

        redraw();
    }
}
