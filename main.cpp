#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "esp_task_wdt.h"
#include "esp_attr.h"

extern "C"
{
#include "ui/ui.h"
#include "ui/screens.h"
}

#include "tasks.h"
#include "screen_manager.h"
#include "MainPage.h"
#include "SettingsPage.h"
#include "BuzzerPage.h"
#include "VCRangePage.h"
#include "USBUpdate.h"


// ==================================================
// Configuration
// ==================================================

#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240

#define LVGL_BUF_LINES      40

#define TOUCH_CAL_X_MIN     351
#define TOUCH_CAL_X_MAX     3465
#define TOUCH_CAL_Y_MIN     306
#define TOUCH_CAL_Y_MAX     3446
#define TOUCH_CAL_SWAP_XY   7

#define WDT_TIMEOUT_SECONDS 4

#define TRACE_INTERVAL_MS   2000


// ==================================================
// RTC Debug Marker
// ==================================================

#define DEBUG_RTC_MAGIC      0x4C56474CUL

#define PHASE_NONE           0UL
#define PHASE_BEFORE_TASKS   1UL
#define PHASE_AFTER_TASKS    2UL
#define PHASE_BEFORE_LVGL    3UL
#define PHASE_AFTER_LVGL     4UL
#define PHASE_LOOP_COMPLETE  5UL


RTC_DATA_ATTR uint32_t debug_rtc_magic =
    0;


RTC_DATA_ATTR uint32_t debug_last_phase =
    PHASE_NONE;


// ==================================================
// TFT
// ==================================================

TFT_eSPI tft =
    TFT_eSPI();


// ==================================================
// LVGL Display Buffer
// ==================================================

static lv_disp_draw_buf_t draw_buf;


static lv_color_t buf1[
    SCREEN_WIDTH *
    LVGL_BUF_LINES
];


static lv_color_t buf2[
    SCREEN_WIDTH *
    LVGL_BUF_LINES
];


// ==================================================
// Touch Calibration
// ==================================================

static uint16_t calData[5] =
{
    TOUCH_CAL_X_MIN,
    TOUCH_CAL_X_MAX,
    TOUCH_CAL_Y_MIN,
    TOUCH_CAL_Y_MAX,
    TOUCH_CAL_SWAP_XY
};


// ==================================================
// Watchdog
// ==================================================

static void watchdog_init(void)
{
    Serial0.println(
        "[DBG][WDT] init"
    );


    esp_err_t result =
        esp_task_wdt_init(
            WDT_TIMEOUT_SECONDS,
            true
        );


    if (
        result !=
        ESP_OK
    )
    {
        Serial0.printf(
            "[DBG][WDT] init result=%d\n",
            (int)result
        );
    }


    esp_err_t add_result =
        esp_task_wdt_add(
            NULL
        );


    if (
        add_result != ESP_OK &&
        add_result != ESP_ERR_INVALID_STATE
    )
    {
        Serial0.printf(
            "[DBG][WDT] add result=%d\n",
            (int)add_result
        );
    }


    Serial0.printf(
        "[DBG][WDT] enabled timeout=%d sec\n",
        WDT_TIMEOUT_SECONDS
    );
}


// ==================================================
// Watchdog Feed
// ==================================================

static inline void watchdog_feed(void)
{
    esp_task_wdt_reset();
}


// ==================================================
// Runtime Debug
// ==================================================

extern "C"
void debug_runtime(
    const char *format,
    ...
)
{
    if (
        format ==
        NULL
    )
    {
        return;
    }


    char buffer[256];


    va_list args;


    va_start(
        args,
        format
    );


    vsnprintf(
        buffer,
        sizeof(buffer),
        format,
        args
    );


    va_end(
        args
    );


    Serial0.print(
        buffer
    );
}


// ==================================================
// Phase Name
// ==================================================

static const char *phase_name(
    uint32_t phase
)
{
    switch (phase)
    {
        case PHASE_BEFORE_TASKS:
            return "BEFORE_TASKS";


        case PHASE_AFTER_TASKS:
            return "AFTER_TASKS";


        case PHASE_BEFORE_LVGL:
            return "BEFORE_LVGL";


        case PHASE_AFTER_LVGL:
            return "AFTER_LVGL";


        case PHASE_LOOP_COMPLETE:
            return "LOOP_COMPLETE";


        case PHASE_NONE:

        default:
            return "NONE";
    }
}


// ==================================================
// Display Flush
// ==================================================

static void my_disp_flush(
    lv_disp_drv_t *disp,
    const lv_area_t *area,
    lv_color_t *color_p
)
{
    if (
        disp == NULL ||
        area == NULL ||
        color_p == NULL
    )
    {
        if (
            disp != NULL
        )
        {
            lv_disp_flush_ready(
                disp
            );
        }

        return;
    }


    uint32_t w =
        area->x2 -
        area->x1 +
        1;


    uint32_t h =
        area->y2 -
        area->y1 +
        1;


    tft.startWrite();


    tft.setAddrWindow(
        area->x1,
        area->y1,
        w,
        h
    );


    tft.pushColors(
        reinterpret_cast<uint16_t *>(
            &color_p->full
        ),
        w * h,
        true
    );


    tft.endWrite();


    lv_disp_flush_ready(
        disp
    );
}


// ==================================================
// Touch Read
// ==================================================

static void my_touchpad_read(
    lv_indev_drv_t *indev_driver,
    lv_indev_data_t *data
)
{
    (void)indev_driver;


    uint16_t x = 0;
    uint16_t y = 0;


    bool touched =
        tft.getTouch(
            &x,
            &y
        );


    if (
        !touched
    )
    {
        data->state =
            LV_INDEV_STATE_REL;

        return;
    }


    data->state =
        LV_INDEV_STATE_PR;


    data->point.x =
        x;


    data->point.y =
        y;
}


// ==================================================
// LVGL Display Init
// ==================================================

static void lvgl_display_init(void)
{
    lv_disp_draw_buf_init(
        &draw_buf,
        buf1,
        buf2,
        SCREEN_WIDTH *
        LVGL_BUF_LINES
    );


    static lv_disp_drv_t disp_drv;


    lv_disp_drv_init(
        &disp_drv
    );


    disp_drv.hor_res =
        SCREEN_WIDTH;


    disp_drv.ver_res =
        SCREEN_HEIGHT;


    disp_drv.flush_cb =
        my_disp_flush;


    disp_drv.draw_buf =
        &draw_buf;


    lv_disp_drv_register(
        &disp_drv
    );
}


// ==================================================
// LVGL Touch Init
// ==================================================

static void lvgl_touch_init(void)
{
    tft.setTouch(
        calData
    );


    static lv_indev_drv_t indev_drv;


    lv_indev_drv_init(
        &indev_drv
    );


    indev_drv.type =
        LV_INDEV_TYPE_POINTER;


    indev_drv.read_cb =
        my_touchpad_read;


    lv_indev_drv_register(
        &indev_drv
    );
}


// ==================================================
// SETUP
// ==================================================

void setup(void)
{
    // --------------------------------------------------
    // UART0 -> CH343 -> COM16
    // --------------------------------------------------

    Serial0.begin(
        115200
    );


    delay(
        200
    );


    Serial0.println();


    Serial0.println(
        "================================"
    );


    Serial0.println(
        "ESP32 LVGL Application Starting"
    );


    Serial0.println(
        "================================"
    );


    // --------------------------------------------------
    // Reset Reason
    // --------------------------------------------------

    esp_reset_reason_t reset_reason =
        esp_reset_reason();


    const char *reason_text =
        "OTHER";


    if (
        reset_reason ==
        ESP_RST_POWERON
    )
    {
        reason_text =
            "POWER_ON";
    }
    else if (
        reset_reason ==
        ESP_RST_TASK_WDT
    )
    {
        reason_text =
            "TASK_WDT";
    }
    else if (
        reset_reason ==
        ESP_RST_SW
    )
    {
        reason_text =
            "SOFTWARE";
    }


    Serial0.printf(
        "[RESET] code=%d reason=%s\n",
        (int)reset_reason,
        reason_text
    );


    // --------------------------------------------------
    // Previous RTC Phase
    // --------------------------------------------------

    if (
        debug_rtc_magic ==
        DEBUG_RTC_MAGIC
    )
    {
        Serial0.printf(
            "[DBG][CRASH] previous phase=%lu (%s)\n",
            (unsigned long)debug_last_phase,
            phase_name(
                debug_last_phase
            )
        );
    }
    else
    {
        Serial0.println(
            "[DBG][CRASH] previous phase=INVALID"
        );
    }


    // --------------------------------------------------
    // Prepare RTC Marker
    // --------------------------------------------------

    debug_rtc_magic =
        DEBUG_RTC_MAGIC;


    debug_last_phase =
        PHASE_NONE;


    // --------------------------------------------------
    // Watchdog
    // --------------------------------------------------

    watchdog_init();


    // --------------------------------------------------
    // TFT
    // --------------------------------------------------

    Serial0.println(
        "[DBG][MAIN] TFT init BEGIN"
    );


    tft.begin();


    tft.setRotation(
        1
    );


    Serial0.println(
        "[DBG][MAIN] TFT init END"
    );


    // --------------------------------------------------
    // LVGL
    // --------------------------------------------------

    Serial0.println(
        "[DBG][MAIN] lv_init BEGIN"
    );


    lv_init();


    Serial0.println(
        "[DBG][MAIN] lv_init END"
    );


    // --------------------------------------------------
    // Display
    // --------------------------------------------------

    lvgl_display_init();


    // --------------------------------------------------
    // Touch
    // --------------------------------------------------

    lvgl_touch_init();


    // --------------------------------------------------
    // EEZ UI
    // --------------------------------------------------

    Serial0.println(
        "[DBG][MAIN] ui_init BEGIN"
    );


    ui_init();


    Serial0.println(
        "[DBG][MAIN] ui_init END"
    );


    // --------------------------------------------------
    // Screen Manager
    // --------------------------------------------------

    Serial0.println(
        "[DBG][MAIN] screen_manager_init BEGIN"
    );


    screen_manager_init();


    Serial0.println(
        "[DBG][MAIN] screen_manager_init END"
    );


    // --------------------------------------------------
    // Tasks
    // --------------------------------------------------

    Serial0.println(
        "[DBG][MAIN] tasks_init BEGIN"
    );


    tasks_init();


    Serial0.println(
        "[DBG][MAIN] tasks_init END"
    );


    // --------------------------------------------------
    // USB Update Module
    // --------------------------------------------------

    Serial0.println(
        "[DBG][MAIN] usb_update_init BEGIN"
    );


    usb_update_init();


    Serial0.println(
        "[DBG][MAIN] usb_update_init END"
    );


    // --------------------------------------------------
    // Initial Watchdog Feed
    // --------------------------------------------------

    watchdog_feed();


    // --------------------------------------------------
    // Boot Runtime
    // --------------------------------------------------

    debug_runtime(
        "[DBG][RUNTIME] %lu ms | "
        "BOOT COMPLETE | "
        "screen=%d | "
        "heap=%u | "
        "min=%u | "
        "block=%u | "
        "stack=%u\n",

        millis(),

        (int)screen_manager_get(),

        ESP.getFreeHeap(),

        ESP.getMinFreeHeap(),

        ESP.getMaxAllocHeap(),

        uxTaskGetStackHighWaterMark(
            NULL
        )
    );
}


// ==================================================
// Trace
// ==================================================

static uint32_t trace_timer =
    0;


// ==================================================
// LOOP
// ==================================================

void loop(void)
{
    uint32_t loop_start =
        micros();


    uint32_t now =
        millis();


    bool trace =
        (
            now -
            trace_timer >=
            TRACE_INTERVAL_MS
        );


    if (
        trace
    )
    {
        trace_timer =
            now;


        Serial0.println();


        Serial0.println(
            "[TRACE][LOOP] ===== HEARTBEAT ====="
        );


        Serial0.printf(
            "[TRACE][LOOP] "
            "screen=%d heap=%u min=%u stack=%u\n",

            (int)screen_manager_get(),

            ESP.getFreeHeap(),

            ESP.getMinFreeHeap(),

            uxTaskGetStackHighWaterMark(
                NULL
            )
        );


        Serial0.printf(
            "[TRACE][LOOP] "
            "previous_runtime_phase=%lu (%s)\n",

            (unsigned long)debug_last_phase,

            phase_name(
                debug_last_phase
            )
        );
    }


    // ==================================================
    // PHASE 1
    // ==================================================

    debug_last_phase =
        PHASE_BEFORE_TASKS;


    watchdog_feed();


    tasks_run();


    // ==================================================
    // PHASE 2
    // ==================================================

    debug_last_phase =
        PHASE_AFTER_TASKS;


    watchdog_feed();


    // ==================================================
    // PHASE 3
    // ==================================================

    debug_last_phase =
        PHASE_BEFORE_LVGL;


    lv_timer_handler();


    // ==================================================
    // PHASE 4
    // ==================================================

    debug_last_phase =
        PHASE_AFTER_LVGL;


    watchdog_feed();


    // ==================================================
    // Loop timing
    // ==================================================

    uint32_t loop_dt =
        micros() -
        loop_start;


    static uint32_t max_loop_us =
        0;


    if (
        loop_dt >
        max_loop_us
    )
    {
        max_loop_us =
            loop_dt;
    }


    if (
        loop_dt >
        200000UL
    )
    {
        Serial0.printf(
            "[DBG][LOOP] LONG LOOP "
            "dt=%lu us\n",

            (unsigned long)loop_dt
        );
    }


    // ==================================================
    // PHASE 5
    // ==================================================

    debug_last_phase =
        PHASE_LOOP_COMPLETE;


    if (
        trace
    )
    {
        Serial0.printf(
            "[TRACE][LOOP] "
            "max=%lu us current=%lu us\n",

            (unsigned long)max_loop_us,

            (unsigned long)loop_dt
        );


        Serial0.println(
            "[TRACE][LOOP] ===== END HEARTBEAT ====="
        );
    }


    yield();
}