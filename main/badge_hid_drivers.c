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
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "usb/hid.h"

static char const TAG[] = "BADGE_HID_DRIVER";

static mouse_field_layout_t   mouse_layout   = {0};
static gamepad_field_layout_t gamepad_layout = {0};

static uint32_t extract_unsigned_bits(const uint8_t* data, int bit_offset, int bit_size) {
    uint32_t value = 0;
    for (int i = 0; i < bit_size; i++) {
        int byte_index = (bit_offset + i) / 8;
        int bit_index  = (bit_offset + i) % 8;
        if (data[byte_index] & (1 << bit_index)) {
            value |= (1u << i);
        }
    }
    return value;
}

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

static void decode_hat_to_dpad(uint8_t hat, gamepad_report_t* rpt) {
    rpt->buttons.up    = (hat == 0x00 || hat == 0x01 || hat == 0x07);
    rpt->buttons.right = (hat == 0x01 || hat == 0x02 || hat == 0x03);
    rpt->buttons.down  = (hat == 0x03 || hat == 0x04 || hat == 0x05);
    rpt->buttons.left  = (hat == 0x05 || hat == 0x06 || hat == 0x07);
}

esp_err_t analyze_mouse_layout(const uint8_t* desc, int desc_len, mouse_field_layout_t* layout_out) {
    memset(layout_out, 0, sizeof(*layout_out));

    uint16_t bit_offset   = 0;
    uint8_t  usage_page   = 0;
    uint16_t usages[32]   = {0};
    int      usage_index  = 0;
    int      report_size  = 0;
    int      report_count = 0;
    uint16_t usage_min    = 0;
    uint16_t usage_max    = 0;

    for (int i = 0; i < desc_len;) {
        uint8_t b = desc[i++];
        if (b == HID_LONG_ITEM_PREFIX || i >= desc_len) break;

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
            case HID_TYPE_GLOBAL:
                if (tag == HID_TAG_USAGE_PAGE)
                    usage_page = data;
                else if (tag == HID_TAG_REPORT_SIZE)
                    report_size = data;
                else if (tag == HID_TAG_REPORT_COUNT)
                    report_count = data;
                else if (tag == HID_TAG_REPORT_ID) {
                    layout_out->report_id = data;
                    bit_offset            += 8;
                }
                break;
                ;

            case HID_TYPE_LOCAL:
                if (tag == HID_TAG_USAGE && usage_index < 32)
                    usages[usage_index++] = data;
                else if (tag == HID_TAG_USAGE_MIN)
                    usage_min = data;
                else if (tag == HID_TAG_USAGE_MAX)
                    usage_max = data;
                break;

            case HID_TYPE_MAIN:
                if (tag == HID_TAG_INPUT) {
                    if (usage_page == 0x01 || usage_page == 0x09 || usage_page == 0x0C) {
                        int fields = report_count;
                        int bits   = report_size;

                        int used = usage_index;
                        if (used > fields) used = fields;

                        for (int u = 0; u < used; u++) {
                            uint16_t usage = usages[u];
                            if (usage_page == USAGE_PAGE_GENERIC_DESKTOP) {
                                if (usage == USAGE_X) {
                                    layout_out->x.present = true;
                                    layout_out->x.offset  = bit_offset;
                                    layout_out->x.size    = bits;
                                } else if (usage == USAGE_Y) {
                                    layout_out->y.present = true;
                                    layout_out->y.offset  = bit_offset;
                                    layout_out->y.size    = bits;
                                } else if (usage == USAGE_WHEEL) {
                                    layout_out->scroll.present = true;
                                    layout_out->scroll.offset  = bit_offset;
                                    layout_out->scroll.size    = bits;
                                } else if (usage == USAGE_TILT) {
                                    layout_out->tilt.present = true;
                                    layout_out->tilt.offset  = bit_offset;
                                    layout_out->tilt.size    = bits;
                                }
                            } else if (usage_page == USAGE_PAGE_CONSUMER && usage == USAGE_CONSUMER_TILT) {
                                layout_out->tilt.present = true;
                                layout_out->tilt.offset  = bit_offset;
                                layout_out->tilt.size    = bits;
                            } else if (usage_page == USAGE_PAGE_BUTTON) {
                                if (!layout_out->buttons.present) layout_out->buttons.offset = bit_offset;

                                layout_out->buttons.present = true;
                            }
                            bit_offset += bits;
                        }

                        if (usage_page == USAGE_PAGE_BUTTON) {
                            if (!layout_out->buttons.present) layout_out->buttons.offset = bit_offset;

                            layout_out->buttons.present = true;
                            if (usage_max) {
                                layout_out->buttons.size = usage_max;
                            }
                        }

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

    return layout_out->x.present && layout_out->y.present && layout_out->buttons.present ? ESP_OK : ESP_FAIL;
}

esp_err_t analyze_gamepad_layout(const uint8_t* desc, const int desc_len, gamepad_field_layout_t* layout_out) {
    memset(layout_out, 0, sizeof(*layout_out));

    uint16_t bit_offset  = 0;
    uint8_t  usage_page  = 0;
    uint16_t usages[32]  = {0};
    int      usage_index = 0;

    uint8_t report_size  = 0;
    uint8_t report_count = 0;

    bool last_input_was_button_array = false;

    for (int i = 0; i < desc_len;) {
        uint8_t b = desc[i++];
        if (b == HID_LONG_ITEM_PREFIX || i >= desc_len) break;

        int size = b & 0x03;
        if (size == 3) size = 4;
        int type = (b >> 2) & 0x03;
        int tag  = (b >> 4) & 0x0F;
        if (i + size > desc_len) break;

        uint32_t data = 0;
        for (int j = 0; j < size; j++) {
            data |= ((uint32_t)desc[i++]) << (j * 8);
        }

        // Reset usage list when starting new section
        if (type == HID_TYPE_GLOBAL && (tag == HID_TAG_USAGE_PAGE || tag == HID_TAG_REPORT_ID)) {
            usage_index = 0;
        }

        switch (type) {
            case HID_TYPE_GLOBAL:
                switch (tag) {
                    case HID_TAG_USAGE_PAGE:
                        usage_page = data;
                        break;
                    case HID_TAG_REPORT_SIZE:
                        report_size = data;
                        break;
                    case HID_TAG_REPORT_COUNT:
                        report_count = data;
                        break;
                    case HID_TAG_REPORT_ID:
                        layout_out->report_id = data;
                        bit_offset            = 8;
                        break;
                }
                break;

            case HID_TYPE_LOCAL:
                if (tag == HID_TAG_USAGE && usage_index < 32) {
                    usages[usage_index++] = data;
                }
                break;

            case HID_TYPE_MAIN:
                if (tag == HID_TAG_INPUT) {
                    bool is_constant = (data & 0x01) != 0;

                    if (usage_page == USAGE_PAGE_BUTTON && usage_index >= 1 && usages[0] >= USAGE_BUTTON_MIN &&
                        usages[0] <= USAGE_BUTTON_MAX) {
                        if (!layout_out->buttons.present) layout_out->buttons.offset = bit_offset;
                        layout_out->buttons.present = true;
                        layout_out->buttons.size    += report_count;
                        last_input_was_button_array = true;
                        bit_offset                  += report_size * report_count;
                    } else if (usage_page == USAGE_PAGE_BUTTON && usage_index == 0 && last_input_was_button_array) {
                        bit_offset += report_size * report_count;
                    } else {
                        for (int u = 0; u < report_count; u++) {
                            uint16_t usage =
                                (u < usage_index) ? usages[u] : (usage_index > 0 ? usages[usage_index - 1] : 0xFFFF);
                            if (usage == 0xFFFF) {
                                bit_offset += report_size;
                                continue;
                            }

                            if (usage_page == USAGE_PAGE_GENERIC_DESKTOP) {
                                switch (usage) {
                                    case USAGE_HATSWITCH:
                                        layout_out->dpad.present = true;
                                        layout_out->dpad.offset  = bit_offset;
                                        layout_out->dpad.size    = report_size;
                                        break;
                                    case USAGE_X:
                                        layout_out->lx.present = true;
                                        layout_out->lx.offset  = bit_offset;
                                        layout_out->lx.size    = report_size;
                                        break;
                                    case USAGE_Y:
                                        layout_out->ly.present = true;
                                        layout_out->ly.offset  = bit_offset;
                                        layout_out->ly.size    = report_size;
                                        break;
                                    case USAGE_Z:
                                        layout_out->rx.present = true;
                                        layout_out->rx.offset  = bit_offset;
                                        layout_out->rx.size    = report_size;
                                        break;
                                    case USAGE_RZ:
                                        layout_out->ry.present = true;
                                        layout_out->ry.offset  = bit_offset;
                                        layout_out->ry.size    = report_size;
                                        break;
                                }
                            } else if (usage_page == USAGE_PAGE_SIMULATION &&
                                       (usage == USAGE_ACCELERATOR || usage == USAGE_BRAKE)) {
                                if (!layout_out->lt.present) {
                                    layout_out->lt.present = true;
                                    layout_out->lt.offset  = bit_offset;
                                    layout_out->lt.size    = report_size;
                                } else {
                                    layout_out->rt.present = true;
                                    layout_out->rt.offset  = bit_offset;
                                    layout_out->rt.size    = report_size;
                                }
                            }

                            bit_offset += report_size;
                        }

                        last_input_was_button_array = false;
                    }

                    // Clear usages after each input block
                }
                usage_index = 0;

                break;
        }
    }

    return ESP_OK;
}

mouse_report_t handle_mouse_event(const uint8_t* data, const int length) {
    return parse_mouse_report(data, length, &mouse_layout);
}

mouse_report_t parse_mouse_report(const uint8_t* data, const int length, mouse_field_layout_t* layout) {

    // ESP_LOG_BUFFER_HEX(TAG, data, length);

    mouse_report_t report = {0};
    if (!layout) {
        ESP_LOGW(TAG, "No layout for mouse!");
        return report;
    }

    if (layout->buttons.present) {
        report.buttons.val = extract_unsigned_bits(data, layout->buttons.offset, layout->buttons.size);
    }

    if (layout->x.present) {
        report.x_displacement = extract_signed_bits(data, layout->x.offset, layout->x.size);
    }

    if (layout->y.present) {
        report.y_displacement = extract_signed_bits(data, layout->y.offset, layout->y.size);
    }

    if (layout->scroll.present) {
        report.scroll = extract_signed_bits(data, layout->scroll.offset, layout->scroll.size);
    }

    if (layout->tilt.present) {
        report.tilt = extract_signed_bits(data, layout->tilt.offset, layout->tilt.size);
    }

    return report;
}

gamepad_report_t handle_gamepad_event(const uint8_t* data, const int length) {
    return parse_gamepad_report(data, length, &gamepad_layout);
}

gamepad_report_t parse_gamepad_report(const uint8_t* data, int length, gamepad_field_layout_t* layout) {

    ESP_LOG_BUFFER_HEX(TAG, data, length);

    gamepad_report_t report = {0};
    if (!layout) {
        ESP_LOGW(TAG, "No layout for gamepad!");
        return report;
    }
    if (layout->dpad.present) {
        uint8_t hat = extract_unsigned_bits(data, layout->dpad.offset, layout->dpad.size);
        decode_hat_to_dpad(hat, &report);
    }

    if (layout->buttons.present) {
        uint32_t btn_bits  = extract_unsigned_bits(data, layout->buttons.offset, layout->buttons.size);
        // Preserve bits that were already set by decode_hat_to_dpad()
        btn_bits           |= report.buttons.val;
        report.buttons.val = btn_bits;
    }

    if (layout->lx.present) {
        report.lx = extract_signed_bits(data, layout->lx.offset, layout->lx.size);
    }

    if (layout->ly.present) {
        report.ly = extract_signed_bits(data, layout->ly.offset, layout->ly.size);
    }

    if (layout->rx.present) {
        report.rx = extract_signed_bits(data, layout->rx.offset, layout->rx.size);
    }

    if (layout->ry.present) {
        report.ry = extract_signed_bits(data, layout->ry.offset, layout->ry.size);
    }

    if (layout->lt.present) {
        report.lt = extract_signed_bits(data, layout->lt.offset, layout->lt.size);
    }

    if (layout->rt.present) {
        report.rt = extract_signed_bits(data, layout->rt.offset, layout->rt.size);
    }

    return report;
}

esp_err_t decode_descriptor_register_driver(const uint8_t* const desc, const int desc_len, const uint8_t proto) {
    if (HID_PROTOCOL_KEYBOARD == proto) {
        ESP_LOGI(TAG, "Keyboard uses generic (boot) driver");
        return ESP_OK;
    }

    if (HID_PROTOCOL_MOUSE == proto) {
        ESP_LOGI(TAG, "Mouse driver analysing");
        ESP_LOG_BUFFER_HEX(TAG, desc, desc_len);
        if (ESP_OK == analyze_mouse_layout(desc, desc_len, &mouse_layout)) {
            ESP_LOGI(TAG, "Parsed mouse layout:");
            ESP_LOGI(TAG, "  Buttons: offset %u bits, count %u", mouse_layout.buttons.offset,
                     mouse_layout.buttons.size);
            if (mouse_layout.x.present)
                ESP_LOGI(TAG, "  X: offset %u bits, size %u bits", mouse_layout.x.offset, mouse_layout.x.size);
            if (mouse_layout.y.offset)
                ESP_LOGI(TAG, "  Y: offset %u bits, size %u bits", mouse_layout.y.offset, mouse_layout.y.size);
            if (mouse_layout.scroll.present)
                ESP_LOGI(TAG, "  Scroll: offset %u bits, size %u bits", mouse_layout.scroll.offset,
                         mouse_layout.scroll.size);
            if (mouse_layout.tilt.present)
                ESP_LOGI(TAG, "  Tilt: offset %u bits, size %u bits", mouse_layout.tilt.offset, mouse_layout.tilt.size);
        } else {
            ESP_LOGW(TAG, "Could not parse mouse layout");
        }
    } else {
        ESP_LOGI(TAG, "Gamepad driver analysing");
        ESP_LOG_BUFFER_HEX(TAG, desc, desc_len);
        if (ESP_OK == analyze_gamepad_layout(desc, desc_len, &gamepad_layout)) {
            ESP_LOGI(TAG, "Parsed gamepad gamepad_layout:");
            if (gamepad_layout.dpad.present) {
                ESP_LOGI(TAG, "  D-Pad: offset %d bits, size %d bits", gamepad_layout.dpad.offset,
                         gamepad_layout.dpad.size);
            }

            if (gamepad_layout.buttons.present) {
                ESP_LOGI(TAG, "  Buttons: offset %d bits, count %d", gamepad_layout.buttons.offset,
                         gamepad_layout.buttons.size);
            }

            if (gamepad_layout.lx.present && gamepad_layout.ly.present) {
                ESP_LOGI(TAG, "  Left Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", gamepad_layout.lx.offset,
                         gamepad_layout.lx.size, gamepad_layout.ly.offset, gamepad_layout.ly.size);
            } else {
                if (gamepad_layout.lx.present) {
                    ESP_LOGI(TAG, "  Left Stick X: offset %d bits, size %d bits", gamepad_layout.lx.offset,
                             gamepad_layout.lx.size);
                }
                if (gamepad_layout.ly.present) {
                    ESP_LOGI(TAG, "  Left Stick Y: offset %d bits, size %d bits", gamepad_layout.ly.offset,
                             gamepad_layout.ly.size);
                }
            }

            if (gamepad_layout.rx.present && gamepad_layout.ry.present) {
                ESP_LOGI(TAG, "  Right Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", gamepad_layout.rx.offset,
                         gamepad_layout.rx.size, gamepad_layout.ry.offset, gamepad_layout.ry.size);
            } else {
                if (gamepad_layout.rx.present) {
                    ESP_LOGI(TAG, "  Right Stick X: offset %d bits, size %d bits", gamepad_layout.rx.offset,
                             gamepad_layout.rx.size);
                }
                if (gamepad_layout.ry.present) {
                    ESP_LOGI(TAG, "  Right Stick Y: offset %d bits, size %d bits", gamepad_layout.ry.offset,
                             gamepad_layout.ry.size);
                }
            }

            if (gamepad_layout.lt.present) {
                ESP_LOGI(TAG, "  Trigger L: offset %d bits, size %d bits", gamepad_layout.lt.offset,
                         gamepad_layout.lt.size);
            }

            if (gamepad_layout.rt.present) {
                ESP_LOGI(TAG, "  Trigger R: offset %d bits, size %d bits", gamepad_layout.rt.offset,
                         gamepad_layout.rt.size);
            }

        } else {
            ESP_LOGW(TAG, "Could not identify gamepad-compatible descriptor");
        }
    }

    return ESP_OK;
}
