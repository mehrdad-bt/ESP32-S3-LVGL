#ifndef ANIMATION_H
#define ANIMATION_H

#include <lvgl.h>
#include <stdint.h>

// ==================================================
// Waiting Dots
// ==================================================

void Animation_CreateWaitingDots(
    lv_obj_t *parent,
    int32_t x,
    int32_t y
);

// ==================================================
// UART Data Transfer Animation
// ==================================================

void data_transfer_animation_init(
    lv_obj_t *parent
);

void data_transfer_animation_start(void);

void data_transfer_animation_stop(void);

void data_transfer_animation_reset(void);

void data_transfer_animation_update(
    uint32_t now
);

// ==================================================
// Animation State
// ==================================================

bool data_transfer_animation_is_active(void);

#endif