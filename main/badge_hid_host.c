/*
 * HID host library for gamepad and mouse input devices.
 * Contains low-level helpers for parsing raw USB HID input reports.

 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Badge.Team
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "badge_hid_host.h"
#include <stdio.h>
#include "esp_log.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_mouse.h"
#include "usb/usb_host.h"

static char const     TAG[]           = "USB HID";
static QueueHandle_t  hid_event_queue = NULL;
static QueueHandle_t* bsp_event_queue = NULL;

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

    ESP_LOGI(TAG, "Event");

    if (hid_event_queue) {
        xQueueSend(hid_event_queue, &evt_queue, 0);
    } else {
        ESP_LOGW(TAG, "Event queue not found!");
    }
}

/**
 * @brief HID Protocol string names
 */
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_LOGI(TAG, "HID Device, protocol '%s' INPUT_REPORT",
                    hid_proto_name_str[dev_params.proto]);
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));

        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                // hid_host_keyboard_report_callback(data, data_length);
            } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                // hid_host_mouse_report_callback(data, data_length);
            }
        } else {
            // hid_host_generic_report_callback(data, data_length);
        }

        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
                 hid_proto_name_str[dev_params.proto]);
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
                 hid_proto_name_str[dev_params.proto]);
        break;
    default:
        ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
                 hid_proto_name_str[dev_params.proto]);
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
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
                 hid_proto_name_str[dev_params.proto]);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };

        if (dev_params.proto != HID_PROTOCOL_NONE) {
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                }
            }
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
        }
        break;
    default:
        break;
    }
}
/**
 * @brief Start USB Host install and handle common USB host library events
 *
 * @param[in] arg  Not used
 */
void hid_client_task(void* arg) {
        app_event_queue_t evt_queue;

    xTaskNotifyGive(arg);

    ESP_LOGI(TAG, "HID Driver installed");

    while (true) {
        if (xQueueReceive(hid_event_queue, &evt_queue, portMAX_DELAY)) {
            if (APP_EVENT == evt_queue.event_group) {
                // User pressed button
                usb_host_lib_info_t lib_info;
                ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
                if (lib_info.num_devices == 0) {
                    // End while cycle
                    break;
                } else {
                    ESP_LOGW(TAG, "To shutdown HID, remove all USB devices and try again.");
                    // Keep polling
                }
            }

            if (APP_EVENT_HID_HOST ==  evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle,
                                      evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);
            }
        }
    }

    ESP_LOGI(TAG, "HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    xQueueReset(hid_event_queue);
    vQueueDelete(hid_event_queue);
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
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
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 *
 */
esp_err_t badge_hid_init(QueueHandle_t* event_queue) {
    bsp_event_queue = event_queue;
    BaseType_t task_created;

    /*
     * Create usb_lib_task to:
     * - initialize USB Host library
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

    hid_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

    task_created = xTaskCreatePinnedToCore(hid_client_task, "hid_events", 4096, xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Subsystem initialized");

    return ESP_OK;
}

esp_err_t badge_hid_deint(void) {
    usb_host_lib_info_t lib_info;
    ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
    if (lib_info.num_devices == 0) {
        ESP_LOGW(TAG, "To shutdown driver, remove all USB devices and try again.");
    }
    // ESP_ERROR_CHECK(hid_host_unregister_callbacks());
    ESP_ERROR_CHECK(hid_host_uninstall());
    ESP_ERROR_CHECK(usb_host_uninstall());
    hid_event_queue = NULL;
    return ESP_OK;
}

/**
 * @brief Sign-extends a 12-bit value to a 16-bit signed integer.
 *
 * Many HID mice encode high-resolution X/Y deltas using 12-bit signed values.
 * This function correctly extends them to usable 16-bit signed values.
 *
 * @param value A 12-bit unsigned value (lower 12 bits significant).
 * @return int16_t Signed version of the value.
 */
inline int16_t sign_extend_12bit(uint16_t value) {
    if (value & 0x800) {
        // If the 12th bit is set (negative number in 12-bit signed)
        return (int16_t)(value | 0xF000);  // Fill top 4 bits with 1s
    } else {
        return (int16_t)(value & 0x0FFF);  // Mask to 12 bits
    }
}

/**
 * @brief Parses a mouse input report into a structured format.
 *
 * Supports both boot protocol reports (4 bytes) and extended HID reports.
 *
 * @param data Raw pointer to HID report data.
 * @param length Length of the report in bytes.
 * @return mouse_report_t Parsed report with movement and button states.
 */
mouse_report_t parse_mouse_event(const uint8_t* const data, const int length) {
    mouse_report_t mouse_report = {0};

    if (length <= 4) {
        hid_mouse_input_report_boot_t* boot_mouse_report = (hid_mouse_input_report_boot_t*)data;
        mouse_report.x_displacement                      = boot_mouse_report->x_displacement;
        mouse_report.y_displacement                      = boot_mouse_report->y_displacement;
        mouse_report.buttons.val                         = boot_mouse_report->buttons.val;
        if (length == 3) {
            mouse_report.scroll = data[4];
        }
    } else if (length == 5) {
        // Modern Logitech
        mouse_report.buttons.val    = data[0];
        mouse_report.x_displacement = (int8_t)data[1];
        mouse_report.y_displacement = (int8_t)data[2];
        mouse_report.scroll         = (int8_t)data[3];
        mouse_report.tilt           = (int8_t)data[4];
    } else if (length < 9) {
        mouse_report.buttons.val    = data[1];
        mouse_report.x_displacement = sign_extend_12bit((data[4] & 0x0F) << 8) | data[3];
        mouse_report.y_displacement = sign_extend_12bit(data[5] << 4) | (data[4] >> 4);
        mouse_report.scroll         = (int8_t)data[6];
        if (length == 8) {
            mouse_report.tilt = (int8_t)data[7];
        }
    } else {
        mouse_report.buttons.val    = data[1];
        mouse_report.x_displacement = (int16_t)((data[4] << 8) | data[3]);
        mouse_report.y_displacement = (int16_t)((data[6] << 8) | data[5]);
        mouse_report.scroll         = (int8_t)data[7];
        mouse_report.tilt           = (int8_t)data[8];
    }

    return mouse_report;
}

/**
 * @brief Parses a gamepad HID report into the standard format.
 *
 * This function should be implemented per controller type (e.g., PS4, Xbox).
 * It fills out the gamepad_report_t with button and axis values.
 *
 * @param rpt Pointer to the report struct to populate.
 * @param data Raw HID report data.
 * @param length Report length in bytes.
 * @return true if parsing was successful; false otherwise.
 */
gamepad_report_t parse_gamepad_report(const uint8_t* data, int length) {
    gamepad_report_t rpt = {0};

    if (length < 10) return rpt;

    rpt.report_id = data[0];

    uint8_t hat = data[1];
    uint8_t b1  = data[2];
    uint8_t b2  = data[3];

    rpt.buttons.val = 0;

    rpt.buttons.up    = (hat == 0x00 || hat == 0x01 || hat == 0x07);
    rpt.buttons.right = (hat == 0x01 || hat == 0x02 || hat == 0x03);
    rpt.buttons.down  = (hat == 0x03 || hat == 0x04 || hat == 0x05);
    rpt.buttons.left  = (hat == 0x05 || hat == 0x06 || hat == 0x07);

    // Face buttons
    rpt.buttons.a = (b2 >> 6) & 1;
    rpt.buttons.b = (b2 >> 5) & 1;
    rpt.buttons.x = (b2 >> 4) & 1;
    rpt.buttons.y = (b2 >> 3) & 1;

    // Thumbsticks
    rpt.buttons.l1 = (b2 >> 0) & 1;
    rpt.buttons.r1 = (b1 >> 7) & 1;

    // Shoulders and triggers
    rpt.buttons.l2 = (b2 >> 2) & 1;
    rpt.buttons.r2 = (b2 >> 1) & 1;
    rpt.buttons.l3 = (b1 >> 2) & 1;
    rpt.buttons.r3 = (b1 >> 3) & 1;

    // Extra buttons
    rpt.buttons.l4     = (b1 >> 1) & 1;
    rpt.buttons.r4     = (b1 >> 0) & 1;
    rpt.buttons.select = (b1 >> 6) & 1;
    rpt.buttons.start  = (b1 >> 5) & 1;
    rpt.buttons.home   = (b1 >> 4) & 1;

    rpt.lx = data[4];
    rpt.ly = data[5];
    rpt.rx = data[6];
    rpt.ry = data[7];
    rpt.lt = data[8];
    rpt.rt = data[9];

    return rpt;
}
