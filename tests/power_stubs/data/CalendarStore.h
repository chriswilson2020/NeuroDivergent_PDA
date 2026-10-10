#pragma once
#include <vector>
#include <time.h>
#include <stdint.h>
struct CalendarEvent {
    uint32_t id=1;int16_t year=2026;uint8_t month=10,day=10;
    uint8_t startHour=9,startMinute=0,endHour=10,endMinute=0;
    uint16_t reminderMinutes=5;
    char title[40]="Lesson",location[20]="Room";
};
class CalendarStore {
public:
    std::vector<CalendarEvent> events;
    time_t next=0,lastQuery=0;bool scanOk=true;
    size_t count()const{return events.size();}
    const CalendarEvent &at(size_t i)const{return events[i];}
    bool ensureWindowForDate(int,int,int){return true;}
    bool occursOn(const CalendarEvent &e,int y,int m,int d)const{return e.year==y&&e.month==m&&e.day==d;}
    bool nextReminderAfter(time_t after,time_t &out){lastQuery=after;out=next>after?next:0;return scanOk;}
};
