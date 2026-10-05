#include <Arduino.h>
#include <lvgl.h>

#include "screen_manager.h"
#include "USBUpdate.h"

extern "C"
{
#include "ui/screens.h"

void debug_runtime(
    const char *tag
);
}


// ==================================================
// CURRENT SCREEN
// ==================================================

static enum ScreensEnum current_screen =
    SCREEN_ID_MAIN;

static enum ScreensEnum pending_screen =
    SCREEN_ID_MAIN;

static bool screen_change_pending =
    false;

static bool screen_reload_pending =
    false;


// ==================================================
// SCREEN NAME
// ==================================================

static const char *screen_name(
    enum ScreensEnum screen
)
{
    switch (screen)
    {
        case SCREEN_ID_MAIN:
            return "MAIN";

        case SCREEN_ID_SETTINGS_PAGE:
            return "SETTINGS";

        case SCREEN_ID_BUZZER_SETTINGS:
            return "BUZZER";

        case SCREEN_ID_V_C_RANGE_SETTINGS:
            return "V_C_RANGE";

        case SCREEN_ID_UPDATE_PAGE:
            return "UPDATE";

        default:
            return "UNKNOWN";
    }
}


// ==================================================
// SCREEN OBJECT
// ==================================================

static lv_obj_t *get_screen_object(
    enum ScreensEnum screen
)
{
    switch (screen)
    {
        case SCREEN_ID_MAIN:
            return objects.main;

        case SCREEN_ID_SETTINGS_PAGE:
            return objects.settings_page;

        case SCREEN_ID_BUZZER_SETTINGS:
            return objects.buzzer_settings;

        case SCREEN_ID_V_C_RANGE_SETTINGS:
            return objects.v_c_range_settings;

        case SCREEN_ID_UPDATE_PAGE:
            return objects.update_page;

        default:
            return NULL;
    }
}


// ==================================================
// LOAD SCREEN NOW
// ==================================================

static void load_screen_now(
    enum ScreensEnum screen
)
{
    uint32_t start =
        micros();


    lv_obj_t *screen_obj =
        get_screen_object(
            screen
        );


    Serial.printf(
        "[DBG][SCREEN] LOAD %s from %s obj=%s\n",
        screen_name(screen),
        screen_name(current_screen),
        screen_obj != NULL ? "VALID" : "NULL"
    );


    if (
        screen_obj == NULL
    )
    {
        Serial.println(
            "[DBG][SCREEN] ERROR target NULL"
        );

        return;
    }


    if (
        screen ==
        current_screen
    )
    {
        Serial.println(
            "[DBG][SCREEN] already active"
        );

        return;
    }


    lv_scr_load(
        screen_obj
    );


    current_screen =
        screen;


    uint32_t elapsed =
        micros() - start;


    Serial.printf(
        "[DBG][SCREEN] LOAD DONE %s dt=%lu us\n",
        screen_name(current_screen),
        elapsed
    );


    if (
        elapsed >
        100000U
    )
    {
        debug_runtime(
            "SLOW SCREEN LOAD"
        );
    }
}


// ==================================================
// INIT
// ==================================================

void screen_manager_init(void)
{
    current_screen =
        SCREEN_ID_MAIN;

    pending_screen =
        SCREEN_ID_MAIN;

    screen_change_pending =
        false;

    screen_reload_pending =
        false;


    lv_obj_t *main_screen =
        objects.main;


    if (
        main_screen ==
        NULL
    )
    {
        Serial.println(
            "[DBG][SCREEN] ERROR MAIN NULL"
        );

        return;
    }


    Serial.println(
        "[DBG][SCREEN] loading MAIN"
    );


    lv_scr_load(
        main_screen
    );


    Serial.println(
        "[DBG][SCREEN] MAIN loaded"
    );
}


// ==================================================
// SHOW SCREEN
// ==================================================

void screen_manager_show(
    enum ScreensEnum screen
)
{
    lv_obj_t *target =
        get_screen_object(
            screen
        );


    Serial.printf(
        "[DBG][SCREEN] REQUEST %s from %s target=%s\n",
        screen_name(screen),
        screen_name(current_screen),
        target != NULL ? "VALID" : "NULL"
    );


    if (
        target ==
        NULL
    )
    {
        Serial.println(
            "[DBG][SCREEN] REQUEST FAILED: NULL target"
        );

        return;
    }


    if (
        screen_change_pending &&
        pending_screen ==
            screen
    )
    {
        Serial.println(
            "[DBG][SCREEN] already pending"
        );

        return;
    }


    if (
        !screen_change_pending &&
        current_screen ==
            screen
    )
    {
        Serial.println(
            "[DBG][SCREEN] already current"
        );

        return;
    }


    pending_screen =
        screen;

    screen_change_pending =
        true;

    screen_reload_pending =
        false;


    Serial.printf(
        "[DBG][SCREEN] REQUEST PENDING %s\n",
        screen_name(pending_screen)
    );
}


// ==================================================
// RELOAD
// ==================================================

void screen_manager_reload(void)
{
    pending_screen =
        current_screen;

    screen_change_pending =
        true;

    screen_reload_pending =
        true;


    Serial.printf(
        "[DBG][SCREEN] RELOAD REQUEST %s\n",
        screen_name(current_screen)
    );
}


// ==================================================
// PROCESS
// ==================================================

void screen_manager_process(void)
{
    // --------------------------------------------------
    // USB update UI always runs inside LVGL task context
    // --------------------------------------------------

    usb_update_gui_update();


    // --------------------------------------------------
    // No pending screen change
    // --------------------------------------------------

    if (
        !screen_change_pending
    )
    {
        return;
    }


    enum ScreensEnum requested =
        pending_screen;

    bool reload =
        screen_reload_pending;


    screen_change_pending =
        false;

    screen_reload_pending =
        false;


    Serial.printf(
        "[DBG][SCREEN] PROCESS target=%s current=%s reload=%d\n",
        screen_name(requested),
        screen_name(current_screen),
        reload ? 1 : 0
    );


    // --------------------------------------------------
    // Reload same screen
    // --------------------------------------------------

    if (
        reload &&
        requested ==
            current_screen
    )
    {
        lv_obj_t *screen_obj =
            get_screen_object(
                requested
            );


        if (
            screen_obj ==
            NULL
        )
        {
            Serial.println(
                "[DBG][SCREEN] RELOAD FAILED NULL"
            );

            return;
        }


        lv_obj_invalidate(
            screen_obj
        );


        Serial.println(
            "[DBG][SCREEN] RELOAD DONE"
        );

        return;
    }


    // --------------------------------------------------
    // Ignore same screen
    // --------------------------------------------------

    if (
        requested ==
        current_screen
    )
    {
        Serial.println(
            "[DBG][SCREEN] PROCESS ignored same screen"
        );

        return;
    }


    // --------------------------------------------------
    // Load requested screen
    // --------------------------------------------------

    load_screen_now(
        requested
    );
}


// ==================================================
// GET CURRENT
// ==================================================

enum ScreensEnum screen_manager_get(void)
{
    return current_screen;
}


// ==================================================
// IS SCREEN
// ==================================================

bool screen_manager_is(
    enum ScreensEnum screen
)
{
    return (
        current_screen ==
        screen
    );
}