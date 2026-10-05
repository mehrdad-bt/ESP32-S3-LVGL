#include "USBUpdate.h"

#include <Arduino.h>
#include <Update.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ui/screens.h"
#include "font_persian_16.h"


// ==================================================
// UPDATE PROTOCOL
// ==================================================
//
// Header:
//
//     4 bytes   = "UPD1"
//     4 bytes   = firmware size, little endian
//     32 bytes  = MD5 hexadecimal text
//
// Total:
//
//     40 bytes
//
// Communication:
//
//     Serial0 -> UART0 -> CH343 -> COM16
//
// ==================================================

static const uint8_t UPDATE_MAGIC[4] =
{
    'U',
    'P',
    'D',
    '1'
};


static constexpr size_t UPDATE_MD5_TEXT_SIZE =
    32;


static constexpr size_t UPDATE_HEADER_SIZE =
    4 +
    4 +
    UPDATE_MD5_TEXT_SIZE;


// ==================================================
// PARAMETERS
// ==================================================

static constexpr size_t UPDATE_BUFFER_SIZE =
    1024;


static constexpr uint32_t UPDATE_TIMEOUT_MS =
    10000;


static constexpr uint32_t READY_INTERVAL_MS =
    500;


static constexpr uint32_t UPDATE_REBOOT_DELAY_MS =
    1500;


// ==================================================
// TASK
// ==================================================

static TaskHandle_t update_task_handle =
    NULL;


// ==================================================
// STATE
// ==================================================

static volatile bool update_requested =
    false;

static volatile bool cancel_requested =
    false;


static volatile UsbUpdateStatus update_status =
    USB_UPDATE_STATUS_IDLE;


static volatile UsbUpdateError update_error =
    USB_UPDATE_ERROR_NONE;


static volatile uint32_t update_total_size =
    0;


static volatile uint32_t update_received_size =
    0;


// ==================================================
// INTERNAL FUNCTIONS
// ==================================================

static void usb_update_task(
    void *parameter
);


static bool read_byte_with_timeout(
    uint8_t *value,
    uint32_t timeout_ms
);


static bool read_exact(
    uint8_t *buffer,
    size_t length,
    uint32_t timeout_ms
);


static bool wait_for_header(
    uint32_t *firmware_size,
    char *md5
);


static void set_error(
    UsbUpdateError error
);


static void send_usb_message(
    const char *message
);


static bool update_receive_firmware(
    uint32_t firmware_size,
    const char *md5
);


static void configure_update_status_text(void);


// ==================================================
// INIT
// ==================================================

void usb_update_init(void)
{
    update_requested =
        false;

    cancel_requested =
        false;

    update_status =
        USB_UPDATE_STATUS_IDLE;

    update_error =
        USB_UPDATE_ERROR_NONE;

    update_total_size =
        0;

    update_received_size =
        0;


    Serial0.println(
        "[UPDATE-DEBUG] USB update module initialized"
    );


    Serial0.println(
        "[UPDATE-DEBUG] Communication = UART0 / Serial0"
    );
}


// ==================================================
// START UPDATE
// ==================================================

bool usb_update_start(void)
{
    UsbUpdateStatus current =
        update_status;


    Serial0.println(
        "[UPDATE-DEBUG] usb_update_start()"
    );


    // --------------------------------------------------
    // Already active
    // --------------------------------------------------

    if (
        current ==
            USB_UPDATE_STATUS_WAITING ||

        current ==
            USB_UPDATE_STATUS_RECEIVING ||

        current ==
            USB_UPDATE_STATUS_VERIFYING
    )
    {
        Serial0.println(
            "[UPDATE-DEBUG] Update already active"
        );

        return false;
    }


    // --------------------------------------------------
    // Reset state
    // --------------------------------------------------

    cancel_requested =
        false;

    update_error =
        USB_UPDATE_ERROR_NONE;

    update_total_size =
        0;

    update_received_size =
        0;

    update_status =
        USB_UPDATE_STATUS_WAITING;


    Serial0.println(
        "[UPDATE-DEBUG] State = WAITING"
    );


    // --------------------------------------------------
    // Make sure UART0 is active
    // --------------------------------------------------

    Serial0.begin(
        115200
    );


    delay(
        50
    );


    // --------------------------------------------------
    // Clear stale RX data
    // --------------------------------------------------

    while (
        Serial0.available() >
        0
    )
    {
        uint8_t dummy;

        Serial0.readBytes(
            &dummy,
            1
        );
    }


    // --------------------------------------------------
    // Create task once
    // --------------------------------------------------

    if (
        update_task_handle ==
        NULL
    )
    {
        BaseType_t result =
            xTaskCreate(
                usb_update_task,
                "USB_Update",
                6144,
                NULL,
                2,
                &update_task_handle
            );


        if (
            result !=
            pdPASS
        )
        {
            Serial0.println(
                "[UPDATE-DEBUG] xTaskCreate FAILED"
            );


            update_status =
                USB_UPDATE_STATUS_ERROR;


            update_error =
                USB_UPDATE_ERROR_BEGIN_FAILED;


            return false;
        }


        Serial0.println(
            "[UPDATE-DEBUG] USB update task created"
        );
    }


    // --------------------------------------------------
    // Request update
    // --------------------------------------------------

    update_requested =
        true;


    Serial0.println(
        "[UPDATE-DEBUG] update_requested = true"
    );


    return true;
}


// ==================================================
// CANCEL
// ==================================================

void usb_update_cancel(void)
{
    if (
        !usb_update_is_active()
    )
    {
        return;
    }


    Serial0.println(
        "[UPDATE-DEBUG] Cancel requested"
    );


    cancel_requested =
        true;
}


// ==================================================
// STATUS
// ==================================================

UsbUpdateStatus usb_update_get_status(void)
{
    return update_status;
}


// ==================================================
// ERROR
// ==================================================

UsbUpdateError usb_update_get_error(void)
{
    return update_error;
}


// ==================================================
// TOTAL SIZE
// ==================================================

uint32_t usb_update_get_total_size(void)
{
    return update_total_size;
}


// ==================================================
// RECEIVED SIZE
// ==================================================

uint32_t usb_update_get_received_size(void)
{
    return update_received_size;
}


// ==================================================
// ACTIVE
// ==================================================

bool usb_update_is_active(void)
{
    UsbUpdateStatus status =
        update_status;


    return (
        status ==
            USB_UPDATE_STATUS_WAITING ||

        status ==
            USB_UPDATE_STATUS_RECEIVING ||

        status ==
            USB_UPDATE_STATUS_VERIFYING
    );
}


// ==================================================
// SET ERROR
// ==================================================

static void set_error(
    UsbUpdateError error
)
{
    update_error =
        error;

    update_status =
        USB_UPDATE_STATUS_ERROR;
}


// ==================================================
// SEND MESSAGE
// ==================================================

static void send_usb_message(
    const char *message
)
{
    Serial0.println(
        message
    );

    Serial0.flush();
}


// ==================================================
// READ BYTE
// ==================================================

static bool read_byte_with_timeout(
    uint8_t *value,
    uint32_t timeout_ms
)
{
    uint32_t start =
        millis();


    while (
        millis() -
            start <
        timeout_ms
    )
    {
        if (
            cancel_requested
        )
        {
            return false;
        }


        if (
            Serial0.available() >
            0
        )
        {
            int data =
                Serial0.read();


            if (
                data >=
                0
            )
            {
                *value =
                    (uint8_t)data;

                return true;
            }
        }


        vTaskDelay(
            pdMS_TO_TICKS(1)
        );
    }


    return false;
}


// ==================================================
// READ EXACT
// ==================================================

static bool read_exact(
    uint8_t *buffer,
    size_t length,
    uint32_t timeout_ms
)
{
    size_t received =
        0;


    uint32_t last_data_time =
        millis();


    while (
        received <
        length
    )
    {
        if (
            cancel_requested
        )
        {
            return false;
        }


        int available =
            Serial0.available();


        if (
            available >
            0
        )
        {
            size_t wanted =
                length -
                received;


            if (
                wanted >
                UPDATE_BUFFER_SIZE
            )
            {
                wanted =
                    UPDATE_BUFFER_SIZE;
            }


            size_t to_read =
                (size_t)available;


            if (
                to_read >
                wanted
            )
            {
                to_read =
                    wanted;
            }


            int read_count =
                Serial0.read(
                    buffer +
                        received,
                    to_read
                );


            if (
                read_count >
                0
            )
            {
                received +=
                    (size_t)read_count;


                last_data_time =
                    millis();
            }
        }
        else
        {
            if (
                millis() -
                    last_data_time >
                timeout_ms
            )
            {
                return false;
            }


            vTaskDelay(
                pdMS_TO_TICKS(1)
            );
        }
    }


    return true;
}


// ==================================================
// WAIT FOR HEADER
// ==================================================

static bool wait_for_header(
    uint32_t *firmware_size,
    char *md5
)
{
    uint8_t magic_index =
        0;


    uint8_t byte =
        0;


    uint32_t start_time =
        millis();


    uint32_t last_ready_time =
        0;


    Serial0.println(
        "[UPDATE-DEBUG] Waiting for firmware header..."
    );


    while (
        millis() -
            start_time <
        UPDATE_TIMEOUT_MS
    )
    {
        uint32_t now =
            millis();


        // --------------------------------------------------
        // Periodic READY message
        // --------------------------------------------------

        if (
            now -
                last_ready_time >=
            READY_INTERVAL_MS
        )
        {
            send_usb_message(
                "__USB_UPDATE_READY__"
            );


            last_ready_time =
                now;
        }


        // --------------------------------------------------
        // Cancel
        // --------------------------------------------------

        if (
            cancel_requested
        )
        {
            return false;
        }


        // --------------------------------------------------
        // Read byte
        // --------------------------------------------------

        if (
            !read_byte_with_timeout(
                &byte,
                100
            )
        )
        {
            continue;
        }


        // --------------------------------------------------
        // Search magic
        // --------------------------------------------------

        if (
            byte ==
            UPDATE_MAGIC[
                magic_index
            ]
        )
        {
            magic_index++;


            if (
                magic_index ==
                sizeof(
                    UPDATE_MAGIC
                )
            )
            {
                Serial0.println(
                    "[UPDATE-DEBUG] Header magic found"
                );


                break;
            }
        }
        else
        {
            if (
                byte ==
                UPDATE_MAGIC[0]
            )
            {
                magic_index =
                    1;
            }
            else
            {
                magic_index =
                    0;
            }
        }
    }


    // --------------------------------------------------
    // Header timeout
    // --------------------------------------------------

    if (
        magic_index !=
        sizeof(
            UPDATE_MAGIC
        )
    )
    {
        set_error(
            USB_UPDATE_ERROR_BAD_HEADER
        );


        send_usb_message(
            "__USB_UPDATE_FAIL__:BAD_HEADER"
        );


        Serial0.println(
            "[UPDATE-DEBUG] Header timeout"
        );


        return false;
    }


    // --------------------------------------------------
    // Firmware size
    // --------------------------------------------------

    uint8_t size_bytes[4];


    if (
        !read_exact(
            size_bytes,
            sizeof(
                size_bytes
            ),
            UPDATE_TIMEOUT_MS
        )
    )
    {
        set_error(
            USB_UPDATE_ERROR_TIMEOUT
        );


        send_usb_message(
            "__USB_UPDATE_FAIL__:SIZE_TIMEOUT"
        );


        return false;
    }


    *firmware_size =
        (
            ((uint32_t)size_bytes[0]) |
            ((uint32_t)size_bytes[1] << 8) |
            ((uint32_t)size_bytes[2] << 16) |
            ((uint32_t)size_bytes[3] << 24)
        );


    // --------------------------------------------------
    // Validate size
    // --------------------------------------------------

    if (
        *firmware_size ==
        0
    )
    {
        set_error(
            USB_UPDATE_ERROR_INVALID_SIZE
        );


        send_usb_message(
            "__USB_UPDATE_FAIL__:INVALID_SIZE"
        );


        return false;
    }


    Serial0.printf(
        "[UPDATE-DEBUG] Firmware size = %lu\n",
        (unsigned long)*firmware_size
    );


    // --------------------------------------------------
    // MD5
    // --------------------------------------------------

    if (
        !read_exact(
            reinterpret_cast<uint8_t *>(md5),
            UPDATE_MD5_TEXT_SIZE,
            UPDATE_TIMEOUT_MS
        )
    )
    {
        set_error(
            USB_UPDATE_ERROR_TIMEOUT
        );


        send_usb_message(
            "__USB_UPDATE_FAIL__:MD5_TIMEOUT"
        );


        return false;
    }


    md5[
        UPDATE_MD5_TEXT_SIZE
    ] =
        '\0';


    Serial0.printf(
        "[UPDATE-DEBUG] MD5 = %s\n",
        md5
    );


    return true;
}


// ==================================================
// RECEIVE FIRMWARE
// ==================================================

static bool update_receive_firmware(
    uint32_t firmware_size,
    const char *md5
)
{
    Serial0.println(
        "[UPDATE-DEBUG] Starting OTA"
    );


    // --------------------------------------------------
    // Start OTA
    // --------------------------------------------------

    if (
        !Update.begin(
            firmware_size,
            U_FLASH
        )
    )
    {
        uint8_t update_error_code =
            Update.getError();


        Serial0.printf(
            "[UPDATE-DEBUG] Update.begin failed: %u\n",
            update_error_code
        );


        if (
            update_error_code ==
            UPDATE_ERROR_NO_PARTITION
        )
        {
            set_error(
                USB_UPDATE_ERROR_NO_OTA
            );


            send_usb_message(
                "__USB_UPDATE_FAIL__:NO_OTA_PARTITION"
            );
        }
        else
        {
            set_error(
                USB_UPDATE_ERROR_BEGIN_FAILED
            );


            send_usb_message(
                "__USB_UPDATE_FAIL__:UPDATE_BEGIN"
            );
        }


        return false;
    }


    Serial0.println(
        "[UPDATE-DEBUG] OTA partition selected"
    );


    // --------------------------------------------------
    // MD5 verification
    // --------------------------------------------------

    if (
        !Update.setMD5(
            md5
        )
    )
    {
        Update.abort();


        set_error(
            USB_UPDATE_ERROR_MD5_FAILED
        );


        send_usb_message(
            "__USB_UPDATE_FAIL__:MD5_SETUP"
        );


        return false;
    }


    Serial0.println(
        "[UPDATE-DEBUG] MD5 verification enabled"
    );


    // --------------------------------------------------
    // Receiving
    // --------------------------------------------------

    update_status =
        USB_UPDATE_STATUS_RECEIVING;


    update_received_size =
        0;


    uint8_t buffer[
        UPDATE_BUFFER_SIZE
    ];


    uint32_t last_data_time =
        millis();


    Serial0.println(
        "[UPDATE-DEBUG] Receiving firmware..."
    );


    // --------------------------------------------------
    // Receive loop
    // --------------------------------------------------

    while (
        update_received_size <
        firmware_size
    )
    {
        // --------------------------------------------------
        // Cancel
        // --------------------------------------------------

        if (
            cancel_requested
        )
        {
            Update.abort();


            update_error =
                USB_UPDATE_ERROR_CANCELED;


            update_status =
                USB_UPDATE_STATUS_CANCELED;


            send_usb_message(
                "__USB_UPDATE_CANCELED__"
            );


            return false;
        }


        // --------------------------------------------------
        // Remaining
        // --------------------------------------------------

        uint32_t remaining =
            firmware_size -
            update_received_size;


        size_t wanted =
            remaining;


        if (
            wanted >
            UPDATE_BUFFER_SIZE
        )
        {
            wanted =
                UPDATE_BUFFER_SIZE;
        }


        // --------------------------------------------------
        // Available UART data
        // --------------------------------------------------

        int available =
            Serial0.available();


        if (
            available <=
            0
        )
        {
            if (
                millis() -
                    last_data_time >
                UPDATE_TIMEOUT_MS
            )
            {
                Update.abort();


                set_error(
                    USB_UPDATE_ERROR_TIMEOUT
                );


                send_usb_message(
                    "__USB_UPDATE_FAIL__:DATA_TIMEOUT"
                );


                return false;
            }


            vTaskDelay(
                pdMS_TO_TICKS(1)
            );


            continue;
        }


        // --------------------------------------------------
        // Read data
        // --------------------------------------------------

        size_t to_read =
            (size_t)available;


        if (
            to_read >
            wanted
        )
        {
            to_read =
                wanted;
        }


        int read_count =
            Serial0.read(
                buffer,
                to_read
            );


        if (
            read_count <=
            0
        )
        {
            continue;
        }


        last_data_time =
            millis();


        // --------------------------------------------------
        // Write Flash
        // --------------------------------------------------

        size_t written =
            Update.write(
                buffer,
                (size_t)read_count
            );


        if (
            written !=
            (size_t)read_count
        )
        {
            Update.abort();


            set_error(
                USB_UPDATE_ERROR_WRITE_FAILED
            );


            send_usb_message(
                "__USB_UPDATE_FAIL__:FLASH_WRITE"
            );


            return false;
        }


        update_received_size +=
            (uint32_t)written;


        taskYIELD();
    }


    // --------------------------------------------------
    // Verify
    // --------------------------------------------------

    update_status =
        USB_UPDATE_STATUS_VERIFYING;


    Serial0.println(
        "[UPDATE-DEBUG] Firmware received"
    );


    Serial0.println(
        "[UPDATE-DEBUG] Verifying..."
    );


    if (
        !Update.end()
    )
    {
        uint8_t error_code =
            Update.getError();


        Update.abort();


        Serial0.printf(
            "[UPDATE-DEBUG] Update.end failed: %u\n",
            error_code
        );


        if (
            error_code ==
            UPDATE_ERROR_MD5
        )
        {
            set_error(
                USB_UPDATE_ERROR_MD5_FAILED
            );


            send_usb_message(
                "__USB_UPDATE_FAIL__:MD5"
            );
        }
        else
        {
            set_error(
                USB_UPDATE_ERROR_VERIFY_FAILED
            );


            send_usb_message(
                "__USB_UPDATE_FAIL__:VERIFY"
            );
        }


        return false;
    }


    // --------------------------------------------------
    // Success
    // --------------------------------------------------

    update_status =
        USB_UPDATE_STATUS_SUCCESS;


    update_error =
        USB_UPDATE_ERROR_NONE;


    Serial0.println(
        "[UPDATE-DEBUG] UPDATE SUCCESS"
    );


    send_usb_message(
        "__USB_UPDATE_OK__"
    );


    delay(
        UPDATE_REBOOT_DELAY_MS
    );


    ESP.restart();


    return true;
}


// ==================================================
// UPDATE TASK
// ==================================================

static void usb_update_task(
    void *parameter
)
{
    (void)parameter;


    Serial0.println(
        "[UPDATE-DEBUG] USB update task started"
    );


    while (
        true
    )
    {
        // --------------------------------------------------
        // Wait for request
        // --------------------------------------------------

        if (
            !update_requested
        )
        {
            vTaskDelay(
                pdMS_TO_TICKS(20)
            );


            continue;
        }


        // --------------------------------------------------
        // Accept request
        // --------------------------------------------------

        update_requested =
            false;


        cancel_requested =
            false;


        Serial0.println(
            "[UPDATE-DEBUG] Update request accepted"
        );


        // --------------------------------------------------
        // Header
        // --------------------------------------------------

        uint32_t firmware_size =
            0;


        char md5[
            UPDATE_MD5_TEXT_SIZE + 1
        ] =
        {
            0
        };


        if (
            !wait_for_header(
                &firmware_size,
                md5
            )
        )
        {
            continue;
        }


        update_total_size =
            firmware_size;


        // --------------------------------------------------
        // Receive firmware
        // --------------------------------------------------

        update_receive_firmware(
            firmware_size,
            md5
        );


        // --------------------------------------------------
        // Error state
        // --------------------------------------------------

        if (
            update_status ==
            USB_UPDATE_STATUS_ERROR
        )
        {
            update_requested =
                false;
        }
    }
}


// ==================================================
// CONFIGURE PERSIAN STATUS LABEL
// ==================================================

static void configure_update_status_text(void)
{
    if (
        objects.update_status_text ==
        NULL
    )
    {
        return;
    }


    lv_obj_set_style_text_font(
        objects.update_status_text,
        &font_persian_16,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_base_dir(
        objects.update_status_text,
        LV_BASE_DIR_RTL,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_text_color(
        objects.update_status_text,
        lv_color_hex(0xffffff),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );
}


// ==================================================
// GUI UPDATE
// ==================================================

void usb_update_gui_update(void)
{
    if (
        objects.update_page ==
        NULL
    )
    {
        return;
    }


    if (
        objects.update_status_text ==
        NULL
    )
    {
        return;
    }


    // --------------------------------------------------
    // Configure Persian font / RTL
    // --------------------------------------------------

    configure_update_status_text();


    UsbUpdateStatus status =
        usb_update_get_status();


    char text[96];


    text[0] =
        '\0';


    // --------------------------------------------------
    // Status text
    // --------------------------------------------------

    switch (
        status
    )
    {
        case USB_UPDATE_STATUS_IDLE:

            snprintf(
                text,
                sizeof(text),
                "لطفاً USB را وصل کنید..."
            );

            break;


        case USB_UPDATE_STATUS_WAITING:

            snprintf(
                text,
                sizeof(text),
                "منتظر فایل Firmware..."
            );

            break;


        case USB_UPDATE_STATUS_RECEIVING:
        {
            uint32_t total =
                usb_update_get_total_size();


            uint32_t received =
                usb_update_get_received_size();


            uint32_t percent =
                0;


            if (
                total >
                0
            )
            {
                percent =
                    (
                        received *
                        100UL
                    ) /
                    total;
            }


            if (
                percent >
                100
            )
            {
                percent =
                    100;
            }


            snprintf(
                text,
                sizeof(text),
                "در حال دریافت Firmware... %lu%%",
                (unsigned long)percent
            );


            break;
        }


        case USB_UPDATE_STATUS_VERIFYING:

            snprintf(
                text,
                sizeof(text),
                "در حال بررسی Firmware..."
            );

            break;


        case USB_UPDATE_STATUS_SUCCESS:

            snprintf(
                text,
                sizeof(text),
                "بروزرسانی موفق بود..."
            );

            break;


        case USB_UPDATE_STATUS_CANCELED:

            snprintf(
                text,
                sizeof(text),
                "بروزرسانی لغو شد."
            );

            break;


        case USB_UPDATE_STATUS_ERROR:

            switch (
                usb_update_get_error()
            )
            {
                case USB_UPDATE_ERROR_BAD_HEADER:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا: فایل Firmware معتبر نیست."
                    );

                    break;


                case USB_UPDATE_ERROR_INVALID_SIZE:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا: حجم Firmware نامعتبر است."
                    );

                    break;


                case USB_UPDATE_ERROR_NO_OTA:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا: پارتیشن OTA وجود ندارد."
                    );

                    break;


                case USB_UPDATE_ERROR_MD5_FAILED:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا: اعتبار Firmware تأیید نشد."
                    );

                    break;


                case USB_UPDATE_ERROR_WRITE_FAILED:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا: نوشتن Flash ناموفق بود."
                    );

                    break;


                case USB_UPDATE_ERROR_TIMEOUT:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا: زمان دریافت تمام شد."
                    );

                    break;


                case USB_UPDATE_ERROR_VERIFY_FAILED:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا: Firmware تأیید نشد."
                    );

                    break;


                case USB_UPDATE_ERROR_CANCELED:

                    snprintf(
                        text,
                        sizeof(text),
                        "بروزرسانی لغو شد."
                    );

                    break;


                case USB_UPDATE_ERROR_BEGIN_FAILED:

                default:

                    snprintf(
                        text,
                        sizeof(text),
                        "خطا در شروع بروزرسانی."
                    );

                    break;
            }

            break;
    }


    // --------------------------------------------------
    // Label
    // --------------------------------------------------

    lv_label_set_text(
        objects.update_status_text,
        text
    );


    // --------------------------------------------------
    // Update button
    // --------------------------------------------------

    bool active =
        (
            status ==
                USB_UPDATE_STATUS_WAITING ||

            status ==
                USB_UPDATE_STATUS_RECEIVING ||

            status ==
                USB_UPDATE_STATUS_VERIFYING
        );


    if (
        objects.update_button !=
        NULL
    )
    {
        if (
            active
        )
        {
            lv_obj_add_state(
                objects.update_button,
                LV_STATE_DISABLED
            );
        }
        else
        {
            lv_obj_clear_state(
                objects.update_button,
                LV_STATE_DISABLED
            );
        }
    }


    // --------------------------------------------------
    // Exit button
    // --------------------------------------------------

    if (
        objects.update_exit_button !=
        NULL
    )
    {
        if (
            status ==
                USB_UPDATE_STATUS_RECEIVING ||

            status ==
                USB_UPDATE_STATUS_VERIFYING
        )
        {
            lv_obj_add_state(
                objects.update_exit_button,
                LV_STATE_DISABLED
            );
        }
        else
        {
            lv_obj_clear_state(
                objects.update_exit_button,
                LV_STATE_DISABLED
            );
        }
    }
}