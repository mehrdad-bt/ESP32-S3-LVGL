#ifndef OTA_UART_H
#define OTA_UART_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================================================
// OTA over UART (ESP32-S3 = receiver, ESP8266 = sender)
// Uses Serial1 (RX=GPIO44, TX=GPIO43).
// Protocol is described at the top of esp8266_main.cpp.
// ==================================================

typedef enum
{
    OTA_STATE_IDLE = 0,
    OTA_STATE_WAIT_BEGIN,      // OTA_REQUEST sent, waiting for the sender
    OTA_STATE_RECEIVING,       // chunks are being received and written
    OTA_STATE_VERIFYING,       // CRC32 check + Update.end()
    OTA_STATE_SUCCESS,         // done, reboot pending
    OTA_STATE_FAILED           // see ota_uart_get_error()
} OtaState;

typedef enum
{
    OTA_ERR_NONE = 0,
    OTA_ERR_NO_RESPONSE,       // sender did not answer
    OTA_ERR_BAD_BEGIN,         // invalid OTA_BEGIN line
    OTA_ERR_NO_SPACE,          // Update.begin() failed (no OTA partition / too big)
    OTA_ERR_WRITE,             // flash write failed
    OTA_ERR_TIMEOUT,           // link went silent during transfer
    OTA_ERR_CRC,               // whole-file CRC32 mismatch
    OTA_ERR_SIZE,              // received size != announced size
    OTA_ERR_FINISH             // Update.end() rejected the image
} OtaError;

// Call every loop iteration (from tasks_run()).
void ota_uart_task(void);

// Ask the ESP8266 to start sending. Returns false if already busy.
bool ota_uart_request_start(void);

// true while this module owns Serial1 (normal V,I parser must stay silent).
bool ota_uart_is_active(void);

// true while an update is in progress (UI must not leave the page / restart).
bool ota_uart_is_busy(void);

// Back to IDLE if the last attempt failed (call when leaving the page).
void ota_uart_clear_result(void);

OtaState ota_uart_get_state(void);
OtaError ota_uart_get_error(void);
uint8_t  ota_uart_get_percent(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_UART_H */