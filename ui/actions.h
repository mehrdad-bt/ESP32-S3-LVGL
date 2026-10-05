#ifndef EEZ_LVGL_UI_ACTIONS_H
#define EEZ_LVGL_UI_ACTIONS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C"
{
#endif


// ==================================================
// SCREEN NAVIGATION
// ==================================================

void action_go_to_settings_page(
    lv_event_t *e
);

void action_exit_to_main_page(
    lv_event_t *e
);

void action_go_to_buzzer_settings(
    lv_event_t *e
);

void action_go_to_touch_calibration(
    lv_event_t *e
);

void action_go_to_v_c_range_settings(
    lv_event_t *e
);

void action_go_to_update_page(
    lv_event_t *e
);

void action_go_from_buzzer_settings_page_to_settings_page(
    lv_event_t *e
);

void action_exit_from_v_c_menu_to_settings(
    lv_event_t *e
);

void action_exit_from_update_page(
    lv_event_t *e
);


// ==================================================
// USB UPDATE
// ==================================================

void action_update_firmware(
    lv_event_t *e
);


// ==================================================
// VOLTAGE / CURRENT
// ==================================================

void action_voltage_min_changed(
    lv_event_t *e
);

void action_voltage_max_changed(
    lv_event_t *e
);

void action_current_min_changed(
    lv_event_t *e
);

void action_current_max_changed(
    lv_event_t *e
);


#ifdef __cplusplus
}
#endif

#endif