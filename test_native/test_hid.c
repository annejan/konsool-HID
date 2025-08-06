#include <assert.h>
#include <stdio.h>
#include "badge_hid_drivers.h"
#include "esp_log.h"
#include "test_descriptors.h"

static char const TAG[] = "HID Test";

void print_layout(gamepad_field_layout_t* layout) {

    ESP_LOGI(TAG, "Parsed gamepad layout:");
    if (layout->dpad.present) {
        ESP_LOGI(TAG, "  D-Pad: offset %d bits, size %d bits", layout->dpad.offset, layout->dpad.size);
    }

    if (layout->buttons.present) {
        ESP_LOGI(TAG, "  Buttons: offset %d bits, count %d", layout->buttons.offset, layout->buttons.size);
    }

    if (layout->lx.present && layout->ly.present) {
        ESP_LOGI(TAG, "  Left Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", layout->lx.offset, layout->lx.size,
                 layout->ly.offset, layout->ly.size);
    } else {
        if (layout->lx.present) {
            ESP_LOGI(TAG, "  Left Stick X: offset %d bits, size %d bits", layout->lx.offset, layout->lx.size);
        }
        if (layout->ly.present) {
            ESP_LOGI(TAG, "  Left Stick Y: offset %d bits, size %d bits", layout->ly.offset, layout->ly.size);
        }
    }

    if (layout->rx.present && layout->ry.present) {
        ESP_LOGI(TAG, "  Right Stick: X@%dbits (%d bits), Y@%dbits (%d bits)", layout->rx.offset, layout->rx.size,
                 layout->ry.offset, layout->ry.size);
    } else {
        if (layout->rx.present) {
            ESP_LOGI(TAG, "  Right Stick X: offset %d bits, size %d bits", layout->rx.offset, layout->rx.size);
        }
        if (layout->ry.present) {
            ESP_LOGI(TAG, "  Right Stick Y: offset %d bits, size %d bits", layout->ry.offset, layout->ry.size);
        }
    }

    if (layout->lt.present) {
        ESP_LOGI(TAG, "  Trigger L: offset %d bits, size %d bits", layout->lt.offset, layout->lt.size);
    }

    if (layout->rt.present) {
        ESP_LOGI(TAG, "  Trigger R: offset %d bits, size %d bits", layout->rt.offset, layout->rt.size);
    }
}

int main(void) {
    mouse_field_layout_t mouse_layout = {0};
    assert(ESP_OK == analyze_mouse_layout(mouse1_desc, mouse1_len, &mouse_layout));

    assert(mouse_layout.buttons.present);
    assert(mouse_layout.x.present);
    assert(mouse_layout.y.present);
    assert(mouse_layout.scroll.present);
    assert(mouse_layout.tilt.present);

    assert(16 == mouse_layout.buttons.size);
    assert(0 == mouse_layout.buttons.offset);

    assert(12 == mouse_layout.x.size);
    assert(16 == mouse_layout.x.offset);

    assert(12 == mouse_layout.y.size);
    assert(28 == mouse_layout.y.offset);

    assert(8 == mouse_layout.scroll.size);
    assert(40 == mouse_layout.scroll.offset);

    assert(8 == mouse_layout.tilt.size);
    assert(48 == mouse_layout.tilt.offset);

    printf("Logitech M705 passed\n");

    assert(ESP_OK == analyze_mouse_layout(mouse2_desc, mouse2_len, &mouse_layout));

    assert(mouse_layout.buttons.present);
    assert(mouse_layout.x.present);
    assert(mouse_layout.y.present);
    assert(mouse_layout.scroll.present);
    assert(!mouse_layout.tilt.present);  // Trust mouse lacks tilt

    assert(5 == mouse_layout.buttons.size);
    assert(0 == mouse_layout.buttons.offset);

    assert(8 == mouse_layout.x.size);
    assert(8 == mouse_layout.x.offset);

    assert(8 == mouse_layout.y.size);
    assert(16 == mouse_layout.y.offset);

    assert(8 == mouse_layout.scroll.size);
    assert(24 == mouse_layout.scroll.offset);

    printf("Trust Kuza passed\n");

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

    printf("Fujitsu M520 passed\n");

    gamepad_field_layout_t pad_layout = {0};
    assert(ESP_OK == analyze_gamepad_layout(gamepad1_desc, gamepad1_len, &pad_layout));
    print_layout(&pad_layout);

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

    printf("Stadia controller passed\n");

    return 0;
}
