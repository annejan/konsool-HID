#pragma once
#include <stdio.h>

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stdout, "[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) fprintf(stdout, "[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) fprintf(stdout, "[V][%s] " fmt "\n", tag, ##__VA_ARGS__)

static inline void ESP_LOG_BUFFER_HEX(const char* tag, const void* buffer, uint16_t buff_len) {
    const uint8_t* data = (const uint8_t*)buffer;
    fprintf(stdout, "[D][%s] HEX DUMP (%d bytes):\n", tag, buff_len);
    for (uint16_t i = 0; i < buff_len; ++i) {
        fprintf(stdout, "%02X%s", data[i], ((i + 1) % 16 == 0 || i + 1 == buff_len) ? "\n" : " ");
    }
}