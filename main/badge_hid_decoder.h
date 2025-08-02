#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "badge_hid_tokenizer.h"

#define MAX_USAGE_STACK 32

typedef struct {
    uint8_t usage_page;
    uint16_t usages[MAX_USAGE_STACK];
    uint8_t usage_count;

    bool has_usage_range;
    uint16_t usage_min;

    uint8_t report_size;
    uint8_t report_count;
    uint8_t report_id;

    uint16_t bit_offset;
} parser_ctx_t;

void hid_decoder_init(parser_ctx_t* decoder);
bool hid_decoder_process_item(parser_ctx_t* decoder, const hid_item_t* item);
