#include "TimerService.h"
#include "Deadline.h"
#include "PowerOptions.h"
#include "hardware/RTCService.h"
#include "ui/Shell.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <cstdio>
#include <cstring>

time_t TimerService::now() const { struct tm value{}; rtc_->now(value); return mktime(&value); }
void TimerService::begin(TimerStore &store, RTCService &rtc, Shell &shell) { store_=&store;rtc_=&rtc;shell_=&shell;ledger_.begin("pda-alarm-ledg");reload(); }
void TimerService::reload() {
    runtime_={}; if(store_)store_->loadRuntime(runtime_);
    tm value{};const bool valid=rtc_&&rtc_->now(value);const time_t current=valid?mktime(&value):0;
    finishMonotonicUs_=runtime_.finishAt&&valid?esp_timer_get_time()+static_cast<int64_t>(runtime_.finishAt-current)*1000000:0;
    rtcRevision_=rtc_?rtc_->revision():0; lastWall_=valid?current-1:0;
}
bool TimerService::start(const TimerPreset &preset) {
    if(!store_||!rtc_||preset.type||!preset.minutes)return false;
    runtime_={};runtime_.finishAt=now()+static_cast<time_t>(preset.minutes)*60;runtime_.effect=preset.effect;
    finishMonotonicUs_=esp_timer_get_time()+static_cast<int64_t>(preset.minutes)*60000000;
    strlcpy(runtime_.name,preset.name,sizeof(runtime_.name));return store_->saveRuntime(runtime_);
}
void TimerService::cancel() { runtime_={};finishMonotonicUs_=0;if(store_)store_->clearRuntime(); }
int32_t TimerService::remainingSeconds() const {
    if(!active())return 0;
    const int64_t us=finishMonotonicUs_-esp_timer_get_time();
    return us>0?static_cast<int32_t>((us+999999)/1000000):0;
}
time_t TimerService::nextDeadline(time_t current) const {
    time_t best=active()?current+remainingSeconds():0;
    if(store_)for(size_t i=0;i<store_->count();++i){
        const auto &p=store_->at(i);
        if(p.type==1&&p.enabled)best=Deadline::earliest(best,Deadline::daily(current,p.hour,p.minute));
    }
    return best;
}
int64_t TimerService::nextSleepDeadlineUs(time_t current,int64_t sampledUs) const {
    int64_t best=active()?finishMonotonicUs_:0;
    if(store_)for(size_t i=0;i<store_->count();++i) {
        const auto &p=store_->at(i);
        if(p.type==1&&p.enabled) {
            // Include alarms crossed since the last service pass, not tomorrow's
            // occurrence when today's deadline still needs dispatching.
            const time_t due=Deadline::daily(lastWall_,p.hour,p.minute);
            best=Deadline::earliestUs(best,Deadline::wallDeadlineUs(current,due,sampledUs));
        }
    }
    return best;
}
void TimerService::update() {
    if(!store_||!rtc_||!shell_||(lastCheckMs_&&millis()-lastCheckMs_<250))return;
    lastCheckMs_=millis();tm wall{};const bool valid=rtc_->now(wall);const time_t current=mktime(&wall);
    if(valid&&!lastWall_)lastWall_=current-1;
    if(active()&&!finishMonotonicUs_&&valid)finishMonotonicUs_=esp_timer_get_time()+static_cast<int64_t>(runtime_.finishAt-current)*1000000;
    if(rtcRevision_!=rtc_->revision()){
        // A clock edit must not shorten or extend an active countdown.
        rtcRevision_=rtc_->revision();
        if(active()&&finishMonotonicUs_&&valid){runtime_.finishAt=current+remainingSeconds();store_->saveRuntime(runtime_);}
    }
    if(active()&&finishMonotonicUs_&&esp_timer_get_time()>=finishMonotonicUs_&&shell_->notifications().canAccept()){
#if POCKETPDA_POWER_DIAGNOSTICS
        Serial.printf("[PocketPDA][deadline] countdown late_ms=%lld\n",(esp_timer_get_time()-finishMonotonicUs_)/1000);
#endif
        char title[64];snprintf(title,sizeof(title),"%s FINISHED",runtime_.name[0]?runtime_.name:"TIMER");
        const uint8_t effect=runtime_.effect;cancel();shell_->notifications().show(title,"Time is up.",nullptr,nullptr,nullptr,effect);
    }
    if(!valid)return;
    ledger_.beginPass();
    if(current<lastWall_)lastWall_=current-1;
    if(current-lastWall_>86400){
        if(!shell_->notifications().canAccept())return;
        shell_->notifications().show("CLOCK MOVED FORWARD","Daily alarms from the last 24 hours will be caught up; older occurrences are not replayed.");
        lastWall_=current-86400;
    }
    for(size_t i=0;i<store_->count();++i){
        const auto &p=store_->at(i);if(p.type!=1||!p.enabled)continue;
        // Find the next occurrence after the previous check, including midnight.
        time_t due=Deadline::daily(lastWall_,p.hour,p.minute);
        for(unsigned occurrence=0;occurrence<3&&Deadline::crossed(due,lastWall_,current);++occurrence,due=Deadline::daily(due,p.hour,p.minute)) {
        if(!Deadline::crossed(due,lastWall_,current))continue;
        if(!shell_->notifications().canAccept())return;
        if(!ledger_.claim('a',p.id,due)){
            if(!ledger_.healthy()){
                if(!ledgerWarning_)shell_->notifications().show("ALARM STORAGE ERROR","Delivery history cannot be saved. Pending alarms will be retried.");
                ledgerWarning_=true;return;
            }continue;
        }
#if POCKETPDA_POWER_DIAGNOSTICS
        Serial.printf("[PocketPDA][deadline] alarm id=%lu late_s=%lld\n",static_cast<unsigned long>(p.id),static_cast<long long>(current-due));
#endif
        char detail[64];snprintf(detail,sizeof(detail),"Daily alarm at %02u:%02u",p.hour,p.minute);
        shell_->notifications().show(p.name[0]?p.name:"ALARM",detail,nullptr,nullptr,nullptr,p.effect);
        }
    }
    lastWall_=current;
    ledgerWarning_=false;
}
