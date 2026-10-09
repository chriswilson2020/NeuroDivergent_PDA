#pragma once
#include <lvgl.h>
class HapticService;
using NotificationAction = void (*)(void *context);

class NotificationManager {
public:
    void begin(lv_obj_t *screen, HapticService &haptic);
    void show(const char *title, const char *detail, const char *actionLabel = nullptr,
              NotificationAction action = nullptr, void *context = nullptr, uint8_t hapticEffect = 47,
              bool wakeDisplay = true);
    void dismiss();
    bool active() const { return active_; }
private:
    static void dismissClicked(lv_event_t *event);
    static void actionClicked(lv_event_t *event);
    lv_obj_t *overlay_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *detail_ = nullptr;
    lv_obj_t *dismissButton_ = nullptr;
    lv_obj_t *actionButton_ = nullptr;
    lv_obj_t *actionLabel_ = nullptr;
    lv_obj_t *focusBefore_ = nullptr;
    HapticService *haptic_ = nullptr;
    NotificationAction action_ = nullptr;
    void *actionContext_ = nullptr;
    bool active_ = false;
};
