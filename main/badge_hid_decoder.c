#include "badge_hid_decoder.h"
#include <stdio.h>
#include <string.h>

void hid_decoder_init(parser_ctx_t* decoder) {
    memset(decoder, 0, sizeof(parser_ctx_t));
}

bool hid_decoder_process_item(parser_ctx_t* decoder, const hid_item_t* item) {
    switch (item->bType) {
        case HID_TYPE_MAIN:
            switch (item->bTag) {
                case HID_MAIN_INPUT:
                case HID_MAIN_OUTPUT:
                case HID_MAIN_FEATURE:
                    decoder->bit_offset  += decoder->report_size * decoder->report_count;
                    decoder->usage_count = 0;
                    break;

                case HID_MAIN_COLLECTION:
                case HID_MAIN_END_COLLECTION:
                    // Ignore for now
                    break;

                default:
                    break;
            }
            break;

        case HID_TYPE_GLOBAL:
            switch (item->bTag) {
                case HID_GLOBAL_USAGE_PAGE:
                    decoder->usage_page = (uint8_t)item->data;
                    break;
                case HID_GLOBAL_REPORT_SIZE:
                    decoder->report_size = (uint8_t)item->data;
                    break;
                case HID_GLOBAL_REPORT_COUNT:
                    decoder->report_count = (uint8_t)item->data;
                    break;
                case HID_GLOBAL_REPORT_ID:
                    decoder->report_id = (uint8_t)item->data;
                    break;
                default:
                    break;
            }
            break;

        case HID_TYPE_LOCAL:
            switch (item->bTag) {
                case HID_LOCAL_USAGE:
                    if (decoder->usage_count < MAX_USAGE_STACK) {
                        decoder->usages[decoder->usage_count++] = (uint8_t)item->data;
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    return true;
}
