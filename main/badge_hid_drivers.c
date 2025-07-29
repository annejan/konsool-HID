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
#include "esp_log.h"
#include "usb/hid.h"

static char const TAG[] = "BADGE_HID_DRIVER";

static int32_t extract_signed_bits(const uint8_t* data, uint16_t bit_offset, uint8_t bit_size) {
    uint32_t raw = 0;
    for (int i = 0; i < ((bit_size + 7) / 8) + 1; ++i) {
        raw |= data[(bit_offset / 8) + i] << (i * 8);
    }

    raw           >>= (bit_offset % 8);
    int32_t value = raw & ((1u << bit_size) - 1);

    // Sign extend
    if (value & (1u << (bit_size - 1))) {
        value |= ~((1u << bit_size) - 1);
    }

    return value;
}

static bool analyze_mouse_layout(const uint8_t* desc, int desc_len, mouse_field_layout_t* layout_out) {
    memset(layout_out, 0, sizeof(*layout_out));

    uint16_t bit_offset   = 0;
    uint8_t  usage_page   = 0;
    uint16_t usages[32]   = {0};
    int      usage_index  = 0;
    int      report_size  = 0;
    int      report_count = 0;

    for (int i = 0; i < desc_len;) {
        uint8_t b = desc[i++];
        if (b == 0xFE || i >= desc_len) break;

        int size = b & 0x03;
        int type = (b >> 2) & 0x03;
        int tag  = (b >> 4) & 0x0F;
        if (size == 3) size = 4;
        if (i + size > desc_len) break;

        uint32_t data = 0;
        for (int j = 0; j < size; j++) {
            data |= desc[i++] << (j * 8);
        }

        switch (type) {
            case 0x1:  // Global
                if (tag == 0x0)
                    usage_page = data;
                else if (tag == 0x7)
                    report_size = data;
                else if (tag == 0x9)
                    report_count = data;
                break;

            case 0x2:  // Local
                if (tag == 0x0 && usage_index < 32) usages[usage_index++] = data;
                break;

            case 0x0:              // Main
                if (tag == 0x8) {  // Input
                    if (usage_page == 0x01 || usage_page == 0x09 || usage_page == 0x0C) {
                        int fields = report_count;
                        int bits   = report_size;

                        int used = usage_index;
                        if (used > fields) used = fields;

                        for (int u = 0; u < used; u++) {
                            uint16_t usage = usages[u];
                            if (usage_page == 0x01) {
                                if (usage == 0x30) {
                                    layout_out->has_x        = true;
                                    layout_out->x_bit_offset = bit_offset;
                                    layout_out->x_bit_size   = bits;
                                } else if (usage == 0x31) {
                                    layout_out->has_y        = true;
                                    layout_out->y_bit_offset = bit_offset;
                                    layout_out->y_bit_size   = bits;
                                } else if (usage == 0x38) {
                                    layout_out->has_scroll        = true;
                                    layout_out->scroll_bit_offset = bit_offset;
                                    layout_out->scroll_bit_size   = bits;
                                } else if (usage == 0x48) {
                                    layout_out->has_tilt        = true;
                                    layout_out->tilt_bit_offset = bit_offset;
                                    layout_out->tilt_bit_size   = bits;
                                }
                            } else if (usage_page == 0x0C && usage == 0x0238) {
                                layout_out->has_tilt        = true;
                                layout_out->tilt_bit_offset = bit_offset;
                                layout_out->tilt_bit_size   = bits;
                            } else if (usage_page == 0x09 && usage >= 0x01 && usage <= 0x10) {
                                if (!layout_out->has_buttons) layout_out->button_bit_offset = bit_offset;
                                layout_out->has_buttons      = true;
                                layout_out->button_bit_count += 1;
                            }
                            bit_offset += bits;
                        }

                        // Account for unused fields (e.g. Button 3-16 with no Usage tags)
                        if (fields > used) {
                            bit_offset += (fields - used) * bits;
                        }

                        usage_index = 0;
                        memset(usages, 0, sizeof(usages));
                    }
                }
                break;
        }
    }

    return layout_out->has_x && layout_out->has_y;
}

static bool analyze_gamepad_layout(const uint8_t* desc, int desc_len, gamepad_field_layout_t* layout_out) {
    memset(layout_out, 0, sizeof(*layout_out));

    uint16_t bit_offset  = 0;
    uint8_t  usage_page  = 0;
    uint16_t usages[32]  = {0};
    int      usage_index = 0;

    // Check for Report ID and adjust initial bit offset
    for (int pre = 0; pre < desc_len - 1; pre++) {
        if ((desc[pre] & 0xFC) == 0x84) {
            bit_offset = 8;
            break;
        }
    }

    for (int i = 0; i < desc_len;) {
        uint8_t b = desc[i++];
        if (b == 0xFE || i >= desc_len) break;  // long item or overrun

        int size = b & 0x03;
        int type = (b >> 2) & 0x03;
        int tag  = (b >> 4) & 0x0F;
        if (size == 3) size = 4;
        if (i + size > desc_len) break;

        uint32_t data = 0;
        for (int j = 0; j < size; j++) {
            data |= desc[i++] << (j * 8);
        }

        switch (type) {
            case 0x1:  // Global
                if (tag == 0x0) usage_page = data;
                break;

            case 0x2:  // Local
                if (tag == 0x0 && usage_index < (int)(sizeof(usages) / sizeof(usages[0]))) usages[usage_index++] = data;
                break;

            case 0x0:              // Main
                if (tag == 0x8) {  // Input
                    int field_count = 1;
                    int field_size  = 0;

                    // Go backwards to find report count and size
                    for (int back = i - size - 2; back > 0; back--) {
                        if ((desc[back] & 0xFC) == 0x94) field_count = desc[back + 1];
                        if ((desc[back] & 0xFC) == 0x74) field_size = desc[back + 1];
                        if (field_count && field_size) break;
                    }

                    for (int u = 0; u < usage_index; u++) {
                        uint16_t usage = usages[u];

                        if (usage_page == 0x01) {  // Generic Desktop
                            if (usage == 0x39) {
                                layout_out->has_dpad        = true;
                                layout_out->dpad_bit_offset = bit_offset;
                                layout_out->dpad_bit_size   = field_size;
                            } else if (usage == 0x30) {
                                layout_out->has_lx        = true;
                                layout_out->lx_bit_offset = bit_offset;
                                layout_out->lx_bit_size   = field_size;
                            } else if (usage == 0x31) {
                                layout_out->has_ly        = true;
                                layout_out->ly_bit_offset = bit_offset;
                                layout_out->ly_bit_size   = field_size;
                            } else if (usage == 0x32) {
                                layout_out->has_rx        = true;
                                layout_out->rx_bit_offset = bit_offset;
                                layout_out->rx_bit_size   = field_size;
                            } else if (usage == 0x35) {
                                layout_out->has_ry        = true;
                                layout_out->ry_bit_offset = bit_offset;
                                layout_out->ry_bit_size   = field_size;
                            }
                        } else if (usage_page == 0x09 && usage >= 0x01 && usage <= 0x20) {  // Buttons
                            if (!layout_out->has_buttons) layout_out->button_bit_offset = bit_offset;
                            layout_out->has_buttons      = true;
                            layout_out->button_bit_count += 1;
                        } else if (usage_page == 0x02 && (usage == 0xC5 || usage == 0xC4)) {  // Triggers
                            if (!layout_out->has_lt) {
                                layout_out->has_lt        = true;
                                layout_out->lt_bit_offset = bit_offset;
                                layout_out->lt_bit_size   = field_size;
                            } else {
                                layout_out->has_rt        = true;
                                layout_out->rt_bit_offset = bit_offset;
                                layout_out->rt_bit_size   = field_size;
                            }
                        }
                    }

                    bit_offset  += field_count * field_size;
                    usage_index = 0;
                }
                break;
        }
    }

    return layout_out->has_dpad && layout_out->has_buttons;
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
        ESP_LOGI(TAG, "Gamepad driver analyusing");
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
