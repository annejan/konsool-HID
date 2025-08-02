/*
 * HID host library for gamepad and mouse input devices.
 *
 * Loosely based on https://github.com/esp32-open-source/usb-components/blob/master/hid-parser/hid_parser.c
 *
 * SPDX-FileCopyrightText: 2024-2025 chegewara
 * SPDX-FileCopyrightText: 2025 Badge.Team
 *
 * SPDX-License-Identifier: MIT
 */
#include "badge_hid_drivers.h"
#include <stdint.h>
#include <string.h>
#include "badge_hid_decoder.h"
#include "badge_hid_tokenizer.h"
#include "esp_log.h"
#include "usb/hid.h"

static char const TAG[] = "BADGE_HID_DRIVER";

bool analyze_mouse_layout(const uint8_t* desc, int desc_len, mouse_field_layout_t* layout_out) {
    memset(layout_out, 0, sizeof(*layout_out));
    parser_ctx_t ctx = {0};
    hid_tokenizer_state_t state = {.ptr = desc, .end = desc + desc_len};
    hid_item_t item;

    while (hid_next_item(&state, &item)) {
        update_parser_state(&ctx, &item);

        if (item.bType == HID_TYPE_MAIN && item.bTag == HID_TAG_INPUT) {
            ESP_LOGD(TAG, "Input: usage_page=0x%02x, count=%u, size=%u, offset=%u", ctx.usage_page, ctx.report_count, ctx.report_size, ctx.bit_offset);
            for (uint8_t i = 0; i < ctx.usage_count; i++) {
                ESP_LOGD(TAG, "  usage[%u] = 0x%04x", i, ctx.usages[i]);
            }
            for (uint8_t i = 0; i < ctx.report_count && i < ctx.usage_count; i++) {
                uint16_t usage = ctx.usages[i];
                ESP_LOGD(TAG, "Input: usage_page=0x%02x, usage=0x%04x", ctx.usage_page, usage);

                if (ctx.usage_page == HID_USAGE_PAGE_BUTTON) {
                    if (!layout_out->has_buttons) {
                        layout_out->has_buttons = true;
                        layout_out->button_bit_offset = ctx.bit_offset;
                    }
                    layout_out->button_bit_count += ctx.report_size;
                } else if (ctx.usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
                    switch (usage) {
                        case HID_USAGE_X:
                            layout_out->has_x = true;
                            layout_out->x_bit_offset = ctx.bit_offset;
                            layout_out->x_bit_size = ctx.report_size;
                            break;
                        case HID_USAGE_Y:
                            layout_out->has_y = true;
                            layout_out->y_bit_offset = ctx.bit_offset;
                            layout_out->y_bit_size = ctx.report_size;
                            break;
                        case HID_USAGE_WHEEL:
                            layout_out->has_scroll = true;
                            layout_out->scroll_bit_offset = ctx.bit_offset;
                            layout_out->scroll_bit_size = ctx.report_size;
                            break;
                    }
                } else if (ctx.usage_page == HID_USAGE_PAGE_CONSUMER && usage == HID_USAGE_AC_PAN) {
                    layout_out->has_tilt = true;
                    layout_out->tilt_bit_offset = ctx.bit_offset;
                    layout_out->tilt_bit_size = ctx.report_size;
                }
                ctx.bit_offset += ctx.report_size;
            }
            ctx.usage_count = 0;
        }
    }

    if (!layout_out->has_buttons) {
        ESP_LOGW(TAG, "No buttons found");
    }
    if (!layout_out->has_x || !layout_out->has_y) {
        ESP_LOGW(TAG, "Missing X/Y axes");
    }

    return layout_out->has_buttons && layout_out->has_x && layout_out->has_y;
}


bool analyze_gamepad_layout(const uint8_t* desc, int desc_len, gamepad_field_layout_t* layout_out) {
    hid_tokenizer_state_t state = {.ptr = desc, .end = desc + desc_len};
    parser_ctx_t          ctx   = {0};
    hid_item_t            item;

    memset(layout_out, 0, sizeof(*layout_out));

    while (hid_next_item(&state, &item)) {
        switch (item.bType) {
            case HID_TYPE_GLOBAL:
                switch (item.bTag) {
                    case HID_GLOBAL_USAGE_PAGE:
                        ctx.usage_page = item.data;
                        break;
                    case HID_GLOBAL_REPORT_SIZE:
                        ctx.report_size = item.data;
                        break;
                    case HID_GLOBAL_REPORT_COUNT:
                        ctx.report_count = item.data;
                        break;
                    case HID_GLOBAL_REPORT_ID:
                        layout_out->has_report_id = true;
                        ctx.bit_offset            = 8;  // reserve first byte
                        break;
                }
                break;

            case HID_TYPE_LOCAL:
                if (item.bTag == HID_LOCAL_USAGE && ctx.usage_count < 32) {
                    ctx.usages[ctx.usage_count++] = item.data;
                }
                break;

            case HID_TYPE_MAIN:
                if (item.bTag == HID_MAIN_INPUT) {
                    if (ctx.usage_page == 0x09) {  // Button page
                        if (!layout_out->has_buttons) {
                            layout_out->has_buttons       = true;
                            layout_out->button_bit_offset = ctx.bit_offset;
                        }
                        layout_out->button_bit_count += ctx.report_count;
                    } else {
                        for (int i = 0; i < ctx.usage_count; i++) {
                            uint8_t usage = ctx.usages[i];
                            switch (ctx.usage_page) {
                                case 0x01:  // Generic Desktop Controls
                                    switch (usage) {
                                        case 0x39:
                                            layout_out->has_dpad        = true;
                                            layout_out->dpad_bit_offset = ctx.bit_offset;
                                            layout_out->dpad_bit_size   = ctx.report_size;
                                            break;
                                        case 0x30:  // LX
                                            layout_out->has_lx        = true;
                                            layout_out->lx_bit_offset = ctx.bit_offset;
                                            layout_out->lx_bit_size   = ctx.report_size;
                                            break;
                                        case 0x31:  // LY
                                            layout_out->has_ly        = true;
                                            layout_out->ly_bit_offset = ctx.bit_offset;
                                            layout_out->ly_bit_size   = ctx.report_size;
                                            break;
                                        case 0x32:  // RX
                                            layout_out->has_rx        = true;
                                            layout_out->rx_bit_offset = ctx.bit_offset;
                                            layout_out->rx_bit_size   = ctx.report_size;
                                            break;
                                        case 0x35:  // RY
                                            layout_out->has_ry        = true;
                                            layout_out->ry_bit_offset = ctx.bit_offset;
                                            layout_out->ry_bit_size   = ctx.report_size;
                                            break;
                                    }
                                    break;

                                case 0x02:  // Simulation Controls (triggers)
                                    if (usage == 0xC5) {
                                        layout_out->has_lt        = true;
                                        layout_out->lt_bit_offset = ctx.bit_offset;
                                        layout_out->lt_bit_size   = ctx.report_size;
                                    } else if (usage == 0xC4) {
                                        layout_out->has_rt        = true;
                                        layout_out->rt_bit_offset = ctx.bit_offset;
                                        layout_out->rt_bit_size   = ctx.report_size;
                                    }
                                    break;
                            }
                        }
                    }

                    ctx.bit_offset  += ctx.report_size * ctx.report_count;
                    ctx.usage_count = 0;  // clear usage stack after each INPUT
                }
                break;
        }
    }

    return layout_out->has_dpad || layout_out->has_buttons || layout_out->has_lx;
}

esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto) {
    if (HID_PROTOCOL_KEYBOARD == proto) {
        ESP_LOGI(TAG, "Keyboard uses generic (boot) driver");
        return ESP_OK;
    }

    if (HID_PROTOCOL_MOUSE == proto) {
        ESP_LOGI(TAG, "Mouse driver analysing");
        ESP_LOG_BUFFER_HEX(TAG, desc, desc_len);

        mouse_field_layout_t layout = {0};

        if (analyze_mouse_layout(desc, desc_len, &layout)) {
            ESP_LOGI(TAG, "Parsed mouse layout:");
            ESP_LOGI(TAG, "  Buttons: offset %u bits, count %u", layout.button_bit_offset, layout.button_bit_count);
            if (layout.has_x)
                ESP_LOGI(TAG, "  X: offset %u bits, size %u bits", layout.x_bit_offset, layout.x_bit_size);
            if (layout.has_y)
                ESP_LOGI(TAG, "  Y: offset %u bits, size %u bits", layout.y_bit_offset, layout.y_bit_size);
            if (layout.has_scroll)
                ESP_LOGI(TAG, "  Scroll: offset %u bits, size %u bits", layout.scroll_bit_offset,
                         layout.scroll_bit_size);
            if (layout.has_tilt)
                ESP_LOGI(TAG, "  Tilt: offset %u bits, size %u bits", layout.tilt_bit_offset, layout.tilt_bit_size);
        } else {
            ESP_LOGW(TAG, "Could not parse mouse layout");
        }
    } else {
        ESP_LOGI(TAG, "Gamepad driver analysing");
        ESP_LOG_BUFFER_HEX(TAG, desc, desc_len);

        gamepad_field_layout_t layout;
        if (analyze_gamepad_layout(desc, desc_len, &layout)) {
            ESP_LOGI(TAG, "Parsed gamepad layout:");
            if (layout.has_dpad) {
                ESP_LOGI(TAG, "  D-Pad: offset %d bits, size %d bits", layout.dpad_bit_offset, layout.dpad_bit_size);
            }

            if (layout.has_buttons) {
                ESP_LOGI(TAG, "  Buttons: offset %d bits, count %d", layout.button_bit_offset, layout.button_bit_count);
            }

            if (layout.has_lx && layout.has_ly) {
                ESP_LOGI(TAG, "  Left Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", layout.lx_bit_offset,
                         layout.lx_bit_size, layout.ly_bit_offset, layout.ly_bit_size);
            } else {
                if (layout.has_lx) {
                    ESP_LOGI(TAG, "  Left Stick X: offset %d bits, size %d bits", layout.lx_bit_offset,
                             layout.lx_bit_size);
                }
                if (layout.has_ly) {
                    ESP_LOGI(TAG, "  Left Stick Y: offset %d bits, size %d bits", layout.ly_bit_offset,
                             layout.ly_bit_size);
                }
            }

            if (layout.has_rx && layout.has_ry) {
                ESP_LOGI(TAG, "  Right Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", layout.rx_bit_offset,
                         layout.rx_bit_size, layout.ry_bit_offset, layout.ry_bit_size);
            } else {
                if (layout.has_rx) {
                    ESP_LOGI(TAG, "  Right Stick X: offset %d bits, size %d bits", layout.rx_bit_offset,
                             layout.rx_bit_size);
                }
                if (layout.has_ry) {
                    ESP_LOGI(TAG, "  Right Stick Y: offset %d bits, size %d bits", layout.ry_bit_offset,
                             layout.ry_bit_size);
                }
            }

            if (layout.has_lt) {
                ESP_LOGI(TAG, "  Trigger L: offset %d bits, size %d bits", layout.lt_bit_offset, layout.lt_bit_size);
            }

            if (layout.has_rt) {
                ESP_LOGI(TAG, "  Trigger R: offset %d bits, size %d bits", layout.rt_bit_offset, layout.rt_bit_size);
            }

        } else {
            ESP_LOGW(TAG, "Could not identify gamepad-compatible descriptor");
        }
    }

    return ESP_OK;
}
