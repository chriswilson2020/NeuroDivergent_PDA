#include "Launcher.h"
#include "Shell.h"
#include "Theme.h"

struct LauncherItem { const char *label; const char *id; };
static constexpr LauncherItem kItems[] = {
    {"TODAY", "today"}, {"CALENDAR", "calendar"}, {"TASKS", "tasks"},
    {"NOTES", "notes"}, {"CLOCK", "clock"}, {"CALCULATOR", "calculator"},
    {"FILES", "files"}, {"SETTINGS", "settings"}, {"HAPTIC TEST", "_haptic"}
};

void Launcher::create(lv_obj_t *parent, Shell &shell) {
    shell_ = &shell;
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root_, Theme::color(0xE9E4D8), 0);
    lv_obj_set_style_border_width(root_, 0, 0); lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 8, 0); lv_obj_set_style_pad_row(root_, 7, 0); lv_obj_set_style_pad_column(root_, 7, 0);
    lv_obj_set_layout(root_, LV_LAYOUT_GRID);
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(root_, cols, rows);
    for (size_t i = 0; i < sizeof(kItems) / sizeof(kItems[0]); ++i) {
        lv_obj_t *button = lv_button_create(root_); Theme::styleButton(button, i == 0);
        lv_obj_set_grid_cell(button, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
        lv_obj_add_event_cb(button, itemClicked, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(button, const_cast<LauncherItem *>(&kItems[i]));
        lv_obj_t *label = lv_label_create(button); lv_label_set_text(label, kItems[i].label); lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0); lv_obj_center(label);
    }
    hide();
}
void Launcher::itemClicked(lv_event_t *event) {
    auto *self = static_cast<Launcher *>(lv_event_get_user_data(event));
    auto *item = static_cast<LauncherItem *>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
    if (self && item) self->shell_->launcherAction(item->id, item->label);
}
void Launcher::show() { lv_obj_set_hidden(root_, false); lv_obj_move_foreground(root_); lv_group_focus_obj(lv_obj_get_child(root_, 0)); }
void Launcher::hide() { if (root_) lv_obj_set_hidden(root_, true); }
void Launcher::toggle() { visible() ? hide() : show(); }
bool Launcher::visible() const { return root_ && !lv_obj_is_hidden(root_); }
