#pragma once
#include <stdint.h>

bool     usb_msc_start(void);       // call once from setup()
bool     usb_msc_mounted(void);     // true while a flash drive is mounted on /usb
uint32_t usb_msc_generation(void);  // increments on every mount / unmount