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
    TimerService svc;svc.begin(store,rtc,shell);svc.update();
    assert(svc.nextSleepDeadlineUs(rtc.wall,fakeUs)==fakeUs); // wake early for RTC's second quantization
    assert(svc.nextSleepDeadlineUs(rtc.wall+2,fakeUs+2000000)<fakeUs+2000000); // crossed alarm cannot become tomorrow's
    advance(rtc,66);svc.update();
    assert(shell.n.titles.size()==2); // both alarms survive a late wake
    advance(rtc,1);svc.update();assert(shell.n.titles.size()==2);
    rtc.wall-=120;++rtc.rev;advance(rtc,1);svc.update();
    advance(rtc,120);svc.update();assert(shell.n.titles.size()==2);
    TimerPreset countdown;countdown.minutes=15;assert(svc.start(countdown));
    const int64_t precise=svc.nextSleepDeadlineUs(rtc.wall,fakeUs);
    fakeUs+=123456;fakeMs+=123;
    assert(svc.nextSleepDeadlineUs(rtc.wall,fakeUs)==precise); // no rounded-up countdown drift
    fakeUs-=123456;fakeMs-=123;
    advance(rtc,60);assert(svc.remainingSeconds()==840);
    rtc.wall+=3600;++rtc.rev;svc.update();assert(svc.remainingSeconds()==840);
    assert(store.persisted.finishAt==rtc.wall+840); // persisted deadline follows explicit clock edit
    advance(rtc,839);svc.update();assert(svc.active());
    advance(rtc,2);svc.update();assert(!svc.active());assert(shell.n.titles.size()==3);
    // A persisted countdown already expired while rebooting is processed immediately.
    store.persisted.finishAt=rtc.wall-5;svc.reload();svc.update();
    advance(rtc,1);svc.update();assert(!svc.active());assert(shell.n.titles.size()==4);
    // Forward RTC correction catches up independent same-minute alarms.
    const size_t before=shell.n.titles.size();
    tm wall{};localtime_r(&rtc.wall,&wall);
    for(auto &p:store.presets){p.id+=10;p.hour=(wall.tm_hour+1)%24;p.minute=0;}
    rtc.wall+=3600;++rtc.rev;advance(rtc,1);svc.update();assert(shell.n.titles.size()==before+2);
    // Backpressure does not mark an undelivered occurrence as delivered.
    for(auto &p:store.presets){p.id+=10;p.hour=(p.hour+1)%24;}
    shell.n.accepts=false;rtc.wall+=3600;++rtc.rev;advance(rtc,1);svc.update();assert(shell.n.titles.size()==before+2);
    shell.n.accepts=true;advance(rtc,1);svc.update();assert(shell.n.titles.size()==before+4);
    puts("timer service tests passed");
}
