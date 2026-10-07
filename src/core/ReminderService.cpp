#include "ReminderService.h"
#include "data/CalendarStore.h"
#include "data/TaskStore.h"
#include "hardware/RTCService.h"
#include "ui/Shell.h"
#include <Arduino.h>
#include <cstdio>

static time_t makeTime(int y,int mon,int day,int hour,int minute){struct tm t{};t.tm_year=y-1900;t.tm_mon=mon-1;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;t.tm_isdst=-1;return mktime(&t);}
void ReminderService::begin(CalendarStore&c,TaskStore&t,RTCService&r,Shell&s){calendar_=&c;tasks_=&t;rtc_=&r;shell_=&s;struct tm now{};r.now(now);reschedule(mktime(&now));}
void ReminderService::update(){if(!calendar_||!rtc_||(lastCheckMs_&&millis()-lastCheckMs_<5000))return;lastCheckMs_=millis();struct tm value{};rtc_->now(value);time_t now=mktime(&value);
    calendar_->ensureWindowForDate(value.tm_year+1900,value.tm_mon+1,value.tm_mday);
    for(size_t i=0;i<calendar_->count();++i){const auto&e=calendar_->at(i);if(!calendar_->occursOn(e,value.tm_year+1900,value.tm_mon+1,value.tm_mday))continue;time_t start=makeTime(value.tm_year+1900,value.tm_mon+1,value.tm_mday,e.startHour,e.startMinute);time_t trigger=start-e.reminderMinutes*60;uint64_t key=(static_cast<uint64_t>(e.id)<<32)|static_cast<uint32_t>(trigger);if(now>=trigger&&now<trigger+60&&lastTrigger_!=key){lastTrigger_=key;char title[64],detail[80];int mins=static_cast<int>((start-now+59)/60);snprintf(title,sizeof(title),"%s IN %d MIN",e.title,mins<0?0:mins);snprintf(detail,sizeof(detail),"%s\n%02u:%02u - %02u:%02u",e.location,e.startHour,e.startMinute,e.endHour,e.endMinute);shell_->notifications().show(title,detail,"OPEN",openCalendar,shell_);}}
    for(size_t i=0;i<tasks_->count();++i){const auto&t=tasks_->at(i);if(t.completed||!t.reminder||!t.dueYear)continue;time_t due=makeTime(t.dueYear,t.dueMonth,t.dueDay,9,0);uint64_t key=(1ULL<<63)|(static_cast<uint64_t>(t.id)<<32)|static_cast<uint32_t>(due);if(now>=due&&now<due+60&&lastTrigger_!=key){lastTrigger_=key;char title[64];snprintf(title,sizeof(title),"TASK DUE: %s",t.title);shell_->notifications().show(title,t.notes[0]?t.notes:"Due today","OPEN",openTasks,shell_);}}
    if(!lastScheduleMs_||millis()-lastScheduleMs_>=300000||(nextAlarm_&&now>=nextAlarm_)){lastScheduleMs_=millis();reschedule(now);}
}
void ReminderService::reschedule(time_t now){time_t best=0;calendar_->nextReminderAfter(now,best);for(size_t i=0;i<tasks_->count();++i){const auto&t=tasks_->at(i);if(t.completed||!t.reminder||!t.dueYear)continue;time_t candidate=makeTime(t.dueYear,t.dueMonth,t.dueDay,9,0);if(candidate>now&&(!best||candidate<best))best=candidate;}nextAlarm_=best;if(best)rtc_->scheduleAlarm(best);else rtc_->clearAlarm();}
void ReminderService::openCalendar(void*context){static_cast<Shell*>(context)->openApp("calendar");}void ReminderService::openTasks(void*context){static_cast<Shell*>(context)->openApp("tasks");}
