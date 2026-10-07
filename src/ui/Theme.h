#pragma once
#include <lvgl.h>

namespace Theme {
inline lv_color_t color(uint32_t hex) { return lv_color_hex(hex); }
void apply();
void stylePanel(lv_obj_t *obj);
void styleButton(lv_obj_t *obj, bool accent = false);
}
