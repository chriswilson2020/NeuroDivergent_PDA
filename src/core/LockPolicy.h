#pragma once
#include <stdint.h>

// Uses monotonic uptime, not wall time or display inactivity (alerts wake the
// display without being user activity). Uptime includes processor light sleep.
class LockPolicy {
public:
    void configure(bool enabled,uint16_t seconds,uint64_t now) {
        enabled_=enabled;seconds_=seconds;lastInput_=now;
        if(!enabled_)locked_=false;
    }
    bool expired(uint64_t now) const { return enabled_&&!locked_&&now>=deadline(); }
    uint64_t deadline() const { return enabled_&&!locked_?lastInput_+uint64_t(seconds_)*1000:0; }
    void activity(uint64_t now) { lastInput_=now; }
    void lock() { if(enabled_)locked_=true; }
    void unlock(uint64_t now) { locked_=false;lastInput_=now;failures_=0;retryAt_=0; }
    bool locked() const { return locked_; }
    bool canTry(uint64_t now) const { return now>=retryAt_; }
    uint32_t waitSeconds(uint64_t now) const { return now>=retryAt_?0:uint32_t((retryAt_-now+999)/1000); }
    void failed(uint64_t now) {
        if(failures_<7)++failures_;
        if(failures_>=3)retryAt_=now+(uint64_t(5000)<<(failures_-3));
    }
private:
    bool enabled_=false,locked_=false;
    uint16_t seconds_=120;
    uint64_t lastInput_=0,retryAt_=0;
    uint8_t failures_=0;
};
