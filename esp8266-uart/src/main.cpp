/*
 * ESP8266 -> ESP32-S3
 *   1) Normal mode: sends "Voltage,Current\n" (e.g. "23.75,1.00\n") every 100 ms
 *      (same format the STM32F103 sent, so S3 uart.cpp is unchanged).
 *   2) OTA mode: sends /fw.bin (stored in LittleFS) to the S3 over the SAME UART.
 *      Voltage/current sending is paused while OTA is running.
 *
 * Wiring (UART0 swapped to GPIO13/GPIO15):
 *   ESP8266 GPIO15 (TX2) -> S3 GPIO44 (RX)
 *   ESP8266 GPIO13 (RX2) <- S3 GPIO43 (TX)
 *   GND common.   Debug log: GPIO2 (Serial1, TX only, 115200)
 *   OTA start: the S3 sends "OTA_REQUEST\n" (update button on its screen).
 *
 * ---------------- OTA protocol (ESP8266 = sender, S3 = receiver) -------------
 * 1. ESP8266 -> "OTA_BEGIN,<size>,<crc32 hex 8 digits>,<chunk>\n"   (text)
 *    S3      -> "OTA_READY\n"  (or "OTA_ERR\n")                      (text)
 * 2. For every chunk (binary frame):
 *      0xAA 0x55 | seq u16 LE | len u16 LE | payload[len] | crc16 u16 LE
 *      crc16 = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over seq,len,payload
 *    S3 replies 3 bytes:  [0x06 ACK | 0x15 NAK | 0x18 CANCEL] [seq lo] [seq hi]
 *    S3 must re-ACK a duplicate of the last accepted seq (lost-ACK case).
 * 3. After the last chunk: frame with seq = total_chunks and len = 0 (END).
 *    S3 verifies whole-file CRC32 (zlib CRC32; esp_rom_crc32_le(0,buf,len) chained),
 *    calls Update.end(true), sends ACK, then restarts.
 * Timeouts: ACK 2 s (3 tries per chunk), END 15 s.
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>

// ==================================================
// Configuration
// ==================================================

#define LINK_BAUD             115200UL
#define LOG                   Serial1

#define FW_PATH               "/fw.bin"

#define SEND_PERIOD_MS        100UL

// ADC: NodeMCU / D1 mini boards have an on-board divider: 3.3 V -> 1023.
// Bare ESP-12 module: 1.0 V -> 1023 (adjust your input divider).
#define ADC_MAX_RAW           1023.0f
#define VOLTAGE_FULL_SCALE    24.0f      // ADC full scale = 24 V (as on the STM32)
#define CURRENT_FIXED_VALUE   1.00f      // temporary fixed current (as on the STM32)
#define ADC_AVG_SAMPLES       8

#define OTA_CHUNK             512
#define OTA_ACK_TIMEOUT_MS    2000UL
#define OTA_END_TIMEOUT_MS    15000UL
#define OTA_BEGIN_TIMEOUT_MS  5000UL
#define OTA_RETRIES           3

static const uint8_t SYNC1 = 0xAA;
static const uint8_t SYNC2 = 0x55;
static const uint8_t R_ACK = 0x06;
static const uint8_t R_NAK = 0x15;
static const uint8_t R_CAN = 0x18;

enum Reply { REPLY_TIMEOUT = 0, REPLY_ACK, REPLY_NAK, REPLY_CAN };

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
// Voltage / Current
// ==================================================

static float read_voltage(void)
{
    uint32_t sum = 0;

    for (int i = 0; i < ADC_AVG_SAMPLES; i++)
    {
        sum += analogRead(A0);
    }

    float raw = (float)sum / (float)ADC_AVG_SAMPLES;
    float v   = raw * VOLTAGE_FULL_SCALE / ADC_MAX_RAW;

    if (v < 0.0f)                v = 0.0f;
    if (v > VOLTAGE_FULL_SCALE)  v = VOLTAGE_FULL_SCALE;

    return v;
}

static void send_values(float voltage, float current)
{
    char tx[32];

    int32_t v100 = (int32_t)(voltage * 100.0f + 0.5f);
    int32_t c100 = (int32_t)(current * 100.0f + 0.5f);

    int len = snprintf(tx, sizeof(tx), "%ld.%02ld,%ld.%02ld\n",
                       (long)(v100 / 100), (long)(v100 % 100),
                       (long)(c100 / 100), (long)(c100 % 100));

    if (len > 0)
    {
        Serial.write((const uint8_t *)tx, (size_t)len);
    }
}

// ==================================================
// OTA helpers
// ==================================================

static void rx_flush(void)
{
    while (Serial.available())
    {
        Serial.read();
    }
}

static bool wait_line(const char *expected, uint32_t timeout_ms)
{
    char    line[32];
    uint8_t n  = 0;
    uint32_t t0 = millis();

    while (millis() - t0 < timeout_ms)
    {
        ESP.wdtFeed();

        while (Serial.available())
        {
            char c = (char)Serial.read();

            if (c == '\n' || c == '\r')
            {
                if (n > 0)
                {
                    line[n] = '\0';
                    if (strcmp(line, expected) == 0)
                    {
                        return true;
                    }
                    n = 0;
                }
            }
            else if (n < sizeof(line) - 1)
            {
                line[n++] = c;
            }
        }

        delay(1);
    }

    return false;
}

static Reply wait_reply(uint16_t seq, uint32_t timeout_ms)
{
    uint8_t  buf[3];
    uint8_t  n  = 0;
    uint32_t t0 = millis();

    while (millis() - t0 < timeout_ms)
    {
        ESP.wdtFeed();

        if (!Serial.available())
        {
            delay(1);
            continue;
        }

        uint8_t b = (uint8_t)Serial.read();

        if (n == 0 && b != R_ACK && b != R_NAK && b != R_CAN)
        {
            continue;   // resync
        }

        buf[n++] = b;

        if (n == 3)
        {
            uint16_t s = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);

            if (buf[0] == R_CAN)
            {
                return REPLY_CAN;
            }

            if (s == seq)
            {
                return (buf[0] == R_ACK) ? REPLY_ACK : REPLY_NAK;
            }

            n = 0;      // stale reply for another seq
        }
    }

    return REPLY_TIMEOUT;
}

static bool send_frame(uint16_t seq, const uint8_t *data, uint16_t len,
                       uint32_t ack_timeout_ms)
{
    uint8_t hdr[6] =
    {
        SYNC1, SYNC2,
        (uint8_t)(seq & 0xFF), (uint8_t)(seq >> 8),
        (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)
    };

    uint16_t crc = crc16_ccitt(0xFFFF, hdr + 2, 4);

    if (len > 0)
    {
        crc = crc16_ccitt(crc, data, len);
    }

    for (int attempt = 0; attempt < OTA_RETRIES; attempt++)
    {
        rx_flush();

        Serial.write(hdr, sizeof(hdr));

        if (len > 0)
        {
            Serial.write(data, len);
        }

        Serial.write((uint8_t)(crc & 0xFF));
        Serial.write((uint8_t)(crc >> 8));
        Serial.flush();

        Reply r = wait_reply(seq, ack_timeout_ms);

        if (r == REPLY_ACK)
        {
            return true;
        }

        if (r == REPLY_CAN)
        {
            LOG.println("[OTA] S3 cancelled");
            return false;
        }

        LOG.printf("[OTA] seq=%u retry %d (%s)\n",
                   seq, attempt + 1, r == REPLY_NAK ? "NAK" : "timeout");
    }

    return false;
}

// ==================================================
// OTA main routine
// ==================================================

static bool ota_run(void)
{
    LOG.println("[OTA] start");

    File f = LittleFS.open(FW_PATH, "r");

    if (!f)
    {
        LOG.println("[OTA] " FW_PATH " not found in LittleFS");
        return false;
    }

    uint32_t size = f.size();

    if (size == 0)
    {
        LOG.println("[OTA] empty file");
        f.close();
        return false;
    }

    static uint8_t buf[OTA_CHUNK];

    // ---- whole-file CRC32 ----
    uint32_t crc = 0xFFFFFFFFUL;
    int      n;

    while ((n = f.read(buf, sizeof(buf))) > 0)
    {
        crc = crc32_update(crc, buf, (size_t)n);
        ESP.wdtFeed();
        yield();
    }

    crc ^= 0xFFFFFFFFUL;
    f.seek(0);

    uint16_t total = (uint16_t)((size + OTA_CHUNK - 1) / OTA_CHUNK);

    LOG.printf("[OTA] size=%lu crc32=%08lX chunks=%u\n",
               (unsigned long)size, (unsigned long)crc, total);

    // ---- handshake ----
    bool ready = false;

    for (int i = 0; i < OTA_RETRIES && !ready; i++)
    {
        rx_flush();

        Serial.printf("OTA_BEGIN,%lu,%08lX,%u\n",
                      (unsigned long)size, (unsigned long)crc, (unsigned)OTA_CHUNK);
        Serial.flush();

        ready = wait_line("OTA_READY", OTA_BEGIN_TIMEOUT_MS);
    }

    if (!ready)
    {
        LOG.println("[OTA] S3 did not answer OTA_READY");
        f.close();
        return false;
    }

    // ---- data ----
    for (uint16_t seq = 0; seq < total; seq++)
    {
        n = f.read(buf, sizeof(buf));

        if (n <= 0)
        {
            LOG.println("[OTA] file read error");
            f.close();
            return false;
        }

        if (!send_frame(seq, buf, (uint16_t)n, OTA_ACK_TIMEOUT_MS))
        {
            LOG.printf("[OTA] FAILED at chunk %u/%u\n", seq, total);
            f.close();
            return false;
        }

        if ((seq & 0x0F) == 0)
        {
            LOG.printf("[OTA] %u%%\n", (unsigned)((uint32_t)seq * 100UL / total));
        }

        yield();
    }

    f.close();

    // ---- end ----
    if (!send_frame(total, NULL, 0, OTA_END_TIMEOUT_MS))
    {
        LOG.println("[OTA] END not acknowledged");
        return false;
    }

    LOG.println("[OTA] DONE - S3 should reboot");
    return true;
}

// ==================================================
// Setup / Loop
// ==================================================

void setup(void)
{
    WiFi.mode(WIFI_OFF);            // less ADC noise, less current

    LOG.begin(115200);
    LOG.println();
    LOG.println("ESP8266 sender starting");

    Serial.setRxBufferSize(1024);
    Serial.begin(LINK_BAUD);
    //Serial.swap();                  // UART0 -> GPIO15 (TX) / GPIO13 (RX)

    if (!LittleFS.begin())
    {
        LOG.println("LittleFS mount failed");
    }
}

void loop(void)
{
    static uint32_t last_send = 0;
    static char     rx_line[24];
    static uint8_t  rx_len = 0;

    // ---- commands from the S3 (update button on its screen) ----
    while (Serial.available())
    {
        char c = (char)Serial.read();

        if (c == '\n' || c == '\r')
        {
            if (rx_len > 0)
            {
                rx_line[rx_len] = '\0';
                rx_len = 0;

                if (strcmp(rx_line, "OTA_REQUEST") == 0)
                {
                    ota_run();
                    last_send = millis();
                    return;
                }
            }
        }
        else if (rx_len < sizeof(rx_line) - 1)
        {
            rx_line[rx_len++] = c;
        }
        else
        {
            rx_len = 0;
        }
    }

    uint32_t now = millis();

    // ---- normal V,I output ----
    if (now - last_send >= SEND_PERIOD_MS)
    {
        last_send = now;
        send_values(read_voltage(), CURRENT_FIXED_VALUE);
    }
}