#pragma once
#include "core/App.h"

class RTCService;
class CalendarStore;

class TransitionApp : public App {
public:
    TransitionApp(RTCService &rtc, CalendarStore &calendar) : rtc_(rtc), calendar_(calendar) {}
    const char *id() const override { return "transition"; }
    const char *title() const override { return "Transition"; }
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
    lv_obj_t *phase_ = nullptr;
    lv_obj_t *countdown_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *detail_ = nullptr;
    lv_timer_t *timer_ = nullptr;
};
