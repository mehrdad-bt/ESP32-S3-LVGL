#include <Arduino.h>
#include <lvgl.h>

#include "MainPage.h"
#include "font_persian_16.h"
#include "font_persian_24.h"
#include "screen_manager.h"
#include "system_error.h"

extern "C"
{
#include "ui/ui.h"
#include "ui/screens.h"
}

static bool page_was_active = false;

// ==================================================
// Message Box State
// ==================================================

static ErrorType last_msg_box_error =
    ERROR_NONE;

// ==================================================
// Configure Settings Button Text
// ==================================================

static void configure_settings_button_text(void)
{
    if (objects.settings_button_text == NULL)
    {
        return;
    }

    lv_label_set_text(
        objects.settings_button_text,
        "تنظیمات"
    );

    lv_obj_set_style_text_font(
        objects.settings_button_text,
        &font_persian_16,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_base_dir(
        objects.settings_button_text,
        LV_BASE_DIR_RTL,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_text_color(
        objects.settings_button_text,
        lv_color_hex(0x000000),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );
}

// ==================================================
// Configure Main Page Persian Labels
// ==================================================

static void configure_main_page_labels(void)
{
    // --------------------------------------------------
    // Status
    // --------------------------------------------------

    if (objects.status_text_main_page != NULL)
    {
        lv_label_set_text(
            objects.status_text_main_page,
            "وضعیت"
        );

        lv_obj_set_style_text_font(
            objects.status_text_main_page,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.status_text_main_page,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // --------------------------------------------------
    // Voltage Static Label
    // --------------------------------------------------

    if (objects.voltage_label_main_static != NULL)
    {
        lv_label_set_text(
            objects.voltage_label_main_static,
            "ولتاژ"
        );

        lv_obj_set_style_text_font(
            objects.voltage_label_main_static,
            &font_persian_24,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.voltage_label_main_static,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // --------------------------------------------------
    // Current Static Label
    // --------------------------------------------------

    if (objects.current_label_main_static != NULL)
    {
        lv_label_set_text(
            objects.current_label_main_static,
            "جریان"
        );

        lv_obj_set_style_text_font(
            objects.current_label_main_static,
            &font_persian_24,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.current_label_main_static,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }
}

// ==================================================
// Configure / Hide Message Box
// ==================================================

static void configure_message_box(void)
{
    if (objects.msg_box == NULL)
    {
        return;
    }

    // Keep the EEZ-generated msg_box. We only configure its
    // existing object from this page file, so screen.c remains
    // completely untouched.
    lv_obj_set_pos(
        objects.msg_box,
        63,
        72
    );

    lv_obj_set_size(
        objects.msg_box,
        194,
        96
    );

    lv_obj_set_style_bg_color(
        objects.msg_box,
        lv_color_hex(0x202020),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_bg_opa(
        objects.msg_box,
        LV_OPA_COVER,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_border_width(
        objects.msg_box,
        2,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_border_color(
        objects.msg_box,
        lv_color_hex(0xFF0000),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_radius(
        objects.msg_box,
        8,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    if (objects.msg_box_text != NULL)
    {
        lv_obj_set_style_text_font(
            objects.msg_box_text,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_text_color(
            objects.msg_box_text,
            lv_color_hex(0xFFFFFF),
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.msg_box_text,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_label_set_long_mode(
            objects.msg_box_text,
            LV_LABEL_LONG_WRAP
        );

        lv_obj_set_width(
            objects.msg_box_text,
            150
        );

        lv_obj_align(
            objects.msg_box_text,
            LV_ALIGN_CENTER,
            0,
            0
        );

        lv_label_set_text(
            objects.msg_box_text,
            ""
        );
    }

    // Hidden until a real error is detected.
    lv_obj_add_flag(
        objects.msg_box,
        LV_OBJ_FLAG_HIDDEN
    );

    last_msg_box_error =
        ERROR_NONE;
}

// ==================================================
// Update Message Box
// ==================================================

static void update_message_box(void)
{
    if (!screen_manager_is(SCREEN_ID_MAIN))
    {
        return;
    }

    if (objects.msg_box == NULL)
    {
        return;
    }

    ErrorType error =
        tasks_get_error_type();

    // Only these three conditions are requested for the message box.
    const char *message =
        NULL;

    switch (error)
    {
        case ERROR_VOLTAGE_LOW:
            message =
                "ولتاژ پایین است";
            break;

        case ERROR_VOLTAGE_HIGH:
            message =
                "ولتاژ بالا است";
            break;

        case ERROR_CONNECTION:
            message =
                "ارتباط UART قطع شده است";
            break;

        default:
            message =
                NULL;
            break;
    }

    // Avoid touching LVGL every 20 ms when nothing changed.
    if (error == last_msg_box_error)
    {
        return;
    }

    last_msg_box_error =
        error;

    if (message == NULL)
    {
        lv_obj_add_flag(
            objects.msg_box,
            LV_OBJ_FLAG_HIDDEN
        );

        return;
    }

    if (objects.msg_box_text != NULL)
    {
        lv_label_set_text(
            objects.msg_box_text,
            message
        );
    }

    lv_obj_clear_flag(
        objects.msg_box,
        LV_OBJ_FLAG_HIDDEN
    );
}

// ==================================================
// Initialization
// ==================================================

void main_page_init(void)
{
    page_was_active = false;
    last_msg_box_error = ERROR_NONE;

    if (objects.msg_box != NULL)
    {
        lv_obj_add_flag(
            objects.msg_box,
            LV_OBJ_FLAG_HIDDEN
        );
    }
}

// ==================================================
// Update
// ==================================================

void main_page_update(void)
{
    if (!screen_manager_is(SCREEN_ID_MAIN))
    {
        page_was_active = false;
        return;
    }

    if (!page_was_active)
    {
        configure_settings_button_text();
        configure_main_page_labels();
        configure_message_box();
        page_was_active = true;
    }

    update_message_box();
}
