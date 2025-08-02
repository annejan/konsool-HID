#include <assert.h>
#include <stdio.h>
#include "badge_hid_drivers.h"
#include "esp_log.h"
#include "test_descriptors.h"

static char const TAG[] = "HID Test";

void print_layout(gamepad_field_layout_t* layout) {
    
     ESP_LOGI(TAG, "Parsed gamepad layout:");
            if (layout->has_dpad) {
                ESP_LOGI(TAG, "  D-Pad: offset %d bits, size %d bits", layout->dpad_bit_offset, layout->dpad_bit_size);
            }

            if (layout->has_buttons) {
                ESP_LOGI(TAG, "  Buttons: offset %d bits, count %d", layout->button_bit_offset, layout->button_bit_count);
            }

            if (layout->has_lx && layout->has_ly) {
                ESP_LOGI(TAG, "  Left Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", layout->lx_bit_offset,
                         layout->lx_bit_size, layout->ly_bit_offset, layout->ly_bit_size);
            } else {
                if (layout->has_lx) {
                    ESP_LOGI(TAG, "  Left Stick X: offset %d bits, size %d bits", layout->lx_bit_offset,
                             layout->lx_bit_size);
                }
                if (layout->has_ly) {
                    ESP_LOGI(TAG, "  Left Stick Y: offset %d bits, size %d bits", layout->ly_bit_offset,
                             layout->ly_bit_size);
                }
            }

            if (layout->has_rx && layout->has_ry) {
                ESP_LOGI(TAG, "  Right Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", layout->rx_bit_offset,
                         layout->rx_bit_size, layout->ry_bit_offset, layout->ry_bit_size);
            } else {
                if (layout->has_rx) {
                    ESP_LOGI(TAG, "  Right Stick X: offset %d bits, size %d bits", layout->rx_bit_offset,
                             layout->rx_bit_size);
                }
                if (layout->has_ry) {
                    ESP_LOGI(TAG, "  Right Stick Y: offset %d bits, size %d bits", layout->ry_bit_offset,
                             layout->ry_bit_size);
                }
            }

            if (layout->has_lt) {
                ESP_LOGI(TAG, "  Trigger L: offset %d bits, size %d bits", layout->lt_bit_offset, layout->lt_bit_size);
            }

            if (layout->has_rt) {
                ESP_LOGI(TAG, "  Trigger R: offset %d bits, size %d bits", layout->rt_bit_offset, layout->rt_bit_size);
            }
}

int main(void) {
    mouse_field_layout_t mouse_layout = {0};
    assert(analyze_mouse_layout(mouse1_desc, mouse1_len, &mouse_layout));
    
    assert(mouse_layout.has_buttons);
    assert(mouse_layout.has_x);
    assert(mouse_layout.has_y);
    assert(mouse_layout.has_scroll);
    assert(mouse_layout.has_tilt);
    
    assert(16 == mouse_layout.button_bit_count);
    assert(0 == mouse_layout.button_bit_offset);

    assert(12 == mouse_layout.x_bit_size);
    assert(16 == mouse_layout.x_bit_offset);
    
    assert(12 == mouse_layout.y_bit_size);
    assert(28 == mouse_layout.y_bit_offset);
    
    assert(8 == mouse_layout.scroll_bit_size);
    assert(40 == mouse_layout.scroll_bit_offset);
    
    assert(8 == mouse_layout.tilt_bit_size);
    assert(48 == mouse_layout.tilt_bit_offset);

    printf("Logitech M705 passed\n");

    assert(analyze_mouse_layout(mouse2_desc, mouse2_len, &mouse_layout));

    assert(mouse_layout.has_buttons);
    assert(mouse_layout.has_x);
    assert(mouse_layout.has_y);
    assert(mouse_layout.has_scroll);
    assert(!mouse_layout.has_tilt);  // Trust mouse lacks tilt

    assert(5 == mouse_layout.button_bit_count);
    assert(0 == mouse_layout.button_bit_offset);

    assert(8 == mouse_layout.x_bit_size);
    assert(8 == mouse_layout.x_bit_offset);

    assert(8 == mouse_layout.y_bit_size);
    assert(16 == mouse_layout.y_bit_offset);

    assert(8 == mouse_layout.scroll_bit_size);
    assert(24 == mouse_layout.scroll_bit_offset);

    printf("Trust Kuza passed\n");

    assert(analyze_mouse_layout(mouse3_desc, mouse3_len, &mouse_layout));

    assert(mouse_layout.has_buttons);
    assert(mouse_layout.has_x);
    assert(mouse_layout.has_y);
    assert(mouse_layout.has_scroll);
    assert(!mouse_layout.has_tilt); 

    assert(3 == mouse_layout.button_bit_count);
    assert(0 == mouse_layout.button_bit_offset);

    assert(8 == mouse_layout.x_bit_size);
    assert(8 == mouse_layout.x_bit_offset);

    assert(8 == mouse_layout.y_bit_size);
    assert(16 == mouse_layout.y_bit_offset);

    assert(8 == mouse_layout.scroll_bit_size);
    assert(24 == mouse_layout.scroll_bit_offset);

    printf("Fujitsu M520 passed\n");

    gamepad_field_layout_t pad_layout = {0};
    assert(analyze_gamepad_layout(gamepad1_desc, gamepad1_len, &pad_layout));
    print_layout(&pad_layout);

    assert(pad_layout.has_dpad);
    assert(pad_layout.has_buttons);
    assert(pad_layout.has_lx);
    assert(pad_layout.has_ly);
    assert(pad_layout.has_rx);
    assert(pad_layout.has_ry);
    assert(pad_layout.has_lt);
    assert(pad_layout.has_rt);

    assert(20 == pad_layout.dpad_bit_offset);
    assert(4  == pad_layout.dpad_bit_size);

    assert(24 == pad_layout.button_bit_offset);
    assert(15 == pad_layout.button_bit_count);

    assert(40 == pad_layout.lx_bit_offset);
    assert(8  == pad_layout.lx_bit_size);

    assert(48 == pad_layout.ly_bit_offset);
    assert(8  == pad_layout.ly_bit_size);

    assert(64 == pad_layout.rx_bit_offset);
    assert(8  == pad_layout.rx_bit_size);

    assert(72 == pad_layout.ry_bit_offset);
    assert(8  == pad_layout.ry_bit_size);

    assert(88 == pad_layout.lt_bit_offset);
    assert(8  == pad_layout.lt_bit_size);

    assert(96 == pad_layout.rt_bit_offset);
    assert(8  == pad_layout.rt_bit_size);

    printf("Stadia controller passed\n");

    return 0;
}
