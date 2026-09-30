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

// ==================================================
// Page State
// ==================================================

static bool page_was_active =
    false;

// ==================================================
// Message Box State
// ==================================================

static ErrorType last_msg_box_error =
    ERROR_NONE;

// ==================================================
// Message Box Colors
// ==================================================

#define MSGBOX_WARNING_COLOR  0xFFA500
#define MSGBOX_ERROR_COLOR    0xFF0000

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
// Configure Message Box
// ==================================================

static void configure_message_box(void)
{
    if (objects.msg_box == NULL)
    {
        return;
    }

    // --------------------------------------------------
    // Keep the EEZ-generated size and position
    // --------------------------------------------------

    lv_obj_set_pos(
        objects.msg_box,
        43,
        56
    );

    lv_obj_set_size(
        objects.msg_box,
        228,
        128
    );

    // --------------------------------------------------
    // Default appearance
    // --------------------------------------------------

    lv_obj_set_style_bg_color(
        objects.msg_box,
        lv_color_hex(MSGBOX_ERROR_COLOR),
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
        3,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_border_color(
        objects.msg_box,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_radius(
        objects.msg_box,
        8,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    // --------------------------------------------------
    // Message Text
    // --------------------------------------------------

    if (objects.msg_box_text != NULL)
    {
        lv_obj_set_style_text_font(
            objects.msg_box_text,
            &font_persian_24,
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

        // --------------------------------------------------
        // Text area
        // --------------------------------------------------

        lv_obj_set_width(
            objects.msg_box_text,
            200
        );

        lv_obj_set_height(
            objects.msg_box_text,
            90
        );

        lv_obj_align(
            objects.msg_box_text,
            LV_ALIGN_CENTER,
            0,
            0
        );

        lv_obj_set_style_text_align(
            objects.msg_box_text,
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_label_set_text(
            objects.msg_box_text,
            ""
        );
    }

    // --------------------------------------------------
    // Hidden initially
    // --------------------------------------------------

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

    const char *message =
        NULL;

    uint32_t box_color =
        MSGBOX_ERROR_COLOR;

    // ==================================================
    // Determine Message and Color
    // ==================================================

    switch (error)
    {
        // --------------------------------------------------
        // UART Connection Warning
        // --------------------------------------------------

        case ERROR_CONNECTION:

            message =
                "ارتباط UART\nقطع شده است";

            box_color =
                MSGBOX_WARNING_COLOR;

            break;

        // --------------------------------------------------
        // Low Voltage Error
        // --------------------------------------------------

        case ERROR_VOLTAGE_LOW:

            message =
                "ولتاژ پایین است";

            box_color =
                MSGBOX_ERROR_COLOR;

            break;

        // --------------------------------------------------
        // High Voltage Error
        // --------------------------------------------------

        case ERROR_VOLTAGE_HIGH:

            message =
                "ولتاژ بالا است";

            box_color =
                MSGBOX_ERROR_COLOR;

            break;

        // --------------------------------------------------
        // No Error
        // --------------------------------------------------

        default:

            message =
                NULL;

            break;
    }

    // ==================================================
    // No State Change
    // ==================================================

    if (error == last_msg_box_error)
    {
        return;
    }

    last_msg_box_error =
        error;

    // ==================================================
    // Hide Message Box
    // ==================================================

    if (message == NULL)
    {
        lv_obj_add_flag(
            objects.msg_box,
            LV_OBJ_FLAG_HIDDEN
        );

        return;
    }

    // ==================================================
    // Set Message
    // ==================================================

    if (objects.msg_box_text != NULL)
    {
        lv_label_set_text(
            objects.msg_box_text,
            message
        );
    }

    // ==================================================
    // Set Box Color
    // ==================================================

    lv_obj_set_style_bg_color(
        objects.msg_box,
        lv_color_hex(box_color),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_border_color(
        objects.msg_box,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    // ==================================================
    // Show
    // ==================================================

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
    page_was_active =
        false;

    last_msg_box_error =
        ERROR_NONE;

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
        page_was_active =
            false;

        return;
    }

    if (!page_was_active)
    {
        configure_settings_button_text();

        configure_main_page_labels();

        configure_message_box();

        page_was_active =
            true;
    }

    update_message_box();
}
