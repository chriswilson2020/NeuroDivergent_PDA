#pragma once
#include <lvgl.h>

namespace FormFocus {
inline void enter(lv_obj_t *first) {
    if(!first) return;
    lv_group_t *group=lv_obj_get_group(first);
    if(!group) {
        group=lv_group_get_default();
        if(!group) return;
        lv_group_add_obj(group,first);
    }
    // Do not inherit a hidden launcher's focus or the previous form's edit
    // mode. A newly built form must have a visible navigation target.
    lv_group_set_editing(group,false);
    lv_group_focus_obj(first);
}
}
