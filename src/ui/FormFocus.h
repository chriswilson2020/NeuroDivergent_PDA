#pragma once
#include <lvgl.h>

namespace FormFocus {
// Visibility includes ancestors: a valid widget behind the launcher is not
// a valid navigation target. Do not test geometry (scrolled-out list rows
// must remain reachable by the encoder).
inline bool usable(lv_obj_t *object) {
    if(!object || !lv_obj_is_valid(object)) return false;
    for(auto *p=object;p;p=lv_obj_get_parent(p))
        if(lv_obj_is_hidden(p) || lv_obj_has_state(p,LV_STATE_DISABLED)) return false;
    return true;
}
inline bool belongs(lv_obj_t *object, lv_obj_t *root) {
    if(!usable(object) || !root) return false;
    for(auto *p=object;p;p=lv_obj_get_parent(p)) if(p==root) return true;
    return false;
}
inline lv_obj_t *firstControl(lv_obj_t *root, lv_group_t *group) {
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) {
        auto *child=lv_obj_get_child(root,i);
        if(!usable(child)) continue;
        if(lv_obj_get_group(child)==group) return child;
        if(auto *target=firstControl(child,group)) return target;
    }
    return nullptr;
}
inline void enter(lv_obj_t *first) {
    if(!usable(first)) return;
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
// Repair only invalid/out-of-view focus, leaving a live text area's editing
// state untouched. Read-only/empty screens retain a root navigation target.
inline void ensure(lv_obj_t *root, lv_obj_t *preferred=nullptr, lv_group_t *group=nullptr) {
    if(!usable(root)) return;
    if(!group) group=lv_group_get_default();
    if(!group) return;
    auto *focused=lv_group_get_focused(group);
    if(!preferred && focused!=root && belongs(focused,root) && lv_obj_get_group(focused)==group) return;
    auto *first=firstControl(root,group);
    if(first && lv_obj_get_group(root)==group) lv_group_remove_obj(root);
    focused=lv_group_get_focused(group);
    if(belongs(preferred,root) && lv_obj_get_group(preferred)==group) enter(preferred);
    else if(!belongs(focused,root) || lv_obj_get_group(focused)!=group) {
        if(first) enter(first);
        else { lv_group_add_obj(group,root); enter(root); }
    }
}
inline void bindInputs(lv_group_t *group) {
    for(auto *input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input)) {
        const auto type=lv_indev_get_type(input);
        if((type==LV_INDEV_TYPE_ENCODER || type==LV_INDEV_TYPE_KEYPAD) && lv_indev_get_group(input)!=group) {
            lv_indev_reset(input,nullptr);
            lv_indev_set_group(input,group);
        }
    }
}
// Use at each rebuilt view's entry. The destructor also runs on empty-list
// early returns; inherited encoder edit mode cannot trap the new view.
class View {
public:
    explicit View(lv_obj_t *root):root_(root) {}
    ~View() {
        if(auto *group=lv_group_get_default()) lv_group_set_editing(group,false);
        ensure(root_);
    }
private:
    lv_obj_t *root_;
};
}
