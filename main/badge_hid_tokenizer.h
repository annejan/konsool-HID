#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// HID item formats
#define HID_ITEM_FORMAT_SHORT 0
#define HID_ITEM_FORMAT_LONG  1

// HID item types
#define HID_TYPE_MAIN     0
#define HID_TYPE_GLOBAL   1
#define HID_TYPE_LOCAL    2
#define HID_TYPE_RESERVED 3

#define HID_USAGE_PAGE_UNDEFINED       0x00
#define HID_USAGE_PAGE_GENERIC_DESKTOP 0x01
#define HID_USAGE_PAGE_SIMULATION      0x02
#define HID_USAGE_PAGE_BUTTON          0x09
#define HID_USAGE_PAGE_CONSUMER        0x0C
#define HID_USAGE_PAGE_DIGITIZER       0x0D
#define HID_USAGE_PAGE_VENDOR          0xFF

// HID main item tags
#define HID_MAIN_INPUT          0x08
#define HID_MAIN_OUTPUT         0x09
#define HID_MAIN_FEATURE        0x0B
#define HID_MAIN_COLLECTION     0x0A
#define HID_MAIN_END_COLLECTION 0x0C

// HID global item tags
#define HID_GLOBAL_USAGE_PAGE    0x00
#define HID_GLOBAL_LOGICAL_MIN   0x01
#define HID_GLOBAL_LOGICAL_MAX   0x02
#define HID_GLOBAL_PHYSICAL_MIN  0x03
#define HID_GLOBAL_PHYSICAL_MAX  0x04
#define HID_GLOBAL_UNIT_EXPONENT 0x05
#define HID_GLOBAL_UNIT          0x06
#define HID_GLOBAL_REPORT_SIZE   0x07
#define HID_GLOBAL_REPORT_ID     0x08
#define HID_GLOBAL_REPORT_COUNT  0x09
#define HID_GLOBAL_PUSH          0x0A
#define HID_GLOBAL_POP           0x0B

// HID local item tags
#define HID_LOCAL_USAGE              0x00
#define HID_LOCAL_USAGE_MINIMUM      0x01
#define HID_LOCAL_USAGE_MAXIMUM      0x02
#define HID_LOCAL_DESIGNATOR_INDEX   0x03
#define HID_LOCAL_DESIGNATOR_MINIMUM 0x04
#define HID_LOCAL_DESIGNATOR_MAXIMUM 0x05
#define HID_LOCAL_STRING_INDEX       0x07
#define HID_LOCAL_STRING_MINIMUM     0x08
#define HID_LOCAL_STRING_MAXIMUM     0x09
#define HID_LOCAL_DELIMITER          0x0A

#define HID_USAGE_POINTER               0x01
#define HID_USAGE_MOUSE                 0x02
#define HID_USAGE_JOYSTICK              0x04
#define HID_USAGE_GAMEPAD               0x05
#define HID_USAGE_KEYBOARD              0x06
#define HID_USAGE_KEYPAD                0x07
#define HID_USAGE_MULTI_AXIS_CONTROLLER 0x08

#define HID_USAGE_X          0x30
#define HID_USAGE_Y          0x31
#define HID_USAGE_Z          0x32
#define HID_USAGE_RX         0x33
#define HID_USAGE_RY         0x34
#define HID_USAGE_RZ         0x35
#define HID_USAGE_SLIDER     0x36
#define HID_USAGE_DIAL       0x37
#define HID_USAGE_WHEEL      0x38
#define HID_USAGE_AC_PAN     0x0238
#define HID_USAGE_TILT       0x3D

// Global items
#define HID_TAG_USAGE_PAGE    0x00
#define HID_TAG_LOGICAL_MIN   0x01
#define HID_TAG_LOGICAL_MAX   0x02
#define HID_TAG_PHYSICAL_MIN  0x03
#define HID_TAG_PHYSICAL_MAX  0x04
#define HID_TAG_UNIT_EXPONENT 0x05
#define HID_TAG_UNIT          0x06
#define HID_TAG_REPORT_SIZE   0x07
#define HID_TAG_REPORT_ID     0x08
#define HID_TAG_REPORT_COUNT  0x09

// Local items
#define HID_TAG_USAGE          0x00
#define HID_TAG_USAGE_MINIMUM  0x01
#define HID_TAG_USAGE_MAXIMUM  0x02

// Main items
#define HID_TAG_INPUT          0x08
#define HID_TAG_OUTPUT         0x09
#define HID_TAG_FEATURE        0x0B
#define HID_TAG_COLLECTION     0x0A
#define HID_TAG_END_COLLECTION 0x0C

typedef struct {
    uint8_t        bTag;
    uint8_t        bType;
    uint8_t        bSize;
    uint8_t        format;
    uint32_t       data;
    const uint8_t* raw;
} hid_item_t;

typedef struct {
    const uint8_t* start;
    const uint8_t* end;
    const uint8_t* ptr;
} hid_tokenizer_state_t;

// Advances the tokenizer to the next HID item
// Returns true if an item was read successfully, false at end or on error
bool hid_next_item(hid_tokenizer_state_t* state, hid_item_t* item);

#ifdef __cplusplus
}
#endif
