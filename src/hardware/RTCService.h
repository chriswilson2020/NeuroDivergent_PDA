#pragma once
#include <time.h>

class RTCService {
public:
    bool begin(bool available);
    bool now(struct tm &value) const;
    bool set(const struct tm &value);
    bool scheduleAlarm(time_t when);
    void clearAlarm();
    bool available() const { return available_; }
private:
    bool available_ = false;
};
