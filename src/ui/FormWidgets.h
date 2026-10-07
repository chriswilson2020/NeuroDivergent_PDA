#pragma once
#include "Theme.h"
#include <lvgl.h>

namespace FormWidgets {
inline lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int width, int height, bool accent = false) {
    lv_obj_t *obj = lv_button_create(parent); Theme::styleButton(obj, accent); lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, width, height);
    lv_obj_t *label = lv_label_create(obj); lv_label_set_text(label, text); lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0); lv_obj_center(label); return obj;
}
inline lv_obj_t *text(lv_obj_t *parent, const char *placeholder, int x, int y, int width, int height, bool oneLine = true) {
    lv_obj_t *obj = lv_textarea_create(parent); lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, width, height); lv_textarea_set_placeholder_text(obj, placeholder); lv_textarea_set_one_line(obj, oneLine); lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, 0); lv_obj_set_style_pad_all(obj, 7, 0); return obj;
}
}
