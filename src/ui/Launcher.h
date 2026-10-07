#pragma once
#include <lvgl.h>
class Shell;

class Launcher {
public:
    void create(lv_obj_t *parent, Shell &shell);
    void show();
    void hide();
    void toggle();
    bool visible() const;
private:
    static void itemClicked(lv_event_t *event);
    lv_obj_t *root_ = nullptr;
    Shell *shell_ = nullptr;
};
