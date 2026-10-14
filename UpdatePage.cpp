#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>

#include "UpdatePage.h"
#include "screen_manager.h"
#include "ota_uart.h"
#include "font_persian_16.h"
#include "font_persian_24.h"

extern "C"
{
#include "ui/screens.h"
}

// ==================================================
// Language
// ==================================================
// 1 = Persian (needs the glyphs in font_persian_16 / _24)
// 0 = English fallback (default LVGL font), useful if a glyph is missing

#define UPDATE_UI_PERSIAN 1

#if UPDATE_UI_PERSIAN

#define STR_HEADER      "آپدیت برنامه"
#define STR_BUTTON      "آپدیت"
#define STR_EXIT        "خروج"

#define STR_IDLE        "برای شروع دکمه را بزنید"
#define STR_WAIT        "در انتظار ارسال فایل"
#define STR_RECEIVING   "در حال دریافت"
#define STR_VERIFYING   "در حال بررسی فایل"
#define STR_SUCCESS     "آپدیت موفق بود\nراه اندازی مجدد"
#define STR_FAILED      "آپدیت ناموفق بود"

#define STR_ERR_NO_RESPONSE "ارتباط برقرار نشد"
#define STR_ERR_BAD_BEGIN   "درخواست نامعتبر"
#define STR_ERR_NO_SPACE    "حافظه کافی نیست"
#define STR_ERR_WRITE       "خطا در نوشتن حافظه"
#define STR_ERR_TIMEOUT     "ارتباط قطع شد"
#define STR_ERR_CRC         "فایل خراب است"
#define STR_ERR_SIZE        "حجم فایل اشتباه است"
#define STR_ERR_FINISH      "فایل نامعتبر است"

#else

#define STR_HEADER      "Firmware update"
#define STR_BUTTON      "update"
#define STR_EXIT        "EXIT"

#define STR_IDLE        "Press update to start"
#define STR_WAIT        "Waiting for file"
#define STR_RECEIVING   "Receiving"
#define STR_VERIFYING   "Verifying"
#define STR_SUCCESS     "Update OK\nRebooting"
#define STR_FAILED      "Update failed"

#define STR_ERR_NO_RESPONSE "No response"
#define STR_ERR_BAD_BEGIN   "Bad request"
#define STR_ERR_NO_SPACE    "Not enough space"
#define STR_ERR_WRITE       "Flash write error"
#define STR_ERR_TIMEOUT     "Link lost"
#define STR_ERR_CRC         "File corrupted"
#define STR_ERR_SIZE        "Wrong file size"
#define STR_ERR_FINISH      "Invalid image"

#endif

// ==================================================
// State
// ==================================================

static bool page_was_active = false;

static int  last_state   = -1;
static int  last_error   = -1;
static int  last_percent = -1;

// ==================================================
// Helpers
// ==================================================

static void style_label(lv_obj_t *obj, const lv_font_t *font)
{
    if (obj == NULL)
    {
        return;
    }

#if UPDATE_UI_PERSIAN
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_base_dir(obj, LV_BASE_DIR_RTL, LV_PART_MAIN | LV_STATE_DEFAULT);
#else
    (void)font;
#endif
}

static void configure_update_labels(void)
{
    if (objects.update_page_header_text != NULL)
    {
        lv_label_set_text(objects.update_page_header_text, STR_HEADER);
        style_label(objects.update_page_header_text, &font_persian_24);

        lv_obj_set_style_text_color(
            objects.update_page_header_text,
            lv_color_hex(0xEBF900),
            LV_PART_MAIN | LV_STATE_DEFAULT
        );
    }

    if (objects.update_button_text != NULL)
    {
        lv_label_set_text(objects.update_button_text, STR_BUTTON);
        style_label(objects.update_button_text, &font_persian_16);

        lv_obj_set_style_text_color(
            objects.update_button_text,
            lv_color_hex(0xFFFFFF),
            LV_PART_MAIN | LV_STATE_DEFAULT
        );
    }

    if (objects.exit_button_update_page_text != NULL)
    {
        lv_label_set_text(objects.exit_button_update_page_text, STR_EXIT);
        style_label(objects.exit_button_update_page_text, &font_persian_16);
    }

    if (objects.update_status_text != NULL)
    {
        style_label(objects.update_status_text, &font_persian_16);

        lv_label_set_long_mode(objects.update_status_text, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(objects.update_status_text, 280);
        lv_obj_set_pos(objects.update_status_text, 20, 60);

        lv_obj_set_style_text_align(
            objects.update_status_text,
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN | LV_STATE_DEFAULT
        );
    }
}

static const char *error_text(OtaError e)
{
    switch (e)
    {
        case OTA_ERR_NO_RESPONSE: return STR_ERR_NO_RESPONSE;
        case OTA_ERR_BAD_BEGIN:   return STR_ERR_BAD_BEGIN;
        case OTA_ERR_NO_SPACE:    return STR_ERR_NO_SPACE;
        case OTA_ERR_WRITE:       return STR_ERR_WRITE;
        case OTA_ERR_TIMEOUT:     return STR_ERR_TIMEOUT;
        case OTA_ERR_CRC:         return STR_ERR_CRC;
        case OTA_ERR_SIZE:        return STR_ERR_SIZE;
        case OTA_ERR_FINISH:      return STR_ERR_FINISH;
        default:                  return "";
    }
}

static void set_status(const char *text, uint32_t color)
{
    if (objects.update_status_text == NULL)
    {
        return;
    }

    lv_label_set_text(objects.update_status_text, text);

    lv_obj_set_style_text_color(
        objects.update_status_text,
        lv_color_hex(color),
        LV_PART_MAIN | LV_STATE_DEFAULT
    );
}

// ==================================================
// Init
// ==================================================

void update_page_init(void)
{
    page_was_active = false;

    last_state   = -1;
    last_error   = -1;
    last_percent = -1;
}

// ==================================================
// Update
// ==================================================

void update_page_update(void)
{
    if (!screen_manager_is(SCREEN_ID_UPDATE_PAGE))
    {
        if (page_was_active)
        {
            ota_uart_clear_result();
            page_was_active = false;
        }

        return;
    }

    if (!page_was_active)
    {
        configure_update_labels();

        page_was_active = true;

        last_state   = -1;
        last_error   = -1;
        last_percent = -1;
    }

    OtaState state   = ota_uart_get_state();
    OtaError error   = ota_uart_get_error();
    int      percent = (int)ota_uart_get_percent();

    if ((int)state   == last_state &&
        (int)error   == last_error &&
        percent      == last_percent)
    {
        return;
    }

    last_state   = (int)state;
    last_error   = (int)error;
    last_percent = percent;

    char text[96];

    switch (state)
    {
        case OTA_STATE_WAIT_BEGIN:

            set_status(STR_WAIT, 0xFFFF00);
            break;

        case OTA_STATE_RECEIVING:

            snprintf(text, sizeof(text), "%s %d%%", STR_RECEIVING, percent);
            set_status(text, 0x00FFFF);
            break;

        case OTA_STATE_VERIFYING:

            set_status(STR_VERIFYING, 0x00FFFF);
            break;

        case OTA_STATE_SUCCESS:

            set_status(STR_SUCCESS, 0x00FF00);
            break;

        case OTA_STATE_FAILED:

            snprintf(text, sizeof(text), "%s\n%s", STR_FAILED, error_text(error));
            set_status(text, 0xFF3030);
            break;

        case OTA_STATE_IDLE:
        default:

            set_status(STR_IDLE, 0xFFFFFF);
            break;
    }

    // Disable the update button while a transfer is running.
    if (objects.update_button != NULL)
    {
        if (ota_uart_is_busy())
        {
            lv_obj_add_state(objects.update_button, LV_STATE_DISABLED);
        }
        else
        {
            lv_obj_clear_state(objects.update_button, LV_STATE_DISABLED);
        }
    }
}