#pragma once
#include <time.h>
#include <stdint.h>

class RTCService {
public:
    bool begin(bool available);
    bool now(struct tm &value) const;
    bool set(const struct tm &value);
    bool scheduleAlarm(time_t when);
    void clearAlarm();
    bool available() const { return available_; }
    uint32_t revision() const { return revision_; }
private:
    bool available_ = false;
    uint32_t revision_ = 0;
};
