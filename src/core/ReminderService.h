#pragma once
#include <stdint.h>
#include <time.h>
class CalendarStore;class TaskStore;class RTCService;class Shell;

class ReminderService{
public:void begin(CalendarStore&,TaskStore&,RTCService&,Shell&);void update();time_t nextAlarm()const{return nextAlarm_;}
private:static void openCalendar(void*);static void openTasks(void*);void reschedule(time_t now);CalendarStore*calendar_=nullptr;TaskStore*tasks_=nullptr;RTCService*rtc_=nullptr;Shell*shell_=nullptr;uint32_t lastCheckMs_=0;uint32_t lastScheduleMs_=0;uint64_t lastTrigger_=0;time_t nextAlarm_=0;};
