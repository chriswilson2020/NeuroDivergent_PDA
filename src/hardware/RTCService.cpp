#include "RTCService.h"
#include "core/TimeBasis.h"
#include "core/EventBus.h"
#include <LilyGoLib.h>
#include <Preferences.h>
#include <Wire.h>
#include <sys/time.h>
#include <cstdio>
#include <cstring>

namespace {
void logClock(time_t epoch) {
    tm utc{},local{};gmtime_r(&epoch,&utc);localtime_r(&epoch,&local);
    char utcText[24],localText[24];
    strftime(utcText,sizeof(utcText),"%Y-%m-%d %H:%M:%S",&utc);
    strftime(localText,sizeof(localText),"%Y-%m-%d %H:%M:%S",&local);
    const char *zone=getenv("TZ");
    Serial.printf("[PocketPDA][clock] TZ=%s utc=%s local=%s dst=%d\n",zone?zone:"unset",utcText,localText,local.tm_isdst);
}
bool readRaw(tm &t) {
    // PCF85063: propagate I2C errors and check oscillator-stop, STOP and BCD.
    Wire.beginTransmission(0x51);Wire.write(uint8_t(0));
    if(Wire.endTransmission(false)!=0||Wire.requestFrom(uint8_t(0x51),uint8_t(11))!=11)return false;
    uint8_t r[11];for(auto &v:r)v=Wire.read();
    if((r[0]&0x20)||(r[4]&0x80))return false;
    auto bcd=[](uint8_t v)->int{return (v&15)>9||(v>>4)>9?-100:(v>>4)*10+(v&15);};
    t={};t.tm_sec=bcd(r[4]&0x7f);t.tm_min=bcd(r[5]&0x7f);t.tm_hour=bcd(r[6]&0x3f);
    t.tm_mday=bcd(r[7]&0x3f);t.tm_mon=bcd(r[9]&0x1f)-1;t.tm_year=bcd(r[10])+100;t.tm_isdst=0;
    return TimeBasis::valid(t);
}
bool mode(uint8_t v) {
    Preferences p;if(!p.begin("pda-clock",false))return false;
    const bool ok=p.putUChar("format",v)==1;p.end();return ok;
}
void buildTime(tm &t) {
    char monthName[4]{};int d=1,y=2026;sscanf(__DATE__,"%3s %d %d",monthName,&d,&y);
    static const char months[]="JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *m=strstr(months,monthName);t={};t.tm_year=y-1900;t.tm_mon=m?(m-months)/3:0;t.tm_mday=d;
    sscanf(__TIME__,"%d:%d:%d",&t.tm_hour,&t.tm_min,&t.tm_sec);t.tm_isdst=-1;mktime(&t);
}
}
bool RTCService::begin(bool available) {
    available_=available;if(!available_)return false;
    // Missing NVS may mean a factory image erased the marker while the
    // battery-backed RTC kept UTC. Never assume it means legacy local time.
    Preferences p;uint8_t format=255;if(p.begin("pda-clock",true)){format=p.getUChar("format",255);p.end();}
    if(format==2){utcStorage_=true;time_t epoch=0;if(!utcNow(epoch))return false;timeval system{epoch,0};settimeofday(&system,nullptr);logClock(epoch);return true;}
    // Interrupted migration: don't guess the basis or apply the offset twice.
    // A verified manual or GNSS set can recover this state.
    if(format!=0){Serial.println("[PocketPDA][clock] RTC basis unknown: set local time or complete GNSS sync; registers unchanged");return false;}
    tm legacy{};time_t target=0;
    if(!readRaw(legacy)||!TimeBasis::local(legacy,target))return false;
    if(!mode(1))return false;
    if(!setUtc(target))return false;
    Serial.println("[PocketPDA] RTC storage migrated: local civil -> UTC v2");
    return true;
}
bool RTCService::utcNow(time_t &value) const {
    tm raw{};if(!available_||!utcStorage_||!readRaw(raw))return false;
    value=TimeBasis::utc(raw);return true;
}
bool RTCService::now(tm &value) const {
    time_t epoch=0;if(!utcNow(epoch)){buildTime(value);return false;}
    localtime_r(&epoch,&value);return true;
}
bool RTCService::set(const tm &value) {
    time_t epoch=0;return TimeBasis::valid(value)&&TimeBasis::local(value,epoch)&&setUtc(epoch);
}
bool RTCService::setUtc(time_t value) {
    if(!available_)return false;
    tm target{};gmtime_r(&value,&target);if(!TimeBasis::valid(target))return false;
    tm old{};const bool oldValid=readRaw(old);const bool oldUtc=utcStorage_;
    instance.rtc.setDateTime(target);
    tm check{};
    if(!readRaw(check)||llabs(static_cast<int64_t>(TimeBasis::utc(check))-value)>1||!mode(2)) {
        if(oldValid)instance.rtc.setDateTime(old);
        utcStorage_=oldUtc;return false;
    }
    utcStorage_=true;timeval system{value,0};settimeofday(&system,nullptr);
    logClock(value);
    ++revision_;EventBus::instance().publish(SystemEvent::TimeChanged);return true;
}
void RTCService::timezoneChanged() {++revision_;EventBus::instance().publish(SystemEvent::TimeChanged);}
bool RTCService::scheduleAlarm(time_t when) {
    if(!trusted())return false;
    tm utc{};gmtime_r(&when,&utc);
    instance.rtc.disableAlarm();instance.rtc.resetAlarm();
    instance.rtc.setAlarm(utc.tm_hour,utc.tm_min,utc.tm_sec,utc.tm_mday,0xff);
    instance.rtc.enableAlarm();return true;
}
void RTCService::clearAlarm(){if(available_){instance.rtc.disableAlarm();instance.rtc.resetAlarm();}}
