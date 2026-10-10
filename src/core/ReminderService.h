#pragma once
#include <stdint.h>
#include <time.h>
class CalendarStore;class TaskStore;class RTCService;class Shell;

class ReminderService {
public:
    void begin(CalendarStore &, TaskStore &, RTCService &, Shell &);
    void update();
    time_t nextAlarm() const { return nextAlarm_; }

private:
    static void snoozeCurrent(void *context);
    void setReminder(const char *title, const char *detail);
    void presentReminder();
    void snooze();
    void reschedule(time_t now);

    CalendarStore *calendar_ = nullptr;
    TaskStore *tasks_ = nullptr;
    RTCService *rtc_ = nullptr;
    Shell *shell_ = nullptr;
    uint32_t lastCheckMs_ = 0;
    uint32_t lastScheduleMs_ = 0;
    time_t lastWall_ = 0;
    uint32_t rtcRevision_ = 0;
    time_t nextAlarm_ = 0;
    time_t snoozeAt_ = 0;
    char reminderTitle_[64]{};
    char reminderDetail_[96]{};
    char snoozedTitle_[64]{},snoozedDetail_[96]{};
};
