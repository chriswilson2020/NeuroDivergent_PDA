#pragma once
#include "core/App.h"
#include <stdint.h>

class RTCService;
class TimerStore;
class TimerService;

class ClockApp : public App {
public:
    ClockApp(RTCService &rtc, TimerStore &store, TimerService &timers) : rtc_(rtc), store_(store), timers_(timers) {}
    const char *id() const override { return "clock"; }
    const char *title() const override { return "Timers"; }
    void create(lv_obj_t *parent) override;
    void resume() override;
    void suspend() override;
    void destroy() override;
    lv_obj_t *root() const override { return root_; }

private:
    enum class View : uint8_t { List, Editor };
    void showList();
    void showEditor(uint32_t id);
    void saveEditor();
    void refresh();
    static void tick(lv_timer_t *timer);
    static void presetClicked(lv_event_t *event);
    static void editClicked(lv_event_t *event);
    static void addClicked(lv_event_t *event);
    static void cancelClicked(lv_event_t *event);
    static void saveClicked(lv_event_t *event);
    static void backClicked(lv_event_t *event);
    static void deleteClicked(lv_event_t *event);

    RTCService &rtc_;
    TimerStore &store_;
    TimerService &timers_;
    View view_ = View::List;
    uint32_t editingId_ = 0;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *timeLabel_ = nullptr;
    lv_obj_t *activeLabel_ = nullptr;
    lv_obj_t *nameField_ = nullptr;
    lv_obj_t *valueField_ = nullptr;
    lv_obj_t *typeDropdown_ = nullptr;
    lv_obj_t *vibeDropdown_ = nullptr;
    lv_obj_t *enabledCheck_ = nullptr;
    lv_timer_t *timer_ = nullptr;
};
