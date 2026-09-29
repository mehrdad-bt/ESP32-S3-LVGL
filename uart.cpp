#include "uart.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// ==================================================
// UART Configuration
// ==================================================

// !!! شماره GPIO واقعی RX بردت را اینجا قرار بده !!!
#define UART_RX_PIN        44

// اگر TX لازم نداری، می‌توانی -1 بگذاری.
// فعلاً برای UART کامل روی GPIO43 قرار داده شده.
#define UART_TX_PIN        43

#define UART_BAUDRATE      115200

// ==================================================
// UART Buffer Configuration
// ==================================================

#define UART_BUFFER_SIZE        128
#define UART_MAX_BYTES_PER_CALL 32

static char uart_buffer[
    UART_BUFFER_SIZE
];

static uint16_t uart_index =
    0;

// ==================================================
// Parsed Sensor Values
// ==================================================

static float uart_voltage =
    0.0f;

static float uart_current =
    0.0f;

static bool uart_values_ready =
    false;

// ==================================================
// Last Complete Message
// ==================================================

static char uart_message[
    UART_BUFFER_SIZE
];

static bool uart_message_ready =
    false;

// ==================================================
// Serial Initialization
// ==================================================

extern "C"
{

void serial_init(void)
{
    /*
     * Serial:
     * فقط برای Debug و Serial Monitor
     */
    Serial.begin(
        115200
    );

    /*
     * Serial1:
     * UART واقعی دستگاه
     *
     * RX = GPIO44
     * TX = GPIO43
     */
    Serial1.begin(
        UART_BAUDRATE,
        SERIAL_8N1,
        UART_RX_PIN,
        UART_TX_PIN
    );

    Serial.println(
        "[UART] Serial1 initialized"
    );

    Serial.printf(
        "[UART] RX GPIO=%d TX GPIO=%d BAUD=%d\n",
        UART_RX_PIN,
        UART_TX_PIN,
        UART_BAUDRATE
    );
}

// ==================================================
// Receive and Parse UART Data
// ==================================================

void uart_receive(void)
{
    uint16_t processed_bytes =
        0;

    /*
     * در هر بار اجرا حداکثر
     * UART_MAX_BYTES_PER_CALL
     * بایت پردازش می‌کنیم.
     */

    while (
        Serial1.available() &&
        processed_bytes < UART_MAX_BYTES_PER_CALL
    )
    {
        char c =
            (char)Serial1.read();

        processed_bytes++;

        // --------------------------------------------------
        // Debug raw byte
        // --------------------------------------------------

        Serial.printf(
            "[UART_RX] 0x%02X '%c'\n",
            (unsigned char)c,
            (
                c >= 32 &&
                c <= 126
            )
            ? c
            : '.'
        );

        // --------------------------------------------------
        // End of message
        // --------------------------------------------------

        if (
            c == '\n' ||
            c == '\r'
        )
        {
            if (
                uart_index >
                0
            )
            {
                uart_buffer[
                    uart_index
                ] = '\0';

                Serial.printf(
                    "[UART] MESSAGE: %s\n",
                    uart_buffer
                );

                // --------------------------------------------------
                // Parse:
                // Voltage,Current
                //
                // Example:
                // 23.75,0.82
                // --------------------------------------------------

                float voltage =
                    0.0f;

                float current =
                    0.0f;

                int result =
                    sscanf(
                        uart_buffer,
                        "%f,%f",
                        &voltage,
                        &current
                    );

                if (
                    result == 2
                )
                {
                    uart_voltage =
                        voltage;

                    uart_current =
                        current;

                    uart_values_ready =
                        true;

                    Serial.printf(
                        "[UART] PARSED V=%.2f C=%.2f\n",
                        uart_voltage,
                        uart_current
                    );
                }
                else
                {
                    Serial.println(
                        "[UART] PARSE ERROR"
                    );
                }

                // --------------------------------------------------
                // Store original message
                // --------------------------------------------------

                if (
                    !uart_message_ready
                )
                {
                    strncpy(
                        uart_message,
                        uart_buffer,
                        UART_BUFFER_SIZE
                    );

                    uart_message[
                        UART_BUFFER_SIZE - 1
                    ] = '\0';

                    uart_message_ready =
                        true;
                }

                // --------------------------------------------------
                // Reset receive buffer
                // --------------------------------------------------

                uart_index =
                    0;
            }

            /*
             * اگر چند newline پشت سر هم وجود داشته باشد،
             * پیام خالی نادیده گرفته می‌شود.
             */
        }

        // --------------------------------------------------
        // Store normal character
        // --------------------------------------------------

        else
        {
            if (
                uart_index <
                UART_BUFFER_SIZE - 1
            )
            {
                uart_buffer[
                    uart_index++
                ] = c;
            }
            else
            {
                Serial.println(
                    "[UART] BUFFER OVERFLOW"
                );

                /*
                 * تا newline بعدی
                 * پیام فعلی را دور می‌ریزیم.
                 */
                uart_index =
                    0;
            }
        }
    }
}

// ==================================================
// Get Last Complete Message
// ==================================================

bool uart_get_message(
    char *buffer,
    uint16_t size
)
{
    if (
        !uart_message_ready
    )
    {
        return false;
    }

    if (
        buffer == NULL ||
        size == 0
    )
    {
        return false;
    }

    strncpy(
        buffer,
        uart_message,
        size
    );

    buffer[
        size - 1
    ] = '\0';

    uart_message_ready =
        false;

    return true;
}

// ==================================================
// Get Parsed Voltage and Current
// ==================================================

bool uart_get_values(
    float *voltage,
    float *current
)
{
    if (
        !uart_values_ready
    )
    {
        return false;
    }

    if (
        voltage == NULL ||
        current == NULL
    )
    {
        return false;
    }

    *voltage =
        uart_voltage;

    *current =
        uart_current;

    uart_values_ready =
        false;

    return true;
}

}