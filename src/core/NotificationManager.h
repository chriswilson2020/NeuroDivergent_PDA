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
    bool canAccept() const { return !active_ || queueCount_<kQueueCapacity; }
    const char *actionTitle() const { return actionTitle_; }
    const char *actionDetail() const { return actionDetail_; }
private:
    struct Pending {
        char title[80]{}, detail[180]{}, label[24]{};
        NotificationAction action=nullptr; void *context=nullptr;
        uint8_t effect=47; bool wake=true;
    };
    static constexpr size_t kQueueCapacity=64;
    Pending pending_[kQueueCapacity]{};
    size_t queueHead_=0, queueCount_=0;
    char actionTitle_[80]{},actionDetail_[180]{};
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
