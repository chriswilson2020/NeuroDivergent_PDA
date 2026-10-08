#pragma once
#include "data/TimerStore.h"
#include <stdint.h>

class RTCService;
class Shell;

class TimerService {
public:
    void begin(TimerStore &store, RTCService &rtc, Shell &shell);
    void reload();
    void update();
    bool start(const TimerPreset &preset);
    void cancel();
    bool active() const { return runtime_.finishAt > 0; }
    time_t finishAt() const { return runtime_.finishAt; }
    int32_t remainingSeconds() const;
    const char *activeName() const { return runtime_.name; }

private:
    time_t now() const;
    TimerStore *store_ = nullptr;
    RTCService *rtc_ = nullptr;
    Shell *shell_ = nullptr;
    TimerRuntime runtime_{};
    uint32_t lastCheckMs_ = 0;
    int64_t lastAlarmMinute_ = -1;
};
