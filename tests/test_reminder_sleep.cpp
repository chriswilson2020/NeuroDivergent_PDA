#include "core/ReminderService.h"
#include "hardware/RTCService.h"
#include "data/CalendarStore.h"
#include "data/TaskStore.h"
#include "ui/Shell.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
uint32_t fakeMs=1000;
int64_t fakeUs=1000000;
int main(){
    setenv("TZ","UTC",1);tzset();RTCService rtc;tm t{};t.tm_year=126;t.tm_mon=9;t.tm_mday=10;t.tm_hour=8;t.tm_min=54;t.tm_sec=59;rtc.wall=mktime(&t);
    CalendarStore calendar;calendar.next=rtc.wall+1;calendar.events.push_back(CalendarEvent{});
    TaskStore tasks;Shell shell;ReminderService svc;svc.begin(calendar,tasks,rtc,shell);
    assert(svc.prepareSleep());assert(svc.nextAlarm()==calendar.next);
    // Fresh edits are discovered without waiting for the old five-minute cache.
    calendar.next=rtc.wall+30;assert(svc.prepareSleep());assert(svc.nextAlarm()==calendar.next);
    calendar.next=rtc.wall+1;
    // Time crosses a reminder before the next throttled update: sleep retains
    // the unprocessed deadline, rather than scanning it away into the future.
    rtc.wall+=2;assert(svc.prepareSleep());assert(svc.nextAlarm()==rtc.wall-1);
    svc.update();assert(shell.n.titles.size()==1);
    assert(svc.prepareSleep());assert(svc.nextAlarm()==0);
    tasks.tasks.push_back(TaskRecord{});
    assert(svc.prepareSleep());assert(svc.nextAlarm()==rtc.wall+299);
    tasks.tasks[0].completed=1;assert(svc.prepareSleep());assert(svc.nextAlarm()==0);
    calendar.scanOk=false;assert(!svc.prepareSleep()); // SD/checksum failure vetoes sleep
    calendar.scanOk=true;++rtc.rev;assert(!svc.prepareSleep()); // wait for clock-edit processing
    fakeMs+=1000;svc.update();assert(svc.prepareSleep());
    // Two calendar reminders plus a task at 09:00 must all dispatch.
    calendar.events[0].reminderMinutes=0;calendar.events[0].id=3;calendar.events.push_back(calendar.events[0]);calendar.events[1].id=4;
    tasks.tasks[0].completed=0;
    rtc.wall+=299;fakeMs+=299000;svc.update();assert(shell.n.titles.size()==4);
    // Re-crossing the same occurrence after a correction never replays it.
    rtc.wall-=60;++rtc.rev;fakeMs+=1000;svc.update();
    rtc.wall+=60;++rtc.rev;fakeMs+=1000;svc.update();assert(shell.n.titles.size()==4);
    ReminderService rebooted;rebooted.begin(calendar,tasks,rtc,shell);fakeMs+=1000;rebooted.update();assert(shell.n.titles.size()==4);
    // Forward correction catches an otherwise overdue calendar reminder.
    CalendarEvent future{};future.id=10;future.startHour=10;future.reminderMinutes=0;calendar.events.push_back(future);
    rtc.wall+=3600;++rtc.rev;fakeMs+=1000;svc.update();assert(shell.n.titles.size()==5);
    // Snooze is five elapsed minutes, even across a backward wall correction.
    assert(shell.n.action);shell.n.action(shell.n.context);
    fakeUs+=120000000;fakeMs+=120000;rtc.wall+=120;svc.update();assert(shell.n.titles.size()==5);
    rtc.wall-=3600;++rtc.rev;fakeMs+=1000;svc.update();assert(shell.n.titles.size()==5);
    fakeUs+=180000000;fakeMs+=180000;rtc.wall+=180;svc.update();assert(shell.n.titles.size()==6);
    puts("reminder sleep tests passed");
}
