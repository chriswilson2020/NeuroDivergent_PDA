#pragma once
#include <lvgl.h>
extern lv_indev_t *testKeyboard,*testEncoder;
inline lv_indev_t *lv_get_keyboard_indev(){return testKeyboard;}
inline lv_indev_t *lv_get_encoder_indev(){return testEncoder;}
