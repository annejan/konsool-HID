// hid_tokenizer.c
#include "badge_hid_tokenizer.h"
#include <stddef.h>

bool hid_next_item(hid_tokenizer_state_t* state, hid_item_t* item) {
    if (!state || !item || state->ptr >= state->end) return false;

    const uint8_t* ptr = state->ptr;
    uint8_t        b   = *ptr++;

    // Long item
    if (b == 0xFE) {
        if (ptr + 2 > state->end) return false;

        item->format = HID_ITEM_FORMAT_LONG;
        item->bSize  = ptr[0];
        item->bType  = HID_TYPE_RESERVED;
        item->bTag   = ptr[1];
        item->raw    = state->ptr;

        ptr += 2;

        if (ptr + item->bSize > state->end) return false;

        // Copy data
        item->data = 0;
        for (int i = 0; i < item->bSize && i < 4; ++i) {
            item->data |= ((uint32_t)ptr[i]) << (8 * i);
        }

        ptr += item->bSize;
    }
    // Short item
    else {
        uint8_t size_code = b & 0x03;
        uint8_t type      = (b >> 2) & 0x03;
        uint8_t tag       = (b >> 4) & 0x0F;

        static const uint8_t size_lut[] = {0, 1, 2, 4};
        uint8_t              size       = size_lut[size_code];

        if (ptr + size > state->end) return false;

        item->format = HID_ITEM_FORMAT_SHORT;
        item->bSize  = size;
        item->bType  = type;
        item->bTag   = tag;
        item->raw    = state->ptr;

        item->data = 0;
        for (int i = 0; i < size; ++i) {
            item->data |= ((uint32_t)ptr[i]) << (8 * i);
        }

        ptr += size;
    }

    state->ptr = ptr;
    return true;
}
