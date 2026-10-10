#pragma once
#include <stdint.h>
#include <stddef.h>

namespace SleepTransition {
// Board-specific pin contract; no RTC mux changes are needed for GPIO wake.
constexpr int pins[] = {14, 6, 1, 40, 41, 7, 0};
constexpr size_t pinCount = sizeof(pins) / sizeof(pins[0]);
struct InterruptState { int type=0; bool enabled=false; bool sleepSelected=false; };

// IDF gpio_wakeup_enable overwrites the normal GPIO interrupt type. Keep the
// registered callbacks, but mask CPU delivery BEFORE installing level wake.
// Restore on success, preflight veto, partial setup failure and rejected sleep.
// Backend injection lets native tests exercise the production transaction.
template<class Backend> class WakeGuard {
public:
    explicit WakeGuard(Backend &backend) : backend_(backend) {}
    ~WakeGuard() { restore(); }
    int arm() {
        for(size_t i=0;i<pinCount;++i) {
            const int err=backend_.snapshot(pins[i],saved_[i]);
            if(err)return err;
        }
        for(size_t i=0;i<pinCount;++i) {
            touched_=i+1; // Even a failed operation must be unwound.
            const int err=backend_.mask(pins[i]);
            if(err)return err;
        }
        for(size_t i=0;i<pinCount;++i) {
            const bool high=pins[i]==14 || ((pins[i]==40||pins[i]==41)&&!backend_.level(pins[i]));
            if(high)highMask_|=1ULL<<pins[i];
            const int err=backend_.wake(pins[i],high);
            if(err)return err;
        }
        gpioArmed_=true;
        return backend_.enableGpioWake();
    }
    int timer(uint64_t us) {
        timerArmed_=true;
        return backend_.enableTimerWake(us);
    }
    uint64_t asserted() const {
        uint64_t mask=0;
        for(int pin:pins)if(bool(backend_.level(pin))==bool(highMask_&(1ULL<<pin)))mask|=1ULL<<pin;
        return mask;
    }
    int restore() {
        int error=0;
        const auto check=[&](int err){if(err&&!error)error=err;};
        if(timerArmed_){check(backend_.disableTimerWake());timerArmed_=false;}
        if(gpioArmed_){check(backend_.disableGpioWake());gpioArmed_=false;}
        // Mask all CPU interrupts until ALL normal types have been restored.
        for(size_t i=0;i<touched_;++i)check(backend_.mask(pins[i]));
        for(size_t i=0;i<touched_;++i) {
            check(backend_.unwake(pins[i]));
            check(backend_.restoreType(pins[i],saved_[i]));
        }
        // A failed type restoration must NOT re-enable a level-sensitive ISR.
        if(!error)for(size_t i=0;i<touched_;++i)if(saved_[i].enabled)check(backend_.unmask(pins[i]));
        touched_=0;
        return error;
    }
    WakeGuard(const WakeGuard&)=delete;
    WakeGuard& operator=(const WakeGuard&)=delete;
private:
    Backend &backend_;
    InterruptState saved_[pinCount]{};
    size_t touched_=0;
    uint64_t highMask_=0;
    bool gpioArmed_=false,timerArmed_=false;
};
}
