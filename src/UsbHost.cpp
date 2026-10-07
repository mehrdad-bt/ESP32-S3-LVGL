#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_vfs_fat.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"

#include "UsbHost.h"

#define USB_MOUNT_POINT "/usb"

// The enum is declared anonymously inside msc_host_event_t, so in C++
// its enumerators must be qualified with the struct name.
#define EV_CONNECTED     msc_host_event_t::MSC_DEVICE_CONNECTED
#define EV_DISCONNECTED  msc_host_event_t::MSC_DEVICE_DISCONNECTED

static QueueHandle_t            evt_queue = NULL;
static msc_host_device_handle_t msc_dev   = NULL;
static msc_host_vfs_handle_t    msc_vfs   = NULL;

static volatile bool     mounted    = false;
static volatile uint32_t generation = 0;

// Runs in the MSC driver task: only forward the event
static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    (void)arg;
    if (evt_queue != NULL)
        xQueueSend(evt_queue, event, 0);
}

// USB host library event pump
static void usb_lib_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);

        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            usb_host_device_free_all();
    }
}

// Mount / unmount handler
static void usb_app_task(void *arg)
{
    (void)arg;
    msc_host_event_t ev;

    for (;;)
    {
        if (xQueueReceive(evt_queue, &ev, portMAX_DELAY) != pdTRUE)
            continue;

        if (ev.event == EV_CONNECTED)
        {
            Serial0.println("[USB] device connected");

            if (msc_dev != NULL) continue;

            if (msc_host_install_device(ev.device.address, &msc_dev) != ESP_OK)
            {
                Serial0.println("[USB] install_device FAILED");
                msc_dev = NULL;
                continue;
            }

            esp_vfs_fat_mount_config_t mc = {};
            mc.format_if_mount_failed = false;
            mc.max_files              = 4;
            mc.allocation_unit_size   = 8192;

            if (msc_host_vfs_register(msc_dev, USB_MOUNT_POINT, &mc, &msc_vfs) != ESP_OK)
            {
                Serial0.println("[USB] mount FAILED (is it FAT32?)");
                msc_host_uninstall_device(msc_dev);
                msc_dev = NULL;
                msc_vfs = NULL;
                continue;
            }

            Serial0.println("[USB] mounted on /usb");
            mounted    = true;
            generation = generation + 1;
        }
        else if (ev.event == EV_DISCONNECTED)
        {
            Serial0.println("[USB] device removed");

            mounted = false;

            if (msc_vfs != NULL)
            {
                msc_host_vfs_unregister(msc_vfs);
                msc_vfs = NULL;
            }
            if (msc_dev != NULL)
            {
                msc_host_uninstall_device(msc_dev);
                msc_dev = NULL;
            }
            generation = generation + 1;
        }
    }
}

bool usb_msc_start(void)
{
    evt_queue = xQueueCreate(4, sizeof(msc_host_event_t));
    if (evt_queue == NULL) return false;

    usb_host_config_t hc = {};
    hc.skip_phy_setup = false;
    hc.intr_flags     = ESP_INTR_FLAG_LEVEL1;

    if (usb_host_install(&hc) != ESP_OK)
    {
        Serial0.println("[USB] usb_host_install FAILED");
        return false;
    }

    xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, NULL, 2, NULL, 0);

    msc_host_driver_config_t mc = {};
    mc.create_backround_task = true;   // (sic) the field name has this typo
    mc.task_priority         = 5;
    mc.stack_size            = 4096;
    mc.core_id               = tskNO_AFFINITY;
    mc.callback              = msc_event_cb;
    mc.callback_arg          = NULL;

    if (msc_host_install(&mc) != ESP_OK)
    {
        Serial0.println("[USB] msc_host_install FAILED");
        return false;
    }

    xTaskCreatePinnedToCore(usb_app_task, "usb_app", 4096, NULL, 3, NULL, 0);

    Serial0.println("[USB] host started");
    return true;
}

bool     usb_msc_mounted(void)     { return mounted; }
uint32_t usb_msc_generation(void)  { return generation; }