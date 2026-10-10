#pragma once
#include <stdint.h>
#include <time.h>
class HardwareManager; class PowerManager; class TimerService; class ReminderService; class MessagingService;
class SleepCoordinator {
public:
    void begin(HardwareManager &,PowerManager &,TimerService &,ReminderService &,MessagingService &);
    void idle(bool usbDiskActive);
private:
    void telemetry(time_t now);
    HardwareManager *hw_=nullptr; PowerManager *power_=nullptr;
    TimerService *timers_=nullptr; ReminderService *reminders_=nullptr; MessagingService *radio_=nullptr;
    time_t armed_=0;
    time_t wall_=0, due_=0;
    uint32_t lastScheduleMs_=0;
    uint32_t lastLogMs_=0, wakes_=0, errors_=0, lastCause_=0;
    uint64_t sleepUs_=0, awakeUs_=0, lastWakeUs_=0;
    bool experimental_=false;
    uint32_t lastSleepMs_=0,lastTickAdvanceMs_=0;
    uint64_t lastWakePins_=0;
};
