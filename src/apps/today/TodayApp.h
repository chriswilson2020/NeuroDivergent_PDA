#pragma once
#include "core/App.h"
#include <time.h>
class RTCService;
class CalendarStore;

class TodayApp : public App {
public:
    TodayApp(RTCService &rtc, CalendarStore &calendar) : rtc_(rtc), calendar_(calendar) {}
    const char *id() const override { return "today"; }
    const char *title() const override { return "Today"; }
    void create(lv_obj_t *parent) override;
    void resume() override;
    void suspend() override;
    void destroy() override;
    lv_obj_t *root() const override { return root_; }
private:
    static void timerTick(lv_timer_t *timer);
    void refresh();
    RTCService &rtc_;
    CalendarStore &calendar_;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *nowTitle_ = nullptr;
    lv_obj_t *nowRoom_ = nullptr;
    lv_obj_t *nowTime_ = nullptr;
    lv_obj_t *nextTitle_ = nullptr;
    lv_obj_t *nextDetail_ = nullptr;
    lv_obj_t *countdown_ = nullptr;
    lv_timer_t *timer_ = nullptr;
};
