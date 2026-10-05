#ifndef USB_UPDATE_H
#define USB_UPDATE_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    USB_UPDATE_STATUS_IDLE = 0,
    USB_UPDATE_STATUS_WAITING,
    USB_UPDATE_STATUS_RECEIVING,
    USB_UPDATE_STATUS_VERIFYING,
    USB_UPDATE_STATUS_SUCCESS,
    USB_UPDATE_STATUS_ERROR,
    USB_UPDATE_STATUS_CANCELED
} UsbUpdateStatus;


typedef enum
{
    USB_UPDATE_ERROR_NONE = 0,
    USB_UPDATE_ERROR_BAD_HEADER,
    USB_UPDATE_ERROR_INVALID_SIZE,
    USB_UPDATE_ERROR_NO_OTA,
    USB_UPDATE_ERROR_BEGIN_FAILED,
    USB_UPDATE_ERROR_MD5_FAILED,
    USB_UPDATE_ERROR_WRITE_FAILED,
    USB_UPDATE_ERROR_TIMEOUT,
    USB_UPDATE_ERROR_VERIFY_FAILED,
    USB_UPDATE_ERROR_CANCELED
} UsbUpdateError;


void usb_update_init(void);

bool usb_update_start(void);

void usb_update_cancel(void);

UsbUpdateStatus usb_update_get_status(void);

UsbUpdateError usb_update_get_error(void);

uint32_t usb_update_get_total_size(void);

uint32_t usb_update_get_received_size(void);

bool usb_update_is_active(void);

void usb_update_gui_update(void);

#ifdef __cplusplus
}
#endif

#endif