#include "hardware/RTCService.h"
#include "core/TimeBasis.h"
#include "core/EventBus.h"
#include <LilyGoLib.h>
#include <Preferences.h>
#include <sys/time.h>
#include <cassert>
#include <cstdio>
// Production RTC code is linked with -Dsettimeofday=pocketpdaTestSettimeofday;
// never change the host computer's clock during these tests.
extern "C" int pocketpdaTestSettimeofday(const timeval *,const struct timezone *){return 0;}
static unsigned changes=0;
static void changed(SystemEvent e,void*){if(e==SystemEvent::TimeChanged)++changes;}
static tm civil(){tm t{};t.tm_year=126;t.tm_mon=9;t.tm_mday=10;t.tm_hour=14;t.tm_min=30;t.tm_isdst=-1;return t;}
int main(){
    TimeBasis::configure();EventBus::instance().subscribe(changed,nullptr);
    Wire.set(civil());RTCService unknown;
    assert(!unknown.begin(true)&&!unknown.trusted()&&instance.rtc.writes==0);
    // Only explicitly labelled legacy clocks may be migrated automatically.
    Preferences marker;marker.begin("pda-clock",false);marker.putUChar("format",0);marker.end();
    RTCService first;assert(first.begin(true));
    time_t utc=0;assert(first.utcNow(utc));assert(utc==TimeBasis::utc(civil())-7200);
    tm local{};assert(first.now(local));assert(TimeBasis::sameCivil(civil(),local));
    const unsigned writes=instance.rtc.writes;RTCService reboot;assert(reboot.begin(true));assert(instance.rtc.writes==writes);
    assert(reboot.utcNow(utc));assert(utc==TimeBasis::utc(civil())-7200);
    // Reproduce a factory flash erasing NVS but not the battery-backed RTC.
    Preferences::values.erase("pda-clock/format");const unsigned beforeEraseBoot=instance.rtc.writes;
    RTCService erased;assert(!erased.begin(true)&&!erased.trusted());
    assert(instance.rtc.writes==beforeEraseBoot); // Must NOT subtract two hours again.
    assert(erased.set(civil()));assert(erased.utcNow(utc)&&utc==TimeBasis::utc(civil())-7200);
    assert(reboot.scheduleAlarm(utc));assert(instance.rtc.hour==12&&instance.rtc.minute==30);
    tm invalid=civil();invalid.tm_mon=1;invalid.tm_mday=30;assert(!reboot.set(invalid));
    invalid=civil();invalid.tm_mon=2;invalid.tm_mday=29;invalid.tm_hour=2;assert(!reboot.set(invalid));
    Wire.writeOk=false;assert(!reboot.setUtc(utc+10));time_t after=0;assert(reboot.utcNow(after)&&after==utc);Wire.writeOk=true;
    Wire.readOk=false;assert(!reboot.now(local));Wire.readOk=true;
    Wire.regs[4]|=0x80;assert(!reboot.trusted());assert(reboot.setUtc(utc));assert(reboot.trusted());
    Preferences p;p.begin("pda-clock",false);p.putUChar("format",1);p.end();
    RTCService interrupted;assert(!interrupted.begin(true));assert(!interrupted.trusted());assert(interrupted.setUtc(utc));
    TimeBasis::configure(1);assert(interrupted.now(local)&&local.tm_hour==12);
    assert(changes>=3);
    puts("RTC migration, UTC/local conversion and readback tests passed");
}
