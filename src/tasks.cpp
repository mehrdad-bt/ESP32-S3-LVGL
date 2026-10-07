#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>

extern "C"
{
#include "ui/ui.h"
#include "ui/screens.h"
#include "ui/actions.h"
}

extern lv_obj_t *tick_value_change_obj;

#include "tasks.h"
#include "system_error.h"
#include "uart.h"
#include "screen_manager.h"
#include "ui/vars.h"
#include "MainPage.h"
#include "SettingsPage.h"
#include "BuzzerPage.h"
#include "VCRangePage.h"
#include "Animation.h"
#include "UpdatePage.h"

// ==================================================
// Hardware Configuration
// ==================================================

// --------------------------------------------------
// Buttons
// --------------------------------------------------
// فعلاً غیرفعال هستند.
// بعداً GPIO واقعی را اینجا وارد کن.
//
// #define BTN_RIGHT   xx
// #define BTN_SELECT  xx
//
// برای فعال کردن:
// #define BUTTONS_ENABLED 1

#define BUTTONS_ENABLED 0

// --------------------------------------------------
// Buzzer
// --------------------------------------------------
// فعلاً غیرفعال است.
// بعداً GPIO واقعی را اینجا وارد کن.
//
// #define BUZZER_PIN  xx

#define BUZZER_ENABLED 0

#define BUZZER_FREQ 2000
#define BUZZER_LEDC_CHANNEL 0
#define BUZZER_LEDC_RESOLUTION 10

// ==================================================
// Timing Configuration
// ==================================================

#define BUTTON_DEBOUNCE_MS 50
#define UART_TIMEOUT_MS 3000
#define ERROR_TEXT_BLINK_MS 500

// ==================================================
// Menu Indices
// ==================================================

#define MAIN_OPTION_SETTINGS 0

#define SETTINGS_OPTION_BUZZER        0
#define SETTINGS_OPTION_CALIBRATION   1
#define SETTINGS_OPTION_VC_RANGE      2
#define SETTINGS_OPTION_BACK          3

// ==================================================
// Buzzer Modes
// ==================================================

#define BUZZER_MODE_1  0
#define BUZZER_MODE_2  1
#define BUZZER_MODE_3  2

// ==================================================
// LED Colors
// ==================================================

#define LED_BLUE    0x0000FF
#define LED_GREEN   0x00FF00
#define LED_RED     0xFF0000
#define LED_ORANGE  0xFFA500

#define FOCUS_COLOR 0xFF0000

// ==================================================
// Default Voltage and Current Limits
// ==================================================

#define DEFAULT_VOLTAGE_MIN 20.0f
#define DEFAULT_VOLTAGE_MAX 25.0f

#define DEFAULT_CURRENT_MIN 0.0f
#define DEFAULT_CURRENT_MAX 1.0f

// ==================================================
// Allowed Voltage and Current Limits
// ==================================================

#define VOLTAGE_LIMIT_MIN 0.0f
#define VOLTAGE_LIMIT_MAX 30.0f

#define CURRENT_LIMIT_MIN 0.0f
#define CURRENT_LIMIT_MAX 3.0f

// ==================================================
// System State
// ==================================================

struct SystemState
{
    float voltage;
    float current;

    bool data_received;
    bool uart_timeout;
    bool connection_lost;

    bool voltage_ok;
    bool current_ok;
    bool system_ok;

    bool low_voltage;
};

static SystemState system_state =
{
    0.0f,
    0.0f,

    false,
    true,
    true,

    false,
    false,
    false,

    false
};

// ==================================================
// Limit Values
// ==================================================

static float voltage_min_limit =
    DEFAULT_VOLTAGE_MIN;

static float voltage_max_limit =
    DEFAULT_VOLTAGE_MAX;

static float current_min_limit =
    DEFAULT_CURRENT_MIN;

static float current_max_limit =
    DEFAULT_CURRENT_MAX;

// ==================================================
// UART State
// ==================================================

static uint32_t last_valid_uart_time = 0;

static bool valid_uart_received_once =
    false;

// ==================================================
// Button State
// ==================================================

#if BUTTONS_ENABLED

static bool right_last_state =
    HIGH;

static bool select_last_state =
    HIGH;

static uint32_t right_last_change =
    0;

static uint32_t select_last_change =
    0;

#endif

// ==================================================
// Menu State
// ==================================================

static int main_selection =
    MAIN_OPTION_SETTINGS;

static int settings_selection =
    SETTINGS_OPTION_BUZZER;

// ==================================================
// Buzzer Runtime State
// ==================================================

static uint8_t buzzer_mode =
    BUZZER_MODE_1;

static bool buzzer_output_state =
    false;

static uint32_t buzzer_timer =
    0;

static uint8_t buzzer_phase =
    0;

// ==================================================
// Buzzer LEDC State
// ==================================================

static bool buzzer_ledc_initialized =
    false;

// ==================================================
// Buzzer UI State
// ==================================================

static lv_obj_t *buzzer_dropdown =
    NULL;

static bool buzzer_dropdown_open =
    false;

static bool buzzer_focus_back =
    false;

// ==================================================
// Voltage / Current UI State
// ==================================================

static uint8_t vc_focus =
    0;

static bool vc_edit_mode =
    false;

// ==================================================
// LED Runtime State
// ==================================================

enum LedVisualState
{
    LED_VISUAL_BLUE = 0,
    LED_VISUAL_GREEN,
    LED_VISUAL_ORANGE_BLINK,
    LED_VISUAL_RED_BLINK
};

static LedVisualState led_visual_state =
    LED_VISUAL_BLUE;

static bool led_gui_initialized =
    false;

static bool led_blink_state =
    true;

static uint32_t led_blink_timer =
    0;

// ==================================================
// Main GUI Cache
// ==================================================

static float gui_last_voltage =
    -1000.0f;

static float gui_last_current =
    -1000.0f;

static bool gui_last_low_voltage =
    false;

static bool gui_last_connection_lost =
    false;

static bool gui_last_system_ok =
    false;

// ==================================================
// Voltage / Current GUI Cache
// ==================================================

static float gui_last_voltage_min =
    -1000.0f;

static float gui_last_voltage_max =
    -1000.0f;

static float gui_last_current_min =
    -1000.0f;

static float gui_last_current_max =
    -1000.0f;

// ==================================================
// Focus Screen Cache
// ==================================================

static enum ScreensEnum last_focus_screen =
    SCREEN_ID_MAIN;

static int last_buzzer_focus_state =
    -1;

static int last_vc_focus_state =
    -1;

static bool last_vc_edit_state =
    false;

// ==================================================
// Forward Declarations
// ==================================================

static void update_vc_range_gui(void);
static void update_buzzer_gui(void);
static void update_input_focus_gui(void);

static void led_bisect_style(void);
static void led_apply_color(uint32_t color);
static void led_turn_off(void);
static void set_status_led(uint32_t color);
static void set_status_led_blink(uint32_t color);
static void update_led_state(void);

static ErrorType get_error_type(void);

static bool main_screen_active(void);
static bool vc_range_screen_active(void);
static bool buzzer_screen_active(void);

static void buttons_task(void);

static void buzzer_init_ledc(void);
static void buzzer_start(void);
static void buzzer_stop(void);

static void buzzer_dropdown_find(void);
static void buzzer_dropdown_change(int direction);

static void vc_change_value(void);

static void clear_buzzer_focus(void);
static void apply_buzzer_focus(void);

static void clear_vc_focus(void);
static void apply_vc_focus(void);

static void handle_right_release(void);
static void handle_select_release(void);

// ==================================================
// Page-Owned Public Functions
// ==================================================

void buzzer_set_mode(uint8_t mode);
uint8_t buzzer_get_mode(void);

void set_voltage_min_limit(float value);
void set_voltage_max_limit(float value);
float get_voltage_min_limit(void);
float get_voltage_max_limit(void);

void set_current_min_limit(float value);
void set_current_max_limit(float value);
float get_current_min_limit(void);
float get_current_max_limit(void);

void buzzer_page_handle_right(void);
void buzzer_page_handle_select(void);
void vc_range_page_handle_right(void);
void vc_range_page_handle_select(void);

// ==================================================
// Screen State Helpers
// ==================================================

static bool main_screen_active(void)
{
    if (objects.main == NULL)
    {
        return false;
    }

    return lv_scr_act() == objects.main;
}

static bool vc_range_screen_active(void)
{
    return screen_manager_get() ==
           SCREEN_ID_V_C_RANGE_SETTINGS;
}

static bool buzzer_screen_active(void)
{
    return screen_manager_get() ==
           SCREEN_ID_BUZZER_SETTINGS;
}

// ==================================================
// Buzzer LEDC Initialization
// ==================================================

static void buzzer_init_ledc(void)
{
#if BUZZER_ENABLED

    if (buzzer_ledc_initialized)
    {
        return;
    }

    double result =
        ledcSetup(
            BUZZER_LEDC_CHANNEL,
            BUZZER_FREQ,
            BUZZER_LEDC_RESOLUTION
        );

    if (result <= 0.0)
    {
        buzzer_ledc_initialized =
            false;

        return;
    }

    ledcAttachPin(
        BUZZER_PIN,
        BUZZER_LEDC_CHANNEL
    );

    ledcWriteTone(
        BUZZER_LEDC_CHANNEL,
        0
    );

    buzzer_ledc_initialized =
        true;

#else

    buzzer_ledc_initialized =
        false;

#endif
}

// ==================================================
// Buzzer ON
// ==================================================

static void buzzer_start(void)
{
#if BUZZER_ENABLED

    if (!buzzer_ledc_initialized)
    {
        return;
    }

    ledcWriteTone(
        BUZZER_LEDC_CHANNEL,
        BUZZER_FREQ
    );

#endif
}

// ==================================================
// Buzzer OFF
// ==================================================

static void buzzer_stop(void)
{
#if BUZZER_ENABLED

    if (!buzzer_ledc_initialized)
    {
        return;
    }

    ledcWriteTone(
        BUZZER_LEDC_CHANNEL,
        0
    );

#endif
}

// ==================================================
// Buzzer Dropdown Helper
// ==================================================

static void buzzer_dropdown_find(void)
{
    if (objects.buzzer_settings == NULL)
    {
        buzzer_dropdown = NULL;
        return;
    }

    buzzer_dropdown =
        lv_obj_get_child(
            objects.buzzer_settings,
            0
        );
}

// ==================================================
// Update V/C Range GUI
// ==================================================

static void update_vc_range_gui(void)
{
    if (!vc_range_screen_active())
    {
        return;
    }

    // --------------------------------------------------
    // Voltage minimum display
    // --------------------------------------------------

    if (
        objects.voltage_min_value != NULL &&
        get_voltage_min_limit() !=
        gui_last_voltage_min
    )
    {
        char text[16];

        snprintf(
            text,
            sizeof(text),
            "%.1f",
            get_voltage_min_limit()
        );

        lv_label_set_text(
            objects.voltage_min_value,
            text
        );

        gui_last_voltage_min =
            get_voltage_min_limit();
    }

    // --------------------------------------------------
    // Voltage maximum display
    // --------------------------------------------------

    if (
        objects.voltage_max_value != NULL &&
        get_voltage_max_limit() !=
        gui_last_voltage_max
    )
    {
        char text[16];

        snprintf(
            text,
            sizeof(text),
            "%.1f",
            get_voltage_max_limit()
        );

        lv_label_set_text(
            objects.voltage_max_value,
            text
        );

        gui_last_voltage_max =
            get_voltage_max_limit();
    }

    // --------------------------------------------------
    // Current minimum display
    // --------------------------------------------------

    if (
        objects.current_min_value != NULL &&
        get_current_min_limit() !=
        gui_last_current_min
    )
    {
        char text[16];

        snprintf(
            text,
            sizeof(text),
            "%.1f",
            get_current_min_limit()
        );

        lv_label_set_text(
            objects.current_min_value,
            text
        );

        gui_last_current_min =
            get_current_min_limit();
    }

    // --------------------------------------------------
    // Current maximum display
    // --------------------------------------------------

    if (
        objects.current_max_value != NULL &&
        get_current_max_limit() !=
        gui_last_current_max
    )
    {
        char text[16];

        snprintf(
            text,
            sizeof(text),
            "%.1f",
            get_current_max_limit()
        );

        lv_label_set_text(
            objects.current_max_value,
            text
        );

        gui_last_current_max =
            get_current_max_limit();
    }

    // --------------------------------------------------
    // Voltage minimum slider
    // --------------------------------------------------

    if (objects.voltage_minimum != NULL)
    {
        int32_t value =
            (int32_t)get_voltage_min_limit();

        int32_t current =
            lv_slider_get_value(
                objects.voltage_minimum
            );

        if (value != current)
        {
            tick_value_change_obj =
                objects.voltage_minimum;

            lv_slider_set_value(
                objects.voltage_minimum,
                value,
                LV_ANIM_OFF
            );

            tick_value_change_obj =
                NULL;
        }
    }

    // --------------------------------------------------
    // Voltage maximum slider
    // --------------------------------------------------

    if (objects.voltage_maximum != NULL)
    {
        int32_t value =
            (int32_t)get_voltage_max_limit();

        int32_t current =
            lv_slider_get_value(
                objects.voltage_maximum
            );

        if (value != current)
        {
            tick_value_change_obj =
                objects.voltage_maximum;

            lv_slider_set_value(
                objects.voltage_maximum,
                value,
                LV_ANIM_OFF
            );

            tick_value_change_obj =
                NULL;
        }
    }

    // --------------------------------------------------
    // Current minimum slider
    // --------------------------------------------------

    if (objects.current_minimum != NULL)
    {
        int32_t value =
            (int32_t)get_current_min_limit();

        int32_t current =
            lv_slider_get_value(
                objects.current_minimum
            );

        if (value != current)
        {
            tick_value_change_obj =
                objects.current_minimum;

            lv_slider_set_value(
                objects.current_minimum,
                value,
                LV_ANIM_OFF
            );

            tick_value_change_obj =
                NULL;
        }
    }

    // --------------------------------------------------
    // Current maximum slider
    // --------------------------------------------------

    if (objects.current_maximum != NULL)
    {
        int32_t value =
            (int32_t)get_current_max_limit();

        int32_t current =
            lv_slider_get_value(
                objects.current_maximum
            );

        if (value != current)
        {
            tick_value_change_obj =
                objects.current_maximum;

            lv_slider_set_value(
                objects.current_maximum,
                value,
                LV_ANIM_OFF
            );

            tick_value_change_obj =
                NULL;
        }
    }
}

// ==================================================
// Update Buzzer Screen
// ==================================================

static void update_buzzer_gui(void)
{
    if (!buzzer_screen_active())
    {
        return;
    }

    if (buzzer_dropdown == NULL)
    {
        buzzer_dropdown_find();
    }

    if (buzzer_dropdown == NULL)
    {
        return;
    }

    if (!buzzer_dropdown_open)
    {
        uint16_t selected =
            lv_dropdown_get_selected(
                buzzer_dropdown
            );

        uint8_t mode =
            buzzer_get_mode();

        if (selected != mode)
        {
            lv_dropdown_set_selected(
                buzzer_dropdown,
                mode
            );
        }
    }
}

// ==================================================
// LED Simple Style
// ==================================================

static void led_bisect_style(void)
{
    if (objects.led_main == NULL)
    {
        return;
    }

    lv_obj_set_style_shadow_width(
        objects.led_main,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_shadow_spread(
        objects.led_main,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_border_width(
        objects.led_main,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );
}

// ==================================================
// LED Output
// ==================================================

static void led_apply_color(uint32_t color)
{
    if (!main_screen_active())
    {
        return;
    }

    if (objects.led_main == NULL)
    {
        return;
    }

    lv_led_set_color(
        objects.led_main,
        lv_color_hex(color)
    );

    lv_led_set_brightness(
        objects.led_main,
        255
    );
}

// ==================================================
// LED OFF
// ==================================================

static void led_turn_off(void)
{
    if (!main_screen_active())
    {
        return;
    }

    if (objects.led_main == NULL)
    {
        return;
    }

    lv_led_set_brightness(
        objects.led_main,
        0
    );
}

// ==================================================
// Set Solid LED
// ==================================================

static void set_status_led(uint32_t color)
{
    led_blink_state =
        true;

    led_blink_timer =
        millis();

    led_apply_color(
        color
    );
}

// ==================================================
// Set Blinking LED
// ==================================================

static void set_status_led_blink(uint32_t color)
{
    led_blink_state =
        true;

    led_blink_timer =
        millis();

    led_apply_color(
        color
    );
}

// ==================================================
// Settings Focus
// ==================================================

static void clear_settings_highlight(void)
{
    if (objects.buzzer != NULL)
    {
        lv_obj_set_style_border_width(
            objects.buzzer,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    if (objects.touch_calibration != NULL)
    {
        lv_obj_set_style_border_width(
            objects.touch_calibration,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    if (objects.voltage_range != NULL)
    {
        lv_obj_set_style_border_width(
            objects.voltage_range,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    if (objects.exit_settings != NULL)
    {
        lv_obj_set_style_border_width(
            objects.exit_settings,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }
}

static void apply_settings_highlight(void)
{
    clear_settings_highlight();

    lv_obj_t *selected =
        NULL;

    switch (settings_selection)
    {
        case SETTINGS_OPTION_BUZZER:
            selected =
                objects.buzzer;
            break;

        case SETTINGS_OPTION_CALIBRATION:
            selected =
                objects.touch_calibration;
            break;

        case SETTINGS_OPTION_VC_RANGE:
            selected =
                objects.voltage_range;
            break;

        case SETTINGS_OPTION_BACK:
            selected =
                objects.exit_settings;
            break;

        default:
            break;
    }

    if (selected != NULL)
    {
        lv_obj_set_style_border_width(
            selected,
            2,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_border_color(
            selected,
            lv_color_hex(
                FOCUS_COLOR
            ),
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }
}

// ==================================================
// Buzzer Focus
// ==================================================

static void clear_buzzer_focus(void)
{
    buzzer_dropdown_find();

    if (buzzer_dropdown != NULL)
    {
        lv_obj_set_style_border_width(
            buzzer_dropdown,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    if (
        objects.buzzer_settings_page_back_button != NULL
    )
    {
        lv_obj_set_style_border_width(
            objects.buzzer_settings_page_back_button,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }
}

static void apply_buzzer_focus(void)
{
    if (!buzzer_screen_active())
    {
        return;
    }

    clear_buzzer_focus();

    buzzer_dropdown_find();

    if (!buzzer_focus_back)
    {
        if (buzzer_dropdown != NULL)
        {
            lv_obj_set_style_border_width(
                buzzer_dropdown,
                buzzer_dropdown_open ? 3 : 2,
                LV_PART_MAIN |
                LV_STATE_DEFAULT
            );

            lv_obj_set_style_border_color(
                buzzer_dropdown,
                lv_color_hex(
                    FOCUS_COLOR
                ),
                LV_PART_MAIN |
                LV_STATE_DEFAULT
            );
        }
    }
    else
    {
        if (
            objects.buzzer_settings_page_back_button != NULL
        )
        {
            lv_obj_set_style_border_width(
                objects.buzzer_settings_page_back_button,
                2,
                LV_PART_MAIN |
                LV_STATE_DEFAULT
            );

            lv_obj_set_style_border_color(
                objects.buzzer_settings_page_back_button,
                lv_color_hex(
                    FOCUS_COLOR
                ),
                LV_PART_MAIN |
                LV_STATE_DEFAULT
            );
        }
    }
}

// ==================================================
// V/C Focus
// ==================================================

static lv_obj_t *get_vc_back_button(void)
{
    if (
        objects.exit_from_v_c_menu_button == NULL
    )
    {
        return NULL;
    }

    return lv_obj_get_parent(
        objects.exit_from_v_c_menu_button
    );
}

static void clear_vc_focus(void)
{
    if (objects.voltage_minimum != NULL)
    {
        lv_obj_set_style_border_width(
            objects.voltage_minimum,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    if (objects.voltage_maximum != NULL)
    {
        lv_obj_set_style_border_width(
            objects.voltage_maximum,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    if (objects.current_minimum != NULL)
    {
        lv_obj_set_style_border_width(
            objects.current_minimum,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    if (objects.current_maximum != NULL)
    {
        lv_obj_set_style_border_width(
            objects.current_maximum,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    lv_obj_t *back =
        get_vc_back_button();

    if (back != NULL)
    {
        lv_obj_set_style_border_width(
            back,
            0,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }
}

static void apply_vc_focus(void)
{
    if (!vc_range_screen_active())
    {
        return;
    }

    clear_vc_focus();

    lv_obj_t *selected =
        NULL;

    switch (vc_focus)
    {
        case 0:
            selected =
                objects.voltage_minimum;
            break;

        case 1:
            selected =
                objects.voltage_maximum;
            break;

        case 2:
            selected =
                objects.current_minimum;
            break;

        case 3:
            selected =
                objects.current_maximum;
            break;

        case 4:
            selected =
                get_vc_back_button();
            break;

        default:
            break;
    }

    if (selected != NULL)
    {
        lv_obj_set_style_border_width(
            selected,
            vc_edit_mode ? 3 : 2,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_border_color(
            selected,
            lv_color_hex(
                FOCUS_COLOR
            ),
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }
}

// ==================================================
// Update Input Focus
// ==================================================

static void update_input_focus_gui(void)
{
    enum ScreensEnum current_screen =
        screen_manager_get();

    if (
        current_screen !=
        last_focus_screen
    )
    {
        clear_buzzer_focus();
        clear_vc_focus();

        if (
            current_screen ==
            SCREEN_ID_BUZZER_SETTINGS
        )
        {
            buzzer_focus_back =
                false;

            buzzer_dropdown_open =
                false;

            buzzer_dropdown_find();

            if (buzzer_dropdown != NULL)
            {
                lv_dropdown_close(
                    buzzer_dropdown
                );
            }

            apply_buzzer_focus();
        }

        if (
            current_screen ==
            SCREEN_ID_V_C_RANGE_SETTINGS
        )
        {
            vc_focus =
                0;

            vc_edit_mode =
                false;

            apply_vc_focus();
        }

        last_focus_screen =
            current_screen;

        last_buzzer_focus_state =
            -1;

        last_vc_focus_state =
            -1;

        last_vc_edit_state =
            false;
    }

    if (
        current_screen ==
        SCREEN_ID_BUZZER_SETTINGS
    )
    {
        int focus_state =
            buzzer_focus_back ? 1 : 0;

        if (
            focus_state !=
            last_buzzer_focus_state
        )
        {
            apply_buzzer_focus();

            last_buzzer_focus_state =
                focus_state;
        }

        if (buzzer_dropdown_open)
        {
            apply_buzzer_focus();
        }
    }

    if (
        current_screen ==
        SCREEN_ID_V_C_RANGE_SETTINGS
    )
    {
        if (
            vc_focus !=
            last_vc_focus_state ||
            vc_edit_mode !=
            last_vc_edit_state
        )
        {
            apply_vc_focus();

            last_vc_focus_state =
                vc_focus;

            last_vc_edit_state =
                vc_edit_mode;
        }
    }
}

// ==================================================
// Buzzer Dropdown Navigation
// ==================================================

static void buzzer_dropdown_change(
    int direction
)
{
    buzzer_dropdown_find();

    if (buzzer_dropdown == NULL)
    {
        return;
    }

    uint16_t selected =
        lv_dropdown_get_selected(
            buzzer_dropdown
        );

    if (direction > 0)
    {
        if (selected < 2)
        {
            selected++;
        }
        else
        {
            selected = 0;
        }
    }
    else
    {
        if (selected > 0)
        {
            selected--;
        }
        else
        {
            selected = 2;
        }
    }

    lv_dropdown_set_selected(
        buzzer_dropdown,
        selected
    );

    set_var_buzzer_mode(
        (int32_t)selected
    );
}

// ==================================================
// V/C Value Change
// ==================================================

static void vc_change_value(void)
{
    if (!vc_range_screen_active())
    {
        return;
    }

    // --------------------------------------------------
    // Voltage minimum
    // --------------------------------------------------

    if (vc_focus == 0)
    {
        float value =
            get_voltage_min_limit();

        value += 1.0f;

        if (
            value >
            VOLTAGE_LIMIT_MAX
        )
        {
            value =
                VOLTAGE_LIMIT_MIN;
        }

        set_voltage_min_limit(
            value
        );
    }

    // --------------------------------------------------
    // Voltage maximum
    // --------------------------------------------------

    else if (vc_focus == 1)
    {
        float value =
            get_voltage_max_limit();

        value += 1.0f;

        if (
            value >
            VOLTAGE_LIMIT_MAX
        )
        {
            value =
                VOLTAGE_LIMIT_MIN;
        }

        set_voltage_max_limit(
            value
        );
    }

    // --------------------------------------------------
    // Current minimum
    // --------------------------------------------------

    else if (vc_focus == 2)
    {
        float value =
            get_current_min_limit();

        value += 1.0f;

        if (
            value >
            CURRENT_LIMIT_MAX
        )
        {
            value =
                CURRENT_LIMIT_MIN;
        }

        set_current_min_limit(
            value
        );
    }

    // --------------------------------------------------
    // Current maximum
    // --------------------------------------------------

    else if (vc_focus == 3)
    {
        float value =
            get_current_max_limit();

        value += 1.0f;

        if (
            value >
            CURRENT_LIMIT_MAX
        )
        {
            value =
                CURRENT_LIMIT_MIN;
        }

        set_current_max_limit(
            value
        );
    }

    update_vc_range_gui();

    apply_vc_focus();
}

// ==================================================
// Handle RIGHT Button Release
// ==================================================

static void handle_right_release(void)
{
    enum ScreensEnum screen =
        screen_manager_get();

    if (
        screen ==
        SCREEN_ID_MAIN
    )
    {
        main_selection =
            MAIN_OPTION_SETTINGS;

        return;
    }

    if (
        screen ==
        SCREEN_ID_SETTINGS_PAGE
    )
    {
        settings_selection++;

        if (
            settings_selection >
            SETTINGS_OPTION_BACK
        )
        {
            settings_selection =
                SETTINGS_OPTION_BUZZER;
        }

        apply_settings_highlight();

        return;
    }

    if (
        screen ==
        SCREEN_ID_BUZZER_SETTINGS
    )
    {
        buzzer_page_handle_right();
        return;
    }

    if (
        screen ==
        SCREEN_ID_V_C_RANGE_SETTINGS
    )
    {
        vc_range_page_handle_right();
        return;
    }
}

// ==================================================
// Handle SELECT Button Release
// ==================================================

static void handle_select_release(void)
{
    enum ScreensEnum screen =
        screen_manager_get();

    if (
        screen ==
        SCREEN_ID_MAIN
    )
    {
        if (
            main_selection ==
            MAIN_OPTION_SETTINGS
        )
        {
            action_go_to_settings_page(
                NULL
            );
        }

        return;
    }

    if (
        screen ==
        SCREEN_ID_SETTINGS_PAGE
    )
    {
        switch (settings_selection)
        {
            case SETTINGS_OPTION_BUZZER:

                action_go_to_buzzer_settings(
                    NULL
                );

                break;

            case SETTINGS_OPTION_CALIBRATION:

                action_go_to_touch_calibration(
                    NULL
                );

                break;

            case SETTINGS_OPTION_VC_RANGE:

                action_go_to_v_c_range_settings(
                    NULL
                );

                break;

            case SETTINGS_OPTION_BACK:

                action_exit_to_main_page(
                    NULL
                );

                break;

            default:
                break;
        }

        return;
    }

    if (
        screen ==
        SCREEN_ID_BUZZER_SETTINGS
    )
    {
        buzzer_page_handle_select();
        return;
    }

    if (
        screen ==
        SCREEN_ID_V_C_RANGE_SETTINGS
    )
    {
        vc_range_page_handle_select();
        return;
    }
}

// ==================================================
// Button Task
// ==================================================

static void buttons_task(void)
{
#if BUTTONS_ENABLED

    bool right_state =
        digitalRead(
            BTN_RIGHT
        );

    bool select_state =
        digitalRead(
            BTN_SELECT
        );

    uint32_t now =
        millis();

    // --------------------------------------------------
    // RIGHT
    // --------------------------------------------------

    if (
        right_state !=
        right_last_state
    )
    {
        if (
            now - right_last_change >=
            BUTTON_DEBOUNCE_MS
        )
        {
            right_last_change =
                now;

            right_last_state =
                right_state;

            if (
                right_state == HIGH
            )
            {
                handle_right_release();
            }
        }
    }

    // --------------------------------------------------
    // SELECT
    // --------------------------------------------------

    if (
        select_state !=
        select_last_state
    )
    {
        if (
            now - select_last_change >=
            BUTTON_DEBOUNCE_MS
        )
        {
            select_last_change =
                now;

            select_last_state =
                select_state;

            if (
                select_state == HIGH
            )
            {
                handle_select_release();
            }
        }
    }

#else

    return;

#endif
}

// ==================================================
// UART Task
// ==================================================

static void uart_task(void)
{
    uart_receive();

    float new_voltage =
        0.0f;

    float new_current =
        0.0f;

    if (
        uart_get_values(
            &new_voltage,
            &new_current
        )
    )
    {
        system_state.voltage =
            new_voltage;

        system_state.current =
            new_current;

        system_state.data_received =
            true;

        system_state.uart_timeout =
            false;

        system_state.connection_lost =
            false;

        last_valid_uart_time =
            millis();

        valid_uart_received_once =
            true;
    }
}

// ==================================================
// Safety Task
// ==================================================

static void safety_task(void)
{
    uint32_t now =
        millis();

    // --------------------------------------------------
    // UART state
    // --------------------------------------------------

    if (!valid_uart_received_once)
    {
        system_state.data_received =
            false;

        system_state.uart_timeout =
            true;

        system_state.connection_lost =
            true;
    }
    else if (
        now - last_valid_uart_time >
        UART_TIMEOUT_MS
    )
    {
        system_state.data_received =
            false;

        system_state.uart_timeout =
            true;

        system_state.connection_lost =
            true;
    }
    else
    {
        system_state.data_received =
            true;

        system_state.uart_timeout =
            false;

        system_state.connection_lost =
            false;
    }

    // --------------------------------------------------
    // Voltage validation
    // --------------------------------------------------

    system_state.voltage_ok =
        (
            system_state.voltage >=
            get_voltage_min_limit()
        )
        &&
        (
            system_state.voltage <=
            get_voltage_max_limit()
        );

    // --------------------------------------------------
    // Current validation
    // --------------------------------------------------

    system_state.current_ok =
        (
            system_state.current >=
            get_current_min_limit()
        )
        &&
        (
            system_state.current <=
            get_current_max_limit()
        );

    // --------------------------------------------------
    // Low voltage
    // --------------------------------------------------

    system_state.low_voltage =
        (
            system_state.voltage <
            get_voltage_min_limit()
        );

    // --------------------------------------------------
    // Overall state
    // --------------------------------------------------

    system_state.system_ok =
        system_state.data_received &&
        !system_state.connection_lost &&
        system_state.voltage_ok &&
        system_state.current_ok;
}

// ==================================================
// Buzzer Task
// ==================================================

static void buzzer_task(void)
{
#if BUZZER_ENABLED

    if (
        !system_state.data_received ||
        system_state.connection_lost
    )
    {
        buzzer_stop();

        buzzer_output_state =
            false;

        buzzer_phase =
            0;

        return;
    }

    if (!system_state.low_voltage)
    {
        buzzer_stop();

        buzzer_output_state =
            false;

        buzzer_phase =
            0;

        return;
    }

    uint32_t now =
        millis();

    uint32_t interval =
        0;

    if (
        buzzer_get_mode() ==
        BUZZER_MODE_1
    )
    {
        interval =
            buzzer_output_state ?
            200 :
            700;
    }
    else if (
        buzzer_get_mode() ==
        BUZZER_MODE_2
    )
    {
        interval =
            100;
    }
    else
    {
        interval =
            500;
    }

    if (
        now - buzzer_timer <
        interval
    )
    {
        return;
    }

    buzzer_timer =
        now;

    buzzer_output_state =
        !buzzer_output_state;

    if (buzzer_output_state)
    {
        buzzer_start();
    }
    else
    {
        buzzer_stop();

        if (
            buzzer_get_mode() ==
            BUZZER_MODE_2
        )
        {
            buzzer_phase++;

            if (
                buzzer_phase >=
                2
            )
            {
                buzzer_phase =
                    0;
            }
        }
    }

#else

    return;

#endif
}

// ==================================================
// Determine Current Error
// ==================================================

static ErrorType get_error_type(void)
{
    if (!valid_uart_received_once)
    {
        return ERROR_NONE;
    }

    if (
        system_state.connection_lost ||
        system_state.uart_timeout
    )
    {
        return ERROR_CONNECTION;
    }

    if (
        system_state.voltage <
        get_voltage_min_limit()
    )
    {
        return ERROR_VOLTAGE_LOW;
    }

    if (
        system_state.voltage >
        get_voltage_max_limit()
    )
    {
        return ERROR_VOLTAGE_HIGH;
    }

    if (
        system_state.current <
        get_current_min_limit()
    )
    {
        return ERROR_CURRENT_LOW;
    }

    if (
        system_state.current >
        get_current_max_limit()
    )
    {
        return ERROR_CURRENT_HIGH;
    }

    return ERROR_NONE;
}

// ==================================================
// Public Error State
// ==================================================

ErrorType tasks_get_error_type(void)
{
    return get_error_type();
}

// ==================================================
// GUI Update
// ==================================================

static void gui_update(void)
{
    // --------------------------------------------------
    // Main screen data
    // --------------------------------------------------

    if (main_screen_active())
    {
        if (
            objects.voltage != NULL &&
            system_state.voltage !=
            gui_last_voltage
        )
        {
            char voltage_text[16];

            snprintf(
                voltage_text,
                sizeof(voltage_text),
                "%.2f",
                system_state.voltage
            );

            lv_label_set_text(
                objects.voltage,
                voltage_text
            );

            gui_last_voltage =
                system_state.voltage;
        }

        if (
            objects.current != NULL &&
            system_state.current !=
            gui_last_current
        )
        {
            char current_text[16];

            snprintf(
                current_text,
                sizeof(current_text),
                "%.2f",
                system_state.current
            );

            lv_label_set_text(
                objects.current,
                current_text
            );

            gui_last_current =
                system_state.current;
        }
    }

    // --------------------------------------------------
    // Page-specific UI
    // --------------------------------------------------

    main_page_update();
    settings_page_update();
    buzzer_page_update();
    vc_range_page_update();
}

// ==================================================
// GUI Task
// ==================================================

static void gui_task(void)
{
    static uint32_t last_gui_update =
        0;

    uint32_t now =
        millis();

    if (
        now - last_gui_update <
        20
    )
    {
        return;
    }

    last_gui_update =
        now;

    gui_update();

    update_led_state();

    // ==================================================
    // UART Transfer Animation
    // ==================================================

    bool uart_connected =
        valid_uart_received_once &&
        system_state.data_received &&
        !system_state.connection_lost &&
        !system_state.uart_timeout;

    bool animation_should_run =
        main_screen_active() &&
        uart_connected;

    if (animation_should_run)
    {
        data_transfer_animation_start();

        data_transfer_animation_update(
            now
        );
    }
    else
    {
        data_transfer_animation_stop();
    }

    // --------------------------------------------------
    // EEZ screen tick
    // --------------------------------------------------

    if (!vc_range_screen_active())
    {
        int16_t screen_index =
            (int16_t)screen_manager_get() -
            1;

        if (
            screen_index >= 0 &&
            screen_index < 4
        )
        {
            tick_screen(
                screen_index
            );
        }
    }
}

// ==================================================
// Update LED State
// ==================================================

static void update_led_state(void)
{
    if (!main_screen_active())
    {
        led_gui_initialized =
            false;

        return;
    }

    if (objects.led_main == NULL)
    {
        return;
    }

    ErrorType error =
        get_error_type();

    LedVisualState desired_state;

    if (!valid_uart_received_once)
    {
        desired_state =
            LED_VISUAL_BLUE;
    }
    else if (
        error == ERROR_CONNECTION
    )
    {
        desired_state =
            LED_VISUAL_ORANGE_BLINK;
    }
    else if (
        error == ERROR_NONE
    )
    {
        desired_state =
            LED_VISUAL_GREEN;
    }
    else
    {
        desired_state =
            LED_VISUAL_RED_BLINK;
    }

    if (
        !led_gui_initialized ||
        desired_state != led_visual_state
    )
    {
        led_visual_state =
            desired_state;

        led_gui_initialized =
            true;

        switch (desired_state)
        {
            case LED_VISUAL_BLUE:

                set_status_led(
                    LED_BLUE
                );

                break;

            case LED_VISUAL_GREEN:

                set_status_led(
                    LED_GREEN
                );

                break;

            case LED_VISUAL_ORANGE_BLINK:

                set_status_led_blink(
                    LED_ORANGE
                );

                break;

            case LED_VISUAL_RED_BLINK:

                set_status_led_blink(
                    LED_RED
                );

                break;

            default:

                set_status_led(
                    LED_BLUE
                );

                break;
        }
    }

    if (
        desired_state == LED_VISUAL_ORANGE_BLINK ||
        desired_state == LED_VISUAL_RED_BLINK
    )
    {
        uint32_t now =
            millis();

        if (
            now - led_blink_timer >=
            ERROR_TEXT_BLINK_MS
        )
        {
            led_blink_timer =
                now;

            led_blink_state =
                !led_blink_state;

            if (led_blink_state)
            {
                uint32_t color =
                    (
                        desired_state ==
                        LED_VISUAL_ORANGE_BLINK
                    )
                    ? LED_ORANGE
                    : LED_RED;

                led_apply_color(
                    color
                );
            }
            else
            {
                led_turn_off();
            }
        }
    }
}

// ==================================================
// Task System Initialization
// ==================================================

void tasks_init(void)
{
    // --------------------------------------------------
    // Hardware inputs
    // --------------------------------------------------

#if BUTTONS_ENABLED

    pinMode(
        BTN_RIGHT,
        INPUT_PULLUP
    );

    pinMode(
        BTN_SELECT,
        INPUT_PULLUP
    );

#endif

    // --------------------------------------------------
    // Buzzer
    // --------------------------------------------------

    buzzer_init_ledc();

    buzzer_stop();

    // --------------------------------------------------
    // Initial button state
    // --------------------------------------------------

#if BUTTONS_ENABLED

    right_last_state =
        digitalRead(
            BTN_RIGHT
        );

    select_last_state =
        digitalRead(
            BTN_SELECT
        );

    right_last_change =
        millis();

    select_last_change =
        millis();

#endif

    // --------------------------------------------------
    // Timers
    // --------------------------------------------------

    buzzer_timer =
        millis();

    // --------------------------------------------------
    // Buzzer dropdown
    // --------------------------------------------------

    buzzer_dropdown_find();

    if (buzzer_dropdown != NULL)
    {
        lv_dropdown_set_selected(
            buzzer_dropdown,
            buzzer_get_mode()
        );
    }

    // --------------------------------------------------
    // Status LED
    // --------------------------------------------------

    led_bisect_style();

    if (objects.led_main != NULL)
    {
        led_apply_color(
            LED_BLUE
        );
    }

    led_visual_state =
        LED_VISUAL_BLUE;

    led_gui_initialized =
        false;

    led_blink_state =
        true;

    led_blink_timer =
        millis();

    // --------------------------------------------------
    // UART Transfer Animation
    // --------------------------------------------------

    data_transfer_animation_init(
        objects.main
    );

    // --------------------------------------------------
    // Menu focus
    // --------------------------------------------------

    apply_settings_highlight();

    buzzer_focus_back =
        false;

    buzzer_dropdown_open =
        false;

    vc_focus =
        0;

    vc_edit_mode =
        false;

    // --------------------------------------------------
    // GUI caches
    // --------------------------------------------------

    gui_last_voltage_min =
        -1000.0f;

    gui_last_voltage_max =
        -1000.0f;

    gui_last_current_min =
        -1000.0f;

    gui_last_current_max =
        -1000.0f;

    gui_last_voltage =
        -1000.0f;

    gui_last_current =
        -1000.0f;

    gui_last_low_voltage =
        false;

    gui_last_connection_lost =
        false;

    gui_last_system_ok =
        false;

    last_focus_screen =
        SCREEN_ID_MAIN;

    last_buzzer_focus_state =
        -1;

    last_vc_focus_state =
        -1;

    last_vc_edit_state =
        false;
}

// ==================================================
// Task Scheduler
// ==================================================

void tasks_run(void)
{
    // --------------------------------------------------
    // Physical buttons
    // --------------------------------------------------

    buttons_task();

    // --------------------------------------------------
    // UART
    // --------------------------------------------------

    uart_task();

    // --------------------------------------------------
    // Safety validation
    // --------------------------------------------------

    safety_task();

    // --------------------------------------------------
    // Buzzer
    // --------------------------------------------------

    buzzer_task();

    // --------------------------------------------------
    // Pending screen changes
    // --------------------------------------------------

    screen_manager_process();

    update_page_process();
    // --------------------------------------------------
    // GUI
    // LED + Animation are updated here
    // --------------------------------------------------

    gui_task();
}