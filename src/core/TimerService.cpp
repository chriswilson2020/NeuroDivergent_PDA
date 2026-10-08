#include "TimerService.h"
#include "hardware/RTCService.h"
#include "ui/Shell.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

time_t TimerService::now() const { struct tm value{}; rtc_->now(value); return mktime(&value); }

void TimerService::begin(TimerStore &store, RTCService &rtc, Shell &shell) { store_=&store;rtc_=&rtc;shell_=&shell;reload(); }
void TimerService::reload() { runtime_={};if(store_)store_->loadRuntime(runtime_); }

bool TimerService::start(const TimerPreset &preset) {
    if(!store_||!rtc_||preset.type||!preset.minutes)return false;
    runtime_={};runtime_.finishAt=now()+static_cast<time_t>(preset.minutes)*60;runtime_.effect=preset.effect;
    strlcpy(runtime_.name,preset.name,sizeof(runtime_.name));return store_->saveRuntime(runtime_);
}

void TimerService::cancel() { runtime_={};if(store_)store_->clearRuntime(); }
int32_t TimerService::remainingSeconds() const { if(!active()||!rtc_)return 0;const time_t left=runtime_.finishAt-now();return left>0?static_cast<int32_t>(left):0; }

void TimerService::update() {
    if(!store_||!rtc_||!shell_||(lastCheckMs_&&millis()-lastCheckMs_<500))return;lastCheckMs_=millis();
    const time_t current=now();
    if(runtime_.finishAt&&current>=runtime_.finishAt){
        char title[64];snprintf(title,sizeof(title),"%s FINISHED",runtime_.name[0]?runtime_.name:"TIMER");
        const uint8_t effect=runtime_.effect;cancel();shell_->notifications().show(title,"Time is up.",nullptr,nullptr,nullptr,effect);
    }
    struct tm value{};localtime_r(&current,&value);const int64_t minuteKey=static_cast<int64_t>(current/60);
    if(minuteKey!=lastAlarmMinute_){
        for(size_t i=0;i<store_->count();++i){const auto &preset=store_->at(i);if(preset.type!=1||!preset.enabled||preset.hour!=value.tm_hour||preset.minute!=value.tm_min)continue;
            lastAlarmMinute_=minuteKey;char detail[64];snprintf(detail,sizeof(detail),"Daily alarm at %02u:%02u",preset.hour,preset.minute);
            shell_->notifications().show(preset.name[0]?preset.name:"ALARM",detail,nullptr,nullptr,nullptr,preset.effect);break;}
    }
}
