#include <stdio.h>
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "bsp/power.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hal/lcd_types.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "portmacro.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"
#include "usb/usb_host.h"

// Constants
static char const TAG[] = "main";

// Global variables
static size_t                       display_h_res        = 0;
static size_t                       display_v_res        = 0;
static lcd_color_rgb_pixel_format_t display_color_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
static lcd_rgb_data_endian_t        display_data_endian  = LCD_RGB_DATA_ENDIAN_LITTLE;
static pax_buf_t                    fb                   = {0};
static QueueHandle_t                app_event_queue      = NULL;

#if defined(CONFIG_BSP_TARGET_KAMI)
#define BLACK 0
#define WHITE 1
#define RED   2
#else
#define BLACK 0xFF000000
#define WHITE 0xFFFFFFFF
#define RED   0xFFFF0000
#endif

#if defined(CONFIG_BSP_TARGET_KAMI)
static pax_col_t palette[] = {0xffffffff, 0xff000000, 0xffff0000};  // white, black, red
#endif

void blit(void) {
    bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(&fb));
}

/**
 * @brief APP event group
 *
 * Application logic can be different. There is a one among other ways to distinguish the
 * event by application event group.
 * In this example we have two event groups:
 * APP_EVENT            - General event, which is APP_QUIT_PIN press event (Generally, it is IO0).
 * APP_EVENT_HID_HOST   - HID Host Driver event, such as device connection/disconnection or input report.
 */
typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
    app_event_group_t event_group;
    /* HID Host - Device related info */
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t  event;
        void*                    arg;
    } hid_host_device;
} app_event_queue_t;

/**
 * @brief HID Protocol string names
 */
static const char* hid_proto_name_str[] = {"NONE", "KEYBOARD", "MOUSE"};

/**
 * @brief Key event
 */
typedef struct {
    enum key_state {
        KEY_STATE_PRESSED  = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

/* Main char symbol for ENTER key */
#define KEYBOARD_ENTER_MAIN_CHAR '\r'
/* When set to 1 pressing ENTER will be extending with LineFeed during serial debug output */
#define KEYBOARD_ENTER_LF_EXTEND 1

/**
 * @brief Scancode to ascii table
 */
const uint8_t keycode2ascii[57][2] = {
    {0, 0},                                               /* HID_KEY_NO_PRESS        */
    {0, 0},                                               /* HID_KEY_ROLLOVER        */
    {0, 0},                                               /* HID_KEY_POST_FAIL       */
    {0, 0},                                               /* HID_KEY_ERROR_UNDEFINED */
    {'a', 'A'},                                           /* HID_KEY_A               */
    {'b', 'B'},                                           /* HID_KEY_B               */
    {'c', 'C'},                                           /* HID_KEY_C               */
    {'d', 'D'},                                           /* HID_KEY_D               */
    {'e', 'E'},                                           /* HID_KEY_E               */
    {'f', 'F'},                                           /* HID_KEY_F               */
    {'g', 'G'},                                           /* HID_KEY_G               */
    {'h', 'H'},                                           /* HID_KEY_H               */
    {'i', 'I'},                                           /* HID_KEY_I               */
    {'j', 'J'},                                           /* HID_KEY_J               */
    {'k', 'K'},                                           /* HID_KEY_K               */
    {'l', 'L'},                                           /* HID_KEY_L               */
    {'m', 'M'},                                           /* HID_KEY_M               */
    {'n', 'N'},                                           /* HID_KEY_N               */
    {'o', 'O'},                                           /* HID_KEY_O               */
    {'p', 'P'},                                           /* HID_KEY_P               */
    {'q', 'Q'},                                           /* HID_KEY_Q               */
    {'r', 'R'},                                           /* HID_KEY_R               */
    {'s', 'S'},                                           /* HID_KEY_S               */
    {'t', 'T'},                                           /* HID_KEY_T               */
    {'u', 'U'},                                           /* HID_KEY_U               */
    {'v', 'V'},                                           /* HID_KEY_V               */
    {'w', 'W'},                                           /* HID_KEY_W               */
    {'x', 'X'},                                           /* HID_KEY_X               */
    {'y', 'Y'},                                           /* HID_KEY_Y               */
    {'z', 'Z'},                                           /* HID_KEY_Z               */
    {'1', '!'},                                           /* HID_KEY_1               */
    {'2', '@'},                                           /* HID_KEY_2               */
    {'3', '#'},                                           /* HID_KEY_3               */
    {'4', '$'},                                           /* HID_KEY_4               */
    {'5', '%'},                                           /* HID_KEY_5               */
    {'6', '^'},                                           /* HID_KEY_6               */
    {'7', '&'},                                           /* HID_KEY_7               */
    {'8', '*'},                                           /* HID_KEY_8               */
    {'9', '('},                                           /* HID_KEY_9               */
    {'0', ')'},                                           /* HID_KEY_0               */
    {KEYBOARD_ENTER_MAIN_CHAR, KEYBOARD_ENTER_MAIN_CHAR}, /* HID_KEY_ENTER           */
    {0, 0},                                               /* HID_KEY_ESC             */
    {'\b', 0},                                            /* HID_KEY_DEL             */
    {0, 0},                                               /* HID_KEY_TAB             */
    {' ', ' '},                                           /* HID_KEY_SPACE           */
    {'-', '_'},                                           /* HID_KEY_MINUS           */
    {'=', '+'},                                           /* HID_KEY_EQUAL           */
    {'[', '{'},                                           /* HID_KEY_OPEN_BRACKET    */
    {']', '}'},                                           /* HID_KEY_CLOSE_BRACKET   */
    {'\\', '|'},                                          /* HID_KEY_BACK_SLASH      */
    {'\\', '|'},
    /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
    {';', ':'},                    /* HID_KEY_COLON           */
    {'\'', '"'},                   /* HID_KEY_QUOTE           */
    {'`', '~'},                    /* HID_KEY_TILDE           */
    {',', '<'},                    /* HID_KEY_LESS            */
    {'.', '>'},                    /* HID_KEY_GREATER         */
    {'/', '?'}                     /* HID_KEY_SLASH           */
};

/**
 * @brief Makes new line depending on report output protocol type
 *
 * @param[in] proto Current protocol to output
 */
static void hid_print_new_device_report_header(hid_protocol_t proto) {
    static hid_protocol_t prev_proto_output = -1;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        printf("\r\n");
        if (proto == HID_PROTOCOL_MOUSE) {
            printf("Mouse\r\n");
        } else if (proto == HID_PROTOCOL_KEYBOARD) {
            printf("Keyboard\r\n");
        } else {
            printf("Generic\r\n");
        }
        fflush(stdout);
    }
}

/**
 * @brief HID Keyboard modifier verification for capitalization application (right or left shift)
 *
 * @param[in] modifier
 * @return true  Modifier was pressed (left or right shift)
 * @return false Modifier was not pressed (left or right shift)
 *
 */
static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier) {
    if (((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) || ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT)) {
        return true;
    }
    return false;
}

/**
 * @brief HID Keyboard get char symbol from key code
 *
 * @param[in] modifier  Keyboard modifier data
 * @param[in] key_code  Keyboard key code
 * @param[in] key_char  Pointer to key char data
 *
 * @return true  Key scancode converted successfully
 * @return false Key scancode unknown
 */
static inline bool hid_keyboard_get_char(uint8_t modifier, uint8_t key_code, unsigned char* key_char) {
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;

    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
        *key_char = keycode2ascii[key_code][mod];
    } else {
        // All other key pressed
        return false;
    }

    return true;
}

/**
 * @brief HID Keyboard print char symbol
 *
 * @param[in] key_char  Keyboard char to stdout
 */
static inline void hid_keyboard_print_char(unsigned int key_char) {
    if (!!key_char) {
        putchar(key_char);
#if (KEYBOARD_ENTER_LF_EXTEND)
        if (KEYBOARD_ENTER_MAIN_CHAR == key_char) {
            putchar('\n');
        }
#endif  // KEYBOARD_ENTER_LF_EXTEND
        fflush(stdout);
    }
}

/**
 * @brief Key Event. Key event with the key code, state and modifier.
 *
 * @param[in] key_event Pointer to Key Event structure
 *
 */
static void key_event_callback(key_event_t* key_event) {
    unsigned char key_char;

    hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

    if (KEY_STATE_PRESSED == key_event->state) {
        if (hid_keyboard_get_char(key_event->modifier, key_event->key_code, &key_char)) {

            hid_keyboard_print_char(key_char);
        }
    }
}

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t* const src, uint8_t key, unsigned int length) {
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_keyboard_report_callback(const uint8_t* const data, const int length) {
    hid_keyboard_input_report_boot_t* kb_report = (hid_keyboard_input_report_boot_t*)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = {0};
    key_event_t    key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {

        // key has been released verification
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED && !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state    = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        // key has been pressed verification
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state    = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host Mouse Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_mouse_report_callback(const uint8_t* const data, const int length) {
    hid_mouse_input_report_boot_t* mouse_report = (hid_mouse_input_report_boot_t*)data;

    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        return;
    }

    static int x_pos = 0;
    static int y_pos = 0;

    // Calculate absolute position from displacement
    x_pos += mouse_report->x_displacement;
    y_pos += mouse_report->y_displacement;

    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

    printf("X: %06d\tY: %06d\t|%c|%c|%c|\r", x_pos, y_pos, (mouse_report->buttons.button1 ? 'o' : ' '),
           (mouse_report->buttons.button3 ? 'o' : ' '), (mouse_report->buttons.button2 ? 'o' : ' '));
    fflush(stdout);

    char text[64];
    pax_simple_rect(&fb, WHITE, 0, 0, pax_buf_get_width(&fb), 72);
    snprintf(text, sizeof(text), "Mouse X: %06d\tY: %06d\t|%c|%c|%c|", x_pos, y_pos,
             (mouse_report->buttons.button1 ? 'o' : ' '), (mouse_report->buttons.button3 ? 'o' : ' '),
             (mouse_report->buttons.button2 ? 'o' : ' '));
    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 18, text);
    blit();
}

/**
 * @brief USB HID Host Generic Interface report callback handler
 *
 * 'generic' means anything else than mouse or keyboard
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_generic_report_callback(const uint8_t* const data, const int length) {
    hid_print_new_device_report_header(HID_PROTOCOL_NONE);
    for (int i = 0; i < length; i++) {
        printf("%02X", data[i]);
    }
    putchar('\r');
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle, const hid_host_interface_event_t event,
                                 void* arg) {
    uint8_t               data[64]    = {0};
    size_t                data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    char text[64];

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle, data, 64, &data_length));

            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    hid_host_keyboard_report_callback(data, data_length);
                } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                    hid_host_mouse_report_callback(data, data_length);
                }
            } else {
                hid_host_generic_report_callback(data, data_length);
            }

            break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED", hid_proto_name_str[dev_params.proto]);

            pax_simple_rect(&fb, WHITE, 0, 0, pax_buf_get_width(&fb), 72);
            snprintf(text, sizeof(text), "HID Device, protocol '%s' DISCONNECTED",
                     hid_proto_name_str[dev_params.proto]);
            pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 18, text);
            blit();

            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
            break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR", hid_proto_name_str[dev_params.proto]);

            pax_simple_rect(&fb, WHITE, 0, 0, pax_buf_get_width(&fb), 72);
            snprintf(text, sizeof(text), "HID Device, protocol '%s' TRANSFER_ERROR",
                     hid_proto_name_str[dev_params.proto]);
            pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 18, text);
            blit();

            break;
        default:
            ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event", hid_proto_name_str[dev_params.proto]);

            pax_simple_rect(&fb, WHITE, 0, 0, pax_buf_get_width(&fb), 72);
            snprintf(text, sizeof(text), "HID Device, protocol '%s' Unhandled event",
                     hid_proto_name_str[dev_params.proto]);
            pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 18, text);
            blit();

            break;
    }
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event, void* arg) {
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);

            pax_simple_rect(&fb, WHITE, 0, 0, pax_buf_get_width(&fb), 72);
            char text[64];
            snprintf(text, sizeof(text), "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);
            pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 18, text);
            blit();

            const hid_host_device_config_t dev_config = {.callback = hid_host_interface_callback, .callback_arg = NULL};

            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                }
            }
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            break;
        default:
            break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void* arg) {
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");

    pax_simple_rect(&fb, WHITE, 0, 0, pax_buf_get_width(&fb), 72);
    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 0, "USB shutdown");
    blit();

    // Clean up USB Host
    vTaskDelay(10);  // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event,
                              void* arg) {
    const app_event_queue_t evt_queue = {.event_group            = APP_EVENT_HID_HOST,
                                         // HID Host Device related info
                                         .hid_host_device.handle = hid_device_handle,
                                         .hid_host_device.event  = event,
                                         .hid_host_device.arg    = arg};

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

/**
 * @brief Lelijker kunnen we het niet maken
 *
 * Leuker wel
 */
void app_main(void) {
    // Start the GPIO interrupt service
    gpio_install_isr_service(0);

    // Initialize the Non Volatile Storage service
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);

    // Initialize the Board Support Package
    ESP_ERROR_CHECK(bsp_device_initialize());
    bsp_led_initialize();

    uint8_t led_data[] = {
        0x47, 0x00, 0xDF, 0x97, 0x5A, 0xEE, 0xD1, 0x4C, 0xE5, 0xCA, 0x68, 0x65, 0x89, 0xEA, 0x14, 0x25, 0xB8, 0x73,
    };
    bsp_led_write(led_data, sizeof(led_data));

    // Get display parameters and rotation
    res = bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format, &display_data_endian);
    ESP_ERROR_CHECK(res);  // Check that the display parameters have been initialized
    bsp_display_rotation_t display_rotation = bsp_display_get_default_rotation();

    // Convert ESP-IDF color format into PAX buffer type
    pax_buf_type_t format = PAX_BUF_24_888RGB;
    switch (display_color_format) {
        case LCD_COLOR_PIXEL_FORMAT_RGB565:
            format = PAX_BUF_16_565RGB;
            break;
        case LCD_COLOR_PIXEL_FORMAT_RGB888:
            format = PAX_BUF_24_888RGB;
            break;
        default:
            break;
    }

    // Convert BSP display rotation format into PAX orientation type
    pax_orientation_t orientation = PAX_O_UPRIGHT;
    switch (display_rotation) {
        case BSP_DISPLAY_ROTATION_90:
            orientation = PAX_O_ROT_CCW;
            break;
        case BSP_DISPLAY_ROTATION_180:
            orientation = PAX_O_ROT_HALF;
            break;
        case BSP_DISPLAY_ROTATION_270:
            orientation = PAX_O_ROT_CW;
            break;
        case BSP_DISPLAY_ROTATION_0:
        default:
            orientation = PAX_O_UPRIGHT;
            break;
    }

        // Initialize graphics stack
#if defined(CONFIG_BSP_TARGET_KAMI)
    format = PAX_BUF_2_PAL;
#endif

    pax_buf_init(&fb, NULL, display_h_res, display_v_res, format);
    pax_buf_reversed(&fb, display_data_endian == LCD_RGB_DATA_ENDIAN_BIG);

#if defined(CONFIG_BSP_TARGET_KAMI)
    fb.palette      = palette;
    fb.palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif

    pax_buf_set_orientation(&fb, orientation);

    ESP_LOGW(TAG, "Hello HID!");

    pax_background(&fb, WHITE);
    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 0, 16, "Hello HID!");
    blit();

    // Power to USB
    bsp_power_set_usb_host_boost_enabled(true);

    BaseType_t        task_created;
    app_event_queue_t evt_queue;
    ESP_LOGI(TAG, "HID Host example");

    /*
     * Create usb_lib_task to:
     * - initialize USB Host library
     * - Handle USB Host events while APP pin in in HIGH state
     */
    task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    assert(task_created == pdTRUE);

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, 1000);

    /*
     * HID host driver configuration
     * - create background task for handling low level event inside the HID driver
     * - provide the device callback to get new HID Device connection event
     */
    const hid_host_driver_config_t hid_host_driver_config = {.create_background_task = true,
                                                             .task_priority          = 5,
                                                             .stack_size             = 4096,
                                                             .core_id                = 0,
                                                             .callback               = hid_host_device_callback,
                                                             .callback_arg           = NULL};

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    // Create queue
    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

    ESP_LOGI(TAG, "Waiting for HID Device to be connected");

    pax_draw_text(&fb, BLACK, pax_font_sky_mono, 16, 48, 16, "Hello HID!");
    blit();

    while (1) {
        // Wait queue
        if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
            if (APP_EVENT == evt_queue.event_group) {
                // User pressed button
                usb_host_lib_info_t lib_info;
                ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
            }

            if (APP_EVENT_HID_HOST == evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle, evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);
            }
        }
    }

    ESP_LOGI(TAG, "HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    xQueueReset(app_event_queue);
    vQueueDelete(app_event_queue);
}
