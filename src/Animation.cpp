#include "Animation.h"
#include "lvgl.h"


/* ========================================================================== */
/* WAITING DOTS                                                               */
/* ========================================================================== */


/* -------------------------------------------------------------------------- */
/* Defines                                                                    */
/* -------------------------------------------------------------------------- */

#define WAITING_DOT_SIZE            8U
#define WAITING_DOT_SPACING         6U
#define WAITING_DOT_ANIM_TIME       700U
#define WAITING_DOT_DELAY           250U


/* -------------------------------------------------------------------------- */
/* Waiting Dot Callback                                                       */
/* -------------------------------------------------------------------------- */

static void Waiting_Dot_Anim_Callback(
    void *var,
    int32_t value
)
{
    lv_obj_t *dot =
        (lv_obj_t *)var;


    if (dot == NULL)
    {
        return;
    }


    lv_obj_set_style_opa(
        dot,
        (lv_opa_t)value,
        LV_PART_MAIN
    );
}


/* -------------------------------------------------------------------------- */
/* Create Waiting Dots                                                        */
/* -------------------------------------------------------------------------- */

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


    lv_obj_t *container =
        lv_obj_create(parent);


    if (container == NULL)
    {
        return;
    }


    lv_obj_remove_style_all(
        container
    );


    lv_obj_set_size(
        container,
        (3U * WAITING_DOT_SIZE) +
        (2U * WAITING_DOT_SPACING),
        WAITING_DOT_SIZE
    );


    lv_obj_set_pos(
        container,
        x,
        y
    );


    for (
        uint8_t i = 0U;
        i < 3U;
        i++
    )
    {
        lv_obj_t *dot =
            lv_obj_create(
                container
            );


        if (dot == NULL)
        {
            continue;
        }


        lv_obj_remove_style_all(
            dot
        );


        lv_obj_set_size(
            dot,
            WAITING_DOT_SIZE,
            WAITING_DOT_SIZE
        );


        lv_obj_set_style_bg_color(
            dot,
            lv_color_hex(0xFFA500),
            LV_PART_MAIN
        );


        lv_obj_set_style_bg_opa(
            dot,
            LV_OPA_COVER,
            LV_PART_MAIN
        );


        lv_obj_set_style_radius(
            dot,
            LV_RADIUS_CIRCLE,
            LV_PART_MAIN
        );


        lv_obj_set_pos(
            dot,
            i * (
                WAITING_DOT_SIZE +
                WAITING_DOT_SPACING
            ),
            0
        );


        lv_anim_t anim;


        lv_anim_init(
            &anim
        );


        lv_anim_set_var(
            &anim,
            dot
        );


        lv_anim_set_values(
            &anim,
            40,
            255
        );


        lv_anim_set_time(
            &anim,
            WAITING_DOT_ANIM_TIME
        );


        lv_anim_set_playback_time(
            &anim,
            WAITING_DOT_ANIM_TIME
        );


        lv_anim_set_exec_cb(
            &anim,
            Waiting_Dot_Anim_Callback
        );


        lv_anim_set_repeat_count(
            &anim,
            LV_ANIM_REPEAT_INFINITE
        );


        lv_anim_set_delay(
            &anim,
            i * WAITING_DOT_DELAY
        );


        lv_anim_set_path_cb(
            &anim,
            lv_anim_path_ease_in_out
        );


        lv_anim_start(
            &anim
        );
    }
}


/* ========================================================================== */
/* DATA TRANSFER ANIMATION                                                    */
/* ========================================================================== */


/* -------------------------------------------------------------------------- */
/* Defines                                                                    */
/* -------------------------------------------------------------------------- */

/*
 * نمایشگر:
 *
 * 320 × 240
 *
 * موقعیت در گوشه بالا سمت راست
 */

#define ARROW_COLOR             0x0080FF

#define ARROW_WIDTH             22
#define ARROW_HEIGHT            24

#define TX_X                    270
#define TX_Y                    4

#define RX_X                    294
#define RX_Y                    4

#define MOVE_DISTANCE           7

#define ANIM_TIME               500

#define ARROW_LINE_WIDTH        4


/* -------------------------------------------------------------------------- */
/* Arrow Objects                                                              */
/* -------------------------------------------------------------------------- */

static lv_obj_t *tx_arrow =
    NULL;

static lv_obj_t *rx_arrow =
    NULL;


/* -------------------------------------------------------------------------- */
/* Arrow Lines                                                                */
/* -------------------------------------------------------------------------- */

static lv_obj_t *tx_line_vertical =
    NULL;

static lv_obj_t *tx_line_left =
    NULL;

static lv_obj_t *tx_line_right =
    NULL;


static lv_obj_t *rx_line_vertical =
    NULL;

static lv_obj_t *rx_line_left =
    NULL;

static lv_obj_t *rx_line_right =
    NULL;


/* -------------------------------------------------------------------------- */
/* Animation State                                                            */
/* -------------------------------------------------------------------------- */

static bool data_animation_created =
    false;

static bool data_animation_running =
    false;


/* -------------------------------------------------------------------------- */
/* Create Line                                                                */
/* -------------------------------------------------------------------------- */

static lv_obj_t *create_line(
    lv_obj_t *parent,
    lv_point_t *points,
    uint16_t point_count
)
{
    if (parent == NULL)
    {
        return NULL;
    }


    lv_obj_t *line =
        lv_line_create(
            parent
        );


    if (line == NULL)
    {
        return NULL;
    }


    lv_line_set_points(
        line,
        points,
        point_count
    );


    lv_obj_set_style_line_color(
        line,
        lv_color_hex(
            ARROW_COLOR
        ),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_line_width(
        line,
        ARROW_LINE_WIDTH,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_line_rounded(
        line,
        true,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    return line;
}


/* -------------------------------------------------------------------------- */
/* TX Animation Callback                                                      */
/* -------------------------------------------------------------------------- */

static void tx_animation_callback(
    void *var,
    int32_t value
)
{
    lv_obj_t *obj =
        (lv_obj_t *)var;


    if (obj == NULL)
    {
        return;
    }


    lv_obj_set_y(
        obj,
        (lv_coord_t)value
    );
}


/* -------------------------------------------------------------------------- */
/* RX Animation Callback                                                      */
/* -------------------------------------------------------------------------- */

static void rx_animation_callback(
    void *var,
    int32_t value
)
{
    lv_obj_t *obj =
        (lv_obj_t *)var;


    if (obj == NULL)
    {
        return;
    }


    lv_obj_set_y(
        obj,
        (lv_coord_t)value
    );
}


/* -------------------------------------------------------------------------- */
/* Create Data Transfer Objects                                               */
/* -------------------------------------------------------------------------- */

static void create_data_transfer_objects(
    lv_obj_t *parent
)
{
    if (parent == NULL)
    {
        return;
    }


    if (data_animation_created)
    {
        return;
    }


    /* ====================================================================== */
    /* TX                                                                      */
    /* ====================================================================== */

    tx_arrow =
        lv_obj_create(
            parent
        );


    if (tx_arrow == NULL)
    {
        return;
    }


    lv_obj_set_size(
        tx_arrow,
        ARROW_WIDTH,
        ARROW_HEIGHT
    );


    lv_obj_set_pos(
        tx_arrow,
        TX_X,
        TX_Y
    );


    lv_obj_set_style_bg_opa(
        tx_arrow,
        LV_OPA_TRANSP,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_border_width(
        tx_arrow,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_pad_all(
        tx_arrow,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_shadow_width(
        tx_arrow,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    static lv_point_t tx_vertical_points[] =
    {
        {11, 20},
        {11, 5}
    };


    tx_line_vertical =
        create_line(
            tx_arrow,
            tx_vertical_points,
            2
        );


    static lv_point_t tx_left_points[] =
    {
        {11, 4},
        {5, 10}
    };


    tx_line_left =
        create_line(
            tx_arrow,
            tx_left_points,
            2
        );


    static lv_point_t tx_right_points[] =
    {
        {11, 4},
        {17, 10}
    };


    tx_line_right =
        create_line(
            tx_arrow,
            tx_right_points,
            2
        );


    /* ====================================================================== */
    /* RX                                                                      */
    /* ====================================================================== */

    rx_arrow =
        lv_obj_create(
            parent
        );


    if (rx_arrow == NULL)
    {
        lv_obj_del(
            tx_arrow
        );

        tx_arrow =
            NULL;

        tx_line_vertical =
            NULL;

        tx_line_left =
            NULL;

        tx_line_right =
            NULL;

        return;
    }


    lv_obj_set_size(
        rx_arrow,
        ARROW_WIDTH,
        ARROW_HEIGHT
    );


    lv_obj_set_pos(
        rx_arrow,
        RX_X,
        RX_Y
    );


    lv_obj_set_style_bg_opa(
        rx_arrow,
        LV_OPA_TRANSP,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_border_width(
        rx_arrow,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_pad_all(
        rx_arrow,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    lv_obj_set_style_shadow_width(
        rx_arrow,
        0,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );


    static lv_point_t rx_vertical_points[] =
    {
        {11, 4},
        {11, 19}
    };


    rx_line_vertical =
        create_line(
            rx_arrow,
            rx_vertical_points,
            2
        );


    static lv_point_t rx_left_points[] =
    {
        {11, 20},
        {5, 14}
    };


    rx_line_left =
        create_line(
            rx_arrow,
            rx_left_points,
            2
        );


    static lv_point_t rx_right_points[] =
    {
        {11, 20},
        {17, 14}
    };


    rx_line_right =
        create_line(
            rx_arrow,
            rx_right_points,
            2
        );


    /*
     * هر دو فلش در ابتدا مخفی هستند.
     */

    lv_obj_add_flag(
        tx_arrow,
        LV_OBJ_FLAG_HIDDEN
    );


    lv_obj_add_flag(
        rx_arrow,
        LV_OBJ_FLAG_HIDDEN
    );


    data_animation_created =
        true;

    data_animation_running =
        false;
}


/* -------------------------------------------------------------------------- */
/* Start Data Transfer Animation                                              */
/* -------------------------------------------------------------------------- */

void data_transfer_animation_start(
    lv_obj_t *parent
)
{
    if (parent == NULL)
    {
        return;
    }


    /*
     * فقط یک بار آبجکت‌ها را ایجاد کن.
     */

    if (!data_animation_created)
    {
        create_data_transfer_objects(
            parent
        );
    }


    if (
        !data_animation_created ||
        tx_arrow == NULL ||
        rx_arrow == NULL
    )
    {
        return;
    }


    /*
     * اگر انیمیشن قبلاً در حال اجراست،
     * دوباره ایجادش نکن.
     */

    if (data_animation_running)
    {
        /*
         * مطمئن شو آبجکت‌ها مخفی نشده‌اند.
         */

        lv_obj_clear_flag(
            tx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );


        lv_obj_clear_flag(
            rx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );


        return;
    }


    /*
     * نمایش فلش‌ها
     */

    lv_obj_clear_flag(
        tx_arrow,
        LV_OBJ_FLAG_HIDDEN
    );


    lv_obj_clear_flag(
        rx_arrow,
        LV_OBJ_FLAG_HIDDEN
    );


    /*
     * حذف هر animation قبلی
     */

    lv_anim_del(
        tx_arrow,
        NULL
    );


    lv_anim_del(
        rx_arrow,
        NULL
    );


    /*
     * قرار دادن فلش در موقعیت اولیه
     */

    lv_obj_set_y(
        tx_arrow,
        TX_Y
    );


    lv_obj_set_y(
        rx_arrow,
        RX_Y
    );


    /* ====================================================================== */
    /* TX                                                                       */
    /* ====================================================================== */

    lv_anim_t tx_anim;


    lv_anim_init(
        &tx_anim
    );


    lv_anim_set_var(
        &tx_anim,
        tx_arrow
    );


    lv_anim_set_exec_cb(
        &tx_anim,
        tx_animation_callback
    );


    lv_anim_set_values(
        &tx_anim,
        TX_Y + MOVE_DISTANCE,
        TX_Y
    );


    lv_anim_set_time(
        &tx_anim,
        ANIM_TIME
    );


    lv_anim_set_playback_time(
        &tx_anim,
        ANIM_TIME
    );


    lv_anim_set_repeat_count(
        &tx_anim,
        LV_ANIM_REPEAT_INFINITE
    );


    lv_anim_set_path_cb(
        &tx_anim,
        lv_anim_path_ease_in_out
    );


    lv_anim_start(
        &tx_anim
    );


    /* ====================================================================== */
    /* RX                                                                       */
    /* ====================================================================== */

    lv_anim_t rx_anim;


    lv_anim_init(
        &rx_anim
    );


    lv_anim_set_var(
        &rx_anim,
        rx_arrow
    );


    lv_anim_set_exec_cb(
        &rx_anim,
        rx_animation_callback
    );


    lv_anim_set_values(
        &rx_anim,
        RX_Y,
        RX_Y + MOVE_DISTANCE
    );


    lv_anim_set_time(
        &rx_anim,
        ANIM_TIME
    );


    lv_anim_set_playback_time(
        &rx_anim,
        ANIM_TIME
    );


    lv_anim_set_repeat_count(
        &rx_anim,
        LV_ANIM_REPEAT_INFINITE
    );


    lv_anim_set_path_cb(
        &rx_anim,
        lv_anim_path_ease_in_out
    );


    lv_anim_set_delay(
        &rx_anim,
        250
    );


    lv_anim_start(
        &rx_anim
    );


    data_animation_running =
        true;
}


/* -------------------------------------------------------------------------- */
/* Stop Data Transfer Animation                                               */
/* -------------------------------------------------------------------------- */

void data_transfer_animation_stop(
    void
)
{
    /*
     * اگر اصلاً ساخته نشده،
     * کاری نکن.
     */

    if (!data_animation_created)
    {
        return;
    }


    /*
     * حذف animationها
     *
     * اما آبجکت‌ها را حذف نمی‌کنیم.
     */

    if (tx_arrow != NULL)
    {
        lv_anim_del(
            tx_arrow,
            NULL
        );


        lv_obj_add_flag(
            tx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );
    }


    if (rx_arrow != NULL)
    {
        lv_anim_del(
            rx_arrow,
            NULL
        );


        lv_obj_add_flag(
            rx_arrow,
            LV_OBJ_FLAG_HIDDEN
        );
    }


    data_animation_running =
        false;
}


/* -------------------------------------------------------------------------- */
/* Optional Reset                                                             */
/* -------------------------------------------------------------------------- */

void data_transfer_animation_reset(
    void
)
{
    data_transfer_animation_stop();


    if (tx_arrow != NULL)
    {
        lv_obj_set_y(
            tx_arrow,
            TX_Y
        );
    }


    if (rx_arrow != NULL)
    {
        lv_obj_set_y(
            rx_arrow,
            RX_Y
        );
    }
}