#pragma once
#include "core/App.h"
class RTCService;

class ClockApp : public App {
public:
    explicit ClockApp(RTCService &rtc) : rtc_(rtc) {}
    const char *id() const override { return "clock"; }
    const char *title() const override { return "Clock"; }
    void create(lv_obj_t *parent) override;
    void resume() override;
    void suspend() override;
    void destroy() override;
    lv_obj_t *root() const override { return root_; }
private:
    static void tick(lv_timer_t *timer);
    static void toggleClicked(lv_event_t *event);
    static void resetClicked(lv_event_t *event);
    void refresh();
    uint32_t elapsedMs() const;
    RTCService &rtc_;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *timeLabel_ = nullptr;
    lv_obj_t *dateLabel_ = nullptr;
    lv_obj_t *stopwatchLabel_ = nullptr;
    lv_obj_t *toggleLabel_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    bool running_ = false;
    uint32_t accumulatedMs_ = 0;
    uint32_t runStartedMs_ = 0;
};
