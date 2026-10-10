#include "SleepCoordinator.h"
#include "Deadline.h"
#include "PowerOptions.h"
#include "PowerManager.h"
#include "TimerService.h"
#include "ReminderService.h"
#include "MessagingService.h"
#include "hardware/HardwareManager.h"
#include <Arduino.h>
#include <SD.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_timer.h>

void SleepCoordinator::begin(HardwareManager &hw,PowerManager &p,TimerService &t,ReminderService &r,MessagingService &m) {
    hw_=&hw;power_=&p;timers_=&t;reminders_=&r;radio_=&m;
    experimental_=POCKETPDA_EXPERIMENTAL_LIGHT_SLEEP;
    lastWakeUs_=esp_timer_get_time();
#if POCKETPDA_POWER_DIAGNOSTICS
    Serial.printf("[PocketPDA][power] mode=%s radio=DIO1/14 keyboard=6 wheel=40,41,7\n",experimental_?"experimental-light-sleep":"display-only");
#endif
}
void SleepCoordinator::telemetry(time_t now) {
#if POCKETPDA_POWER_DIAGNOSTICS || POCKETPDA_POWER_SD_LOG
    if(millis()-lastLogMs_<60000)return;
    lastLogMs_=millis();const auto s=radio_->stats();const auto &b=hw_->battery;
    char row[480];
    snprintf(row,sizeof(row),"%lld,%llu,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lu,%lu,%lu,%lu,%u,%u,%d,%u,%u,%u,%u\n",
        static_cast<long long>(now),esp_timer_get_time()/1000ULL,static_cast<unsigned>(power_->state()),
        power_->timeInStateMs(PowerState::ACTIVE),power_->timeInStateMs(PowerState::DIMMED),power_->timeInStateMs(PowerState::SLEEP),
        s.receiveMs,s.transmitMs,s.sleepMs,sleepUs_/1000, (awakeUs_+esp_timer_get_time()-lastWakeUs_)/1000,
        static_cast<unsigned long>(wakes_),static_cast<unsigned long>(lastCause_),static_cast<unsigned long>(s.packetsReceived),static_cast<unsigned long>(s.packetsSent),
        b.percent(),b.voltageMv(),b.currentMa(),b.usbPresent(),b.charging(),b.valid(),errors_);
#if POCKETPDA_POWER_DIAGNOSTICS
    Serial.print("[PocketPDA][power-sample] ");Serial.print(row);
    Serial.printf("[PocketPDA][wake] cause=%lu gpio_mask=%016llX sleep_ms=%lu lv_tick_advance_ms=%lu errors=%lu\n",
        static_cast<unsigned long>(lastCause_),lastWakePins_,static_cast<unsigned long>(lastSleepMs_),static_cast<unsigned long>(lastTickAdvanceMs_),static_cast<unsigned long>(errors_));
#endif
#if POCKETPDA_POWER_SD_LOG
    // One bounded append per minute; never access the card while owned by USB.
    if(hw_->storage.mounted()) {
        SPIBusManager::Guard guard(hw_->spi);
        if(!guard)return;
        SD.mkdir("/PocketPDA/logs");
        const char *path="/PocketPDA/logs/power.csv";
        File f=SD.open(path,FILE_READ);const bool rotate=f&&f.size()>=1024*1024;f.close();
        if(rotate){SD.remove("/PocketPDA/logs/power.previous.csv");if(!SD.rename(path,"/PocketPDA/logs/power.previous.csv"))return;}
        f=SD.open(path,FILE_APPEND);
        if(f){
            if(!f.size())f.println("epoch,uptime_ms,display_state,active_ms,dimmed_ms,display_off_ms,radio_rx_ms,radio_tx_ms,radio_sleep_ms,cpu_sleep_ms,cpu_awake_ms,wakes,last_wake_cause,packets_rx,packets_tx,soc,vbat_mv,ibat_ma,usb,charging,gauge_valid,sleep_errors");
            f.print(row);f.close();
        }
    }
#endif
#endif
}
void SleepCoordinator::idle(bool usbDiskActive) {
    if(!hw_||usbDiskActive){delay(5);return;}
    if(!lastScheduleMs_||millis()-lastScheduleMs_>=250){
        lastScheduleMs_=millis();struct tm wall{};hw_->rtc.now(wall);wall_=mktime(&wall);
        due_=Deadline::earliest(timers_->nextDeadline(wall_),reminders_->nextAlarm());
        if(due_!=armed_){if(due_)hw_->rtc.scheduleAlarm(due_);else hw_->rtc.clearAlarm();armed_=due_;}
    }
    const time_t now=wall_,due=due_;
    telemetry(now);
    // Keep USB enumeration and disk transfers entirely out of explicit sleep.
    if(power_->state()!=PowerState::SLEEP||!experimental_||!hw_->rtc.available()||!hw_->battery.valid()||hw_->battery.usbPresent()||Serial||!radio_->processorSleepReady()){
        delay(power_->state()==PowerState::SLEEP?20:5);return;
    }
    const uint32_t budget=Deadline::sleepBudgetMs(now,due,100);
    if(!budget)return;
    // GPIO wake works on digital GPIO40/41 as well as RTC-capable GPIO14/6.
    // Do NOT call LilyGoLib::lightSleep(): it powers down radio and keyboard.
    const int pins[]={LORA_IRQ,KB_INT,RTC_INT,ROTARY_A,ROTARY_B,ROTARY_C,0};
    uint64_t highWakeMask=0;
    esp_err_t err=ESP_OK;
    for(int pin:pins){
        const int level=digitalRead(pin);
        if((pin==LORA_IRQ&&level)||(pin==KB_INT&&!level)||(pin==RTC_INT&&!level)){err=ESP_ERR_INVALID_STATE;break;}
        const gpio_int_type_t wake=pin==LORA_IRQ?GPIO_INTR_HIGH_LEVEL:
            (pin==ROTARY_A||pin==ROTARY_B)?(level?GPIO_INTR_LOW_LEVEL:GPIO_INTR_HIGH_LEVEL):GPIO_INTR_LOW_LEVEL;
        if(wake==GPIO_INTR_HIGH_LEVEL)highWakeMask|=1ULL<<pin;
        err=gpio_wakeup_enable(static_cast<gpio_num_t>(pin),wake);if(err!=ESP_OK)break;
    }
    if(err==ESP_OK)err=esp_sleep_enable_gpio_wakeup();
    if(err==ESP_OK)err=esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(budget)*1000);
    if(err==ESP_OK&&radio_->processorSleepReady()){
        const uint32_t tick=millis();const uint64_t start=esp_timer_get_time();awakeUs_+=start-lastWakeUs_;
        err=esp_light_sleep_start();const uint64_t end=esp_timer_get_time();
        sleepUs_+=end-start;lastWakeUs_=end;++wakes_;lastCause_=esp_sleep_get_wakeup_cause();
        // S3 does not expose the GPIO wake-status API in this framework.
        // Sample still-active wake levels; short wheel pulses may already be gone.
        lastWakePins_=0;
        if(lastCause_==ESP_SLEEP_WAKEUP_GPIO)for(int pin:pins)
            if(bool(digitalRead(pin))==bool(highWakeMask&(1ULL<<pin)))lastWakePins_|=1ULL<<pin;
        lastSleepMs_=(end-start)/1000;lastTickAdvanceMs_=millis()-tick;
        // LV_Helper's tick callback uses millis(), which IDF advances over light sleep.
        // Do not call lv_tick_inc(): that would count sleeping time twice.
        if(lastTickAdvanceMs_+10<lastSleepMs_){++errors_;experimental_=false;}
    }
    for(int pin:pins)gpio_wakeup_disable(static_cast<gpio_num_t>(pin));
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if(err!=ESP_OK){
        // A pending input/IRQ is normal. API failures disable the experiment.
        if(err!=ESP_ERR_INVALID_STATE){++errors_;experimental_=false;
#if POCKETPDA_POWER_DIAGNOSTICS
            Serial.printf("[PocketPDA][power] fallback sleep_error=%d\n",err);
#endif
        }
        delay(1);
    }
}
