#include "Theme.h"

namespace Theme {
void apply() {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, color(0xE9E4D8), 0);
    lv_obj_set_style_text_color(screen, color(0x172128), 0);
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
}
void stylePanel(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, color(0xF8F5EC), 0);
    lv_obj_set_style_border_color(obj, color(0xB9B2A4), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 6, 0);
    lv_obj_set_style_pad_all(obj, 10, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}
void styleButton(lv_obj_t *obj, bool accent) {
    lv_obj_set_style_bg_color(obj, color(accent ? 0x1E6675 : 0xF8F5EC), 0);
    lv_obj_set_style_text_color(obj, color(accent ? 0xFFFFFF : 0x172128), 0);
    lv_obj_set_style_border_color(obj, color(accent ? 0x124C58 : 0xAAA294), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 5, 0);
    lv_obj_set_style_outline_color(obj, color(0xE59C35), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(obj, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(obj, color(0xD88122), LV_STATE_PRESSED);
}
}
