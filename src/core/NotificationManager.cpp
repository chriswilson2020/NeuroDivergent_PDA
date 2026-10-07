#include "NotificationManager.h"
#include "EventBus.h"
#include "hardware/HapticService.h"
#include "ui/Theme.h"

void NotificationManager::begin(lv_obj_t *screen, HapticService &haptic) {
    haptic_ = &haptic;
    overlay_ = lv_obj_create(screen); lv_obj_set_size(overlay_, 350, 142); lv_obj_center(overlay_); Theme::stylePanel(overlay_);
    lv_obj_set_style_border_color(overlay_, Theme::color(0xD88122), 0); lv_obj_set_style_border_width(overlay_, 3, 0);
    title_ = lv_label_create(overlay_); lv_obj_set_width(title_, LV_PCT(100)); lv_obj_set_style_text_font(title_, &lv_font_montserrat_20, 0); lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 4);
    detail_ = lv_label_create(overlay_); lv_obj_set_width(detail_, LV_PCT(100)); lv_obj_set_style_text_align(detail_, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(detail_, LV_ALIGN_CENTER, 0, -3);
    dismissButton_ = lv_button_create(overlay_); Theme::styleButton(dismissButton_, false); lv_obj_set_size(dismissButton_, 120, 35); lv_obj_align(dismissButton_, LV_ALIGN_BOTTOM_LEFT, 36, 0); lv_obj_add_event_cb(dismissButton_, dismissClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *label = lv_label_create(dismissButton_); lv_label_set_text(label, "DISMISS"); lv_obj_center(label);
    actionButton_ = lv_button_create(overlay_); Theme::styleButton(actionButton_, true); lv_obj_set_size(actionButton_, 120, 35); lv_obj_align(actionButton_, LV_ALIGN_BOTTOM_RIGHT, -36, 0); lv_obj_add_event_cb(actionButton_, actionClicked, LV_EVENT_CLICKED, this);
    actionLabel_ = lv_label_create(actionButton_); lv_label_set_text(actionLabel_, "OPEN"); lv_obj_center(actionLabel_);
    lv_obj_set_hidden(overlay_, true);
}
void NotificationManager::show(const char *title, const char *detail, const char *actionLabel, NotificationAction action, void *context) {
    lv_label_set_text(title_, title); lv_label_set_text(detail_, detail);
    action_ = action; actionContext_ = context;
    lv_obj_set_hidden(actionButton_, !action_);
    lv_obj_align(dismissButton_, action_ ? LV_ALIGN_BOTTOM_LEFT : LV_ALIGN_BOTTOM_MID, action_ ? 36 : 0, 0);
    if(actionLabel) lv_label_set_text(actionLabel_, actionLabel);
    focusBefore_ = lv_group_get_focused(lv_group_get_default());
    lv_obj_set_hidden(overlay_, false); lv_obj_move_foreground(overlay_); active_ = true;
    lv_group_focus_obj(dismissButton_);
    if (haptic_) haptic_->play(47);
    EventBus::instance().publish(SystemEvent::NotificationChanged);
}
void NotificationManager::dismiss() {
    if (!active_) return;
    lv_obj_set_hidden(overlay_, true); active_ = false;
    if (focusBefore_ && lv_obj_is_valid(focusBefore_)) lv_group_focus_obj(focusBefore_);
    focusBefore_ = nullptr;
    EventBus::instance().publish(SystemEvent::NotificationChanged);
}
void NotificationManager::dismissClicked(lv_event_t *event) { static_cast<NotificationManager *>(lv_event_get_user_data(event))->dismiss(); }
void NotificationManager::actionClicked(lv_event_t *event) { auto *self=static_cast<NotificationManager *>(lv_event_get_user_data(event));auto action=self->action_;void*context=self->actionContext_;self->dismiss();if(action)action(context); }
