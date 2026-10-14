#include <Arduino.h>
#include <Update.h>
#include <stdio.h>
#include <string.h>

#include "ota_uart.h"

// ==================================================
// Configuration
// ==================================================

#define OTA_WAIT_BEGIN_MS       20000UL   // ESP8266 computes CRC32 of the file first
#define OTA_RX_TIMEOUT_MS       5000UL    // no byte at all during transfer
#define OTA_INTERBYTE_MS        100UL     // resync inside a half-received frame
#define OTA_REBOOT_DELAY_MS     2000UL
#define OTA_MAX_CHUNK           1024U
#define OTA_MAX_BYTES_PER_CALL  2048U

#define REPLY_ACK               0x06
#define REPLY_NAK               0x15
#define REPLY_CAN               0x18

// Leading '\n' flushes any junk the sender may have in its line buffer.
#define OTA_REQUEST_TEXT        "\nOTA_REQUEST\n"

// ==================================================
// State
// ==================================================

static OtaState g_state = OTA_STATE_IDLE;
static OtaError g_err   = OTA_ERR_NONE;

static bool     g_update_open  = false;

static uint32_t g_total_size   = 0;
static uint32_t g_expected_crc = 0;
static uint16_t g_chunk_size   = 0;
static uint32_t g_received     = 0;
static uint16_t g_expected_seq = 0;
static uint32_t g_crc_run      = 0xFFFFFFFFUL;
static uint8_t  g_percent      = 0;

static uint32_t g_state_ms     = 0;
static uint32_t g_last_rx_ms   = 0;

// line parser (handshake)
static char     g_line[64];
static uint8_t  g_line_len = 0;

// frame parser
enum FrameState { FS_SYNC1 = 0, FS_SYNC2, FS_BODY };

static FrameState g_fs = FS_SYNC1;
static uint8_t    g_frame[4 + OTA_MAX_CHUNK + 2];
static uint16_t   g_fidx  = 0;
static uint16_t   g_fneed = 0;

// ==================================================
// CRC
// ==================================================

static uint32_t crc32_update(uint32_t crc, const uint8_t *d, size_t n)
{
    while (n--)
    {
        crc ^= *d++;
        for (int i = 0; i < 8; i++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return crc;
}

static uint16_t crc16_ccitt(uint16_t crc, const uint8_t *d, size_t n)
{
    while (n--)
    {
        crc ^= (uint16_t)(*d++) << 8;
        for (int i = 0; i < 8; i++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

// ==================================================
// Helpers
// ==================================================

static void send_reply(uint8_t type, uint16_t seq)
{
    uint8_t r[3] =
    {
        type,
        (uint8_t)(seq & 0xFF),
        (uint8_t)(seq >> 8)
    };

    Serial1.write(r, sizeof(r));
}

static void ota_fail(OtaError err)
{
    if (g_update_open)
    {
        Update.abort();
        g_update_open = false;
    }

    send_reply(REPLY_CAN, 0);

    g_err   = err;
    g_state = OTA_STATE_FAILED;
    g_fs    = FS_SYNC1;

    Serial.printf("[OTA] FAILED err=%d\n", (int)err);
}

static void rx_flush(void)
{
    while (Serial1.available())
    {
        Serial1.read();
    }
}

// ==================================================
// Frame handling
// ==================================================

static void finish_transfer(uint16_t seq)
{
    g_state = OTA_STATE_VERIFYING;

    if (g_received != g_total_size)
    {
        ota_fail(OTA_ERR_SIZE);
        return;
    }

    if ((g_crc_run ^ 0xFFFFFFFFUL) != g_expected_crc)
    {
        Serial.printf("[OTA] CRC mismatch got=%08lX exp=%08lX\n",
                      (unsigned long)(g_crc_run ^ 0xFFFFFFFFUL),
                      (unsigned long)g_expected_crc);
        ota_fail(OTA_ERR_CRC);
        return;
    }

    if (!Update.end(true))
    {
        Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
        g_update_open = false;
        ota_fail(OTA_ERR_FINISH);
        return;
    }

    g_update_open = false;

    send_reply(REPLY_ACK, seq);
    Serial1.flush();

    g_percent  = 100;
    g_state    = OTA_STATE_SUCCESS;
    g_state_ms = millis();

    Serial.println("[OTA] SUCCESS - rebooting soon");
}

static void handle_frame(void)
{
    uint16_t seq = (uint16_t)g_frame[0] | ((uint16_t)g_frame[1] << 8);
    uint16_t len = (uint16_t)g_frame[2] | ((uint16_t)g_frame[3] << 8);

    uint16_t rx_crc = (uint16_t)g_frame[4 + len] |
                      ((uint16_t)g_frame[5 + len] << 8);

    uint16_t calc = crc16_ccitt(0xFFFF, g_frame, 4U + len);

    if (rx_crc != calc)
    {
        send_reply(REPLY_NAK, g_expected_seq);
        return;
    }

    if (seq != g_expected_seq)
    {
        if ((uint16_t)(seq + 1) == g_expected_seq)
        {
            send_reply(REPLY_ACK, seq);          // duplicate (our ACK was lost)
        }
        else
        {
            send_reply(REPLY_NAK, g_expected_seq);
        }
        return;
    }

    // END frame
    if (len == 0)
    {
        finish_transfer(seq);
        return;
    }

    // DATA frame
    size_t written = Update.write(&g_frame[4], len);

    if (written != len)
    {
        Serial.printf("[OTA] write failed: %s\n", Update.errorString());
        ota_fail(OTA_ERR_WRITE);
        return;
    }

    g_crc_run   = crc32_update(g_crc_run, &g_frame[4], len);
    g_received += len;
    g_expected_seq++;

    if (g_total_size > 0)
    {
        g_percent = (uint8_t)(((uint64_t)g_received * 100ULL) / g_total_size);
    }

    send_reply(REPLY_ACK, seq);

    if ((seq & 0x3F) == 0)
    {
        Serial.printf("[OTA] %u%% (%lu/%lu)\n",
                      (unsigned)g_percent,
                      (unsigned long)g_received,
                      (unsigned long)g_total_size);
    }
}

static void feed_frame(uint8_t b)
{
    switch (g_fs)
    {
        case FS_SYNC1:

            if (b == 0xAA)
            {
                g_fs = FS_SYNC2;
            }
            break;

        case FS_SYNC2:

            if (b == 0x55)
            {
                g_fs    = FS_BODY;
                g_fidx  = 0;
                g_fneed = 0xFFFF;
            }
            else if (b != 0xAA)
            {
                g_fs = FS_SYNC1;
            }
            break;

        case FS_BODY:

            g_frame[g_fidx++] = b;

            if (g_fidx == 4)
            {
                uint16_t len = (uint16_t)g_frame[2] | ((uint16_t)g_frame[3] << 8);

                if (len > g_chunk_size)
                {
                    send_reply(REPLY_NAK, g_expected_seq);
                    g_fs = FS_SYNC1;
                    break;
                }

                g_fneed = (uint16_t)(4U + len + 2U);
            }

            if (g_fidx == g_fneed)
            {
                g_fs = FS_SYNC1;
                handle_frame();
            }
            break;
    }
}

// ==================================================
// Handshake line handling
// ==================================================

static void handle_line(void)
{
    if (strncmp(g_line, "OTA_BEGIN,", 10) != 0)
    {
        return;     // normal "V,I" lines etc. are ignored
    }

    unsigned long size  = 0;
    unsigned long crc   = 0;
    unsigned int  chunk = 0;

    if (sscanf(g_line, "OTA_BEGIN,%lu,%lx,%u", &size, &crc, &chunk) != 3 ||
        size == 0 || chunk == 0 || chunk > OTA_MAX_CHUNK)
    {
        Serial1.print("OTA_ERR\n");
        ota_fail(OTA_ERR_BAD_BEGIN);
        return;
    }

    if (!Update.begin((size_t)size))
    {
        Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
        Serial1.print("OTA_ERR\n");
        ota_fail(OTA_ERR_NO_SPACE);
        return;
    }

    g_update_open  = true;
    g_total_size   = (uint32_t)size;
    g_expected_crc = (uint32_t)crc;
    g_chunk_size   = (uint16_t)chunk;
    g_received     = 0;
    g_expected_seq = 0;
    g_crc_run      = 0xFFFFFFFFUL;
    g_percent      = 0;
    g_fs           = FS_SYNC1;
    g_last_rx_ms   = millis();

    Serial.printf("[OTA] BEGIN size=%lu crc=%08lX chunk=%u\n",
                  size, crc, chunk);

    Serial1.print("OTA_READY\n");

    g_state = OTA_STATE_RECEIVING;
}

static void feed_line(uint8_t b)
{
    if (b == '\n' || b == '\r')
    {
        if (g_line_len > 0)
        {
            g_line[g_line_len] = '\0';
            g_line_len = 0;
            handle_line();
        }
        return;
    }

    if (g_line_len < sizeof(g_line) - 1)
    {
        g_line[g_line_len++] = (char)b;
    }
    else
    {
        g_line_len = 0;
    }
}

// ==================================================
// RX pump
// ==================================================

static void process_rx(void)
{
    uint16_t n = 0;

    while (Serial1.available() && n < OTA_MAX_BYTES_PER_CALL)
    {
        uint8_t b = (uint8_t)Serial1.read();
        n++;

        g_last_rx_ms = millis();

        if (g_state == OTA_STATE_WAIT_BEGIN)
        {
            feed_line(b);
        }
        else if (g_state == OTA_STATE_RECEIVING)
        {
            feed_frame(b);
        }

        if (g_state != OTA_STATE_WAIT_BEGIN &&
            g_state != OTA_STATE_RECEIVING)
        {
            break;
        }
    }
}

// ==================================================
// Public API
// ==================================================

bool ota_uart_request_start(void)
{
    if (ota_uart_is_busy())
    {
        return false;
    }

    rx_flush();

    g_line_len     = 0;
    g_fs           = FS_SYNC1;
    g_err          = OTA_ERR_NONE;
    g_percent      = 0;
    g_received     = 0;
    g_update_open  = false;

    Serial1.print(OTA_REQUEST_TEXT);

    g_state    = OTA_STATE_WAIT_BEGIN;
    g_state_ms = millis();

    Serial.println("[OTA] request sent");

    return true;
}

void ota_uart_task(void)
{
    uint32_t now = millis();

    switch (g_state)
    {
        case OTA_STATE_WAIT_BEGIN:

            process_rx();
            now = millis();     // refresh: process_rx() takes time

            if (g_state == OTA_STATE_WAIT_BEGIN &&
                now - g_state_ms > OTA_WAIT_BEGIN_MS)
            {
                ota_fail(OTA_ERR_NO_RESPONSE);
            }
            break;

        case OTA_STATE_RECEIVING:

            process_rx();
            now = millis();     // refresh: g_last_rx_ms was set inside process_rx()

            if (g_state == OTA_STATE_RECEIVING)
            {
                if (g_fs != FS_SYNC1 &&
                    now - g_last_rx_ms > OTA_INTERBYTE_MS)
                {
                    g_fs = FS_SYNC1;        // drop half-received frame
                }

                if (now - g_last_rx_ms > OTA_RX_TIMEOUT_MS)
                {
                    ota_fail(OTA_ERR_TIMEOUT);
                }
            }
            break;

        case OTA_STATE_SUCCESS:

            if (now - g_state_ms >= OTA_REBOOT_DELAY_MS)
            {
                ESP.restart();
            }
            break;

        default:
            break;
    }
}

bool ota_uart_is_active(void)
{
    return g_state == OTA_STATE_WAIT_BEGIN ||
           g_state == OTA_STATE_RECEIVING  ||
           g_state == OTA_STATE_VERIFYING  ||
           g_state == OTA_STATE_SUCCESS;
}

bool ota_uart_is_busy(void)
{
    return ota_uart_is_active();
}

void ota_uart_clear_result(void)
{
    if (g_state == OTA_STATE_FAILED)
    {
        g_state = OTA_STATE_IDLE;
        g_err   = OTA_ERR_NONE;
    }
}

OtaState ota_uart_get_state(void)   { return g_state; }
OtaError ota_uart_get_error(void)   { return g_err; }
uint8_t  ota_uart_get_percent(void) { return g_percent; }