#include "core/TimerService.h"
#include "hardware/RTCService.h"
#include "ui/Shell.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
uint32_t fakeMs=1000;int64_t fakeUs=1000000;
static void advance(RTCService &r,int seconds){r.wall+=seconds;fakeMs+=seconds*1000;fakeUs+=static_cast<int64_t>(seconds)*1000000;}
int main(){
    setenv("TZ","UTC",1);tzset();RTCService rtc;tm t{};t.tm_year=126;t.tm_mon=9;t.tm_mday=10;t.tm_hour=8;t.tm_min=59;t.tm_sec=59;rtc.wall=mktime(&t);
    TimerStore store;Shell shell;TimerPreset alarm;alarm.type=1;alarm.id=1;store.presets.push_back(alarm);alarm.id=2;store.presets.push_back(alarm);
    TimerService svc;svc.begin(store,rtc,shell);svc.update();advance(rtc,66);svc.update();
    assert(shell.n.titles.size()==2); // both alarms survive a late wake
    advance(rtc,1);svc.update();assert(shell.n.titles.size()==2);
    TimerPreset countdown;countdown.minutes=15;assert(svc.start(countdown));
    advance(rtc,60);assert(svc.remainingSeconds()==840);
    rtc.wall+=3600;++rtc.rev;svc.update();assert(svc.remainingSeconds()==840);
    assert(store.persisted.finishAt==rtc.wall+840); // persisted deadline follows explicit clock edit
    advance(rtc,839);svc.update();assert(svc.active());
    advance(rtc,2);svc.update();assert(!svc.active());assert(shell.n.titles.size()==3);
    // A persisted countdown already expired while rebooting is processed immediately.
    store.persisted.finishAt=rtc.wall-5;svc.reload();svc.update();
    advance(rtc,1);svc.update();assert(!svc.active());assert(shell.n.titles.size()==4);
    puts("timer service tests passed");
}
