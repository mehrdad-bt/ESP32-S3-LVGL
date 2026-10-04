#include <Arduino.h>
#include <lvgl.h>

#include "Animation.h"

// ==================================================
// Waiting Dots
// ==================================================

static lv_obj_t *waiting_dots[3] =
{
    NULL,
    NULL,
    NULL
};

void Animation_CreateWaitingDots(
    lv_obj_t *parent,
    int32_t x,
    int32_t y
)
{
    if (parent == NULL)
    {
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        if (waiting_dots[i] == NULL)
        {
            waiting_dots[i] =
                lv_label_create(parent);

            lv_label_set_text(
                waiting_dots[i],
                "."
            );

            lv_obj_set_style_text_color(
                waiting_dots[i],
                lv_color_hex(0xFFFFFF),
                LV_PART_MAIN |
                LV_STATE_DEFAULT
            );

            lv_obj_set_style_text_font(
                waiting_dots[i],
                LV_FONT_DEFAULT,
                LV_PART_MAIN |
                LV_STATE_DEFAULT
            );
        }

        lv_obj_set_pos(
            waiting_dots[i],
            x + (i * 8),
            y
        );
    }
}

// ==================================================
// UART Transfer Animation
// ==================================================

#define TRANSFER_ARROW_COLOR     0x00FFFF

#define TRANSFER_ARROW_WIDTH     18
#define TRANSFER_ARROW_HEIGHT    22

#define TRANSFER_TX_X            270
#define TRANSFER_RX_X            294

#define TRANSFER_TX_BASE_Y       5
#define TRANSFER_RX_BASE_Y       5

#define TRANSFER_MOVE_PIXELS     5
#define TRANSFER_PERIOD_MS       700

// ==================================================
// Arrow Objects
// ==================================================

static lv_obj_t *tx_arrow =
    NULL;

static lv_obj_t *rx_arrow =
    NULL;

// ==================================================
// Arrow Points
// ==================================================

static lv_point_t tx_points[5] =
{
    { 9, 21 },
    { 9, 5  },
    { 3, 11 },
    { 9, 5  },
    { 15, 11 }
};

static lv_point_t rx_points[5] =
{
    { 9, 1  },
    { 9, 17 },
    { 3, 11 },
    { 9, 17 },
    { 15, 11 }
};

// ==================================================
// Runtime State
// ==================================================

static bool animation_initialized =
    false;

static bool animation_active =
    false;

static uint32_t animation_start_time =
    0;

// ==================================================
// Configure Arrow
// ==================================================

static void configure_arrow(
    lv_obj_t *arrow,
    lv_point_t *points
)
{
    if (arrow == NULL)
    {
        return;
    }

    lv_obj_set_size(
        arrow,
        TRANSFER_ARROW_WIDTH,
        TRANSFER_ARROW_HEIGHT
    );

    lv_line_set_points(
        arrow,
        points,
        5
    );

    lv_obj_set_style_line_color(
        arrow,
        lv_color_hex(
            TRANSFER_ARROW_COLOR
        ),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_line_width(
        arrow,
        3,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_set_style_line_rounded(
        arrow,
        true,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    lv_obj_clear_flag(
        arrow,
        LV_OBJ_FLAG_CLICKABLE
    );

    lv_obj_clear_flag(
        arrow,
        LV_OBJ_FLAG_SCROLLABLE
    );

    lv_obj_add_flag(
        arrow,
        LV_OBJ_FLAG_HIDDEN
    );
}

// ==================================================
// Initialization
// ==================================================

void data_transfer_animation_init(
    lv_obj_t *parent
)
{
    if (animation_initialized)
    {
        return;
    }

    if (parent == NULL)
    {
        return;
    }

    // --------------------------------------------------
    // TX Arrow
    // --------------------------------------------------

    tx_arrow =
        lv_line_create(parent);

    if (tx_arrow != NULL)
    {
        configure_arrow(
            tx_arrow,
            tx_points
        );

        lv_obj_set_pos(
            tx_arrow,
            TRANSFER_TX_X,
            TRANSFER_TX_BASE_Y
        );
    }

    // --------------------------------------------------
    // RX Arrow
    // --------------------------------------------------

    rx_arrow =
        lv_line_create(parent);

    if (rx_arrow != NULL)
    {
        configure_arrow(
            rx_arrow,
            rx_points
        );

        lv_obj_set_pos(
            rx_arrow,
            TRANSFER_RX_X,
            TRANSFER_RX_BASE_Y
        );
    }

    animation_active =
        false;

    animation_start_time =
        millis();

    animation_initialized =
        true;
}

// ==================================================
// Start
// ==================================================

void data_transfer_animation_start(void)
{
    if (!animation_initialized)
    {
        return;
    }

    if (
        tx_arrow == NULL ||
        rx_arrow == NULL
    )
    {
        return;
    }

    if (!animation_active)
    {
        animation_start_time =
            millis();
    }

    animation_active =
        true;

    lv_obj_clear_flag(
        tx_arrow,
        LV_OBJ_FLAG_HIDDEN
    );

    lv_obj_clear_flag(
        rx_arrow,
        LV_OBJ_FLAG_HIDDEN
    );
}

// ==================================================
// Stop
// ==================================================

void data_transfer_animation_stop(void)
{
    if (!animation_initialized)
    {
        return;
    }

    animation_active =
        false;

    if (tx_arrow != NULL)
    {
        lv_obj_add_flag(
            tx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );

        lv_obj_set_pos(
            tx_arrow,
            TRANSFER_TX_X,
            TRANSFER_TX_BASE_Y
        );
    }

    if (rx_arrow != NULL)
    {
        lv_obj_add_flag(
            rx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );

        lv_obj_set_pos(
            rx_arrow,
            TRANSFER_RX_X,
            TRANSFER_RX_BASE_Y
        );
    }
}

// ==================================================
// Reset
// ==================================================

void data_transfer_animation_reset(void)
{
    if (!animation_initialized)
    {
        return;
    }

    animation_active =
        false;

    animation_start_time =
        millis();

    if (tx_arrow != NULL)
    {
        lv_obj_add_flag(
            tx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );

        lv_obj_set_pos(
            tx_arrow,
            TRANSFER_TX_X,
            TRANSFER_TX_BASE_Y
        );
    }

    if (rx_arrow != NULL)
    {
        lv_obj_add_flag(
            rx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );

        lv_obj_set_pos(
            rx_arrow,
            TRANSFER_RX_X,
            TRANSFER_RX_BASE_Y
        );
    }
}

// ==================================================
// Get Animation State
// ==================================================

bool data_transfer_animation_is_active(void)
{
    return animation_active;
}

// ==================================================
// Calculate Movement
// ==================================================

static int32_t get_transfer_offset(
    uint32_t elapsed
)
{
    uint32_t phase =
        elapsed %
        TRANSFER_PERIOD_MS;

    uint32_t half_period =
        TRANSFER_PERIOD_MS /
        2;

    if (phase < half_period)
    {
        return
            (
                (int32_t)(
                    phase *
                    TRANSFER_MOVE_PIXELS
                )
                /
                (int32_t)half_period
            );
    }

    return
        (
            (int32_t)(
                (TRANSFER_PERIOD_MS - phase) *
                TRANSFER_MOVE_PIXELS
            )
            /
            (int32_t)half_period
        );
}

// ==================================================
// Update
// ==================================================

void data_transfer_animation_update(
    uint32_t now
)
{
    if (!animation_initialized)
    {
        return;
    }

    if (!animation_active)
    {
        return;
    }

    if (
        tx_arrow == NULL ||
        rx_arrow == NULL
    )
    {
        return;
    }

    uint32_t elapsed =
        now -
        animation_start_time;

    int32_t offset =
        get_transfer_offset(
            elapsed
        );

    // TX: upward

    lv_obj_set_y(
        tx_arrow,
        TRANSFER_TX_BASE_Y -
        offset
    );

    // RX: downward

    lv_obj_set_y(
        rx_arrow,
        TRANSFER_RX_BASE_Y +
        offset
    );
}