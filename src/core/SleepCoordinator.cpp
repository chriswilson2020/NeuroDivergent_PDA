#include "SleepCoordinator.h"
#include "Deadline.h"
#include "SleepTransition.h"
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
#include <soc/gpio_struct.h>
#include <lvgl.h>

namespace {
// ESP32-S3 pinned IDF has no public interrupt-type/enabled getters. Read those
// two register fields only; all writes go through IDF to keep ISR bookkeeping
// consistent. Do not detach callbacks or reinitialize the keyboard/rotary.
struct WakeBackend {
    int snapshot(int pin,SleepTransition::InterruptState &state) {
        gpio_io_config_t io{};
        const int err=gpio_get_io_config(static_cast<gpio_num_t>(pin),&io);
        if(err)return err;
        state={static_cast<int>(GPIO.pin[pin].int_type),GPIO.pin[pin].int_ena!=0,bool(io.slp_sel)};
        return ESP_OK;
    }
    int level(int pin) const { return gpio_get_level(static_cast<gpio_num_t>(pin)); }
    int mask(int pin){return gpio_intr_disable(static_cast<gpio_num_t>(pin));}
    int unmask(int pin){return gpio_intr_enable(static_cast<gpio_num_t>(pin));}
    int wake(int pin,bool high){return gpio_wakeup_enable(static_cast<gpio_num_t>(pin),high?GPIO_INTR_HIGH_LEVEL:GPIO_INTR_LOW_LEVEL);}
    int unwake(int pin){return gpio_wakeup_disable(static_cast<gpio_num_t>(pin));}
    int restoreType(int pin,const SleepTransition::InterruptState &state) {
        const auto gpio=static_cast<gpio_num_t>(pin);
        const int err=gpio_set_intr_type(gpio,static_cast<gpio_int_type_t>(state.type));
        if(err)return err;
        return state.sleepSelected?gpio_sleep_sel_en(gpio):gpio_sleep_sel_dis(gpio);
    }
    int enableGpioWake(){return esp_sleep_enable_gpio_wakeup();}
    int disableGpioWake(){const int e=esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);return e==ESP_ERR_INVALID_STATE?ESP_OK:e;}
    int enableTimerWake(uint64_t us){return esp_sleep_enable_timer_wakeup(us);}
    int disableTimerWake(){const int e=esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);return e==ESP_ERR_INVALID_STATE?ESP_OK:e;}
};
enum Block:uint32_t {
    Disabled=1,DisplayOn=2,Usb=4,Clock=8,Gauge=16,RadioBusy=32,
    InputPending=64,ReminderScan=128,SpiBusy=256,DeadlineDue=512,TransitionIrq=1024
};
constexpr uint64_t inputMask=(1ULL<<6)|(1ULL<<40)|(1ULL<<41)|(1ULL<<7)|(1ULL<<0);
}

void SleepCoordinator::begin(HardwareManager &hw,PowerManager &p,TimerService &t,ReminderService &r,MessagingService &m) {
    hw_=&hw;power_=&p;timers_=&t;reminders_=&r;radio_=&m;
    experimental_=POCKETPDA_EXPERIMENTAL_LIGHT_SLEEP;
    lastWakeUs_=esp_timer_get_time();
#if POCKETPDA_POWER_DIAGNOSTICS
    Serial.printf("[PocketPDA][power] mode=%s transition=2 radio=DIO1/14 keyboard=6 wheel=40,41,7\n",experimental_?"experimental-light-sleep":"display-only");
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
    if(POCKETPDA_EXPERIMENTAL_LIGHT_SLEEP)trace("sample");
#endif
}
void SleepCoordinator::idle(bool usbDiskActive) {
    if(!hw_||usbDiskActive){delay(5);return;}
    if(!lastScheduleMs_||millis()-lastScheduleMs_>=250){
        lastScheduleMs_=millis();struct tm wall{};hw_->rtc.now(wall);wall_=mktime(&wall);
        due_=Deadline::earliest(timers_->nextDeadline(wall_),reminders_->nextAlarm());
        if(due_!=armed_){if(due_)hw_->rtc.scheduleAlarm(due_);else hw_->rtc.clearAlarm();armed_=due_;}
    }
    telemetry(wall_);
    if(power_->state()!=PowerState::SLEEP)entryRecorded_=false;
    blocked_=doPreflightSleep(usbDiskActive);
    if(blocked_){delay(power_->state()==PowerState::SLEEP?20:5);return;}
    doLightSleep();
}

uint32_t SleepCoordinator::doPreflightSleep(bool usbDiskActive) const {
    uint32_t mask=0;
    if(!experimental_)mask|=Disabled;
    if(power_->state()!=PowerState::SLEEP)mask|=DisplayOn;
    if(usbDiskActive||hw_->battery.usbPresent()||Serial)mask|=Usb;
    if(!hw_->rtc.available())mask|=Clock;
    if(!hw_->battery.valid())mask|=Gauge;
    if(!radio_->processorSleepReady())mask|=RadioBusy;
    if(!digitalRead(KB_INT)||!digitalRead(ROTARY_C)||!digitalRead(0))mask|=InputPending;
    return mask;
}

void SleepCoordinator::doLightSleep() {
    // One attempt per loop, never Meshtastic's blocking waitEnterSleep loop.
    const int64_t sampleUs=esp_timer_get_time();
    struct tm wall{};
    if(!hw_->rtc.now(wall)){blocked_=Clock;return;}
    wall_=mktime(&wall);
    if(!reminders_->prepareSleep()){blocked_=ReminderScan;delay(1);return;}
    const int64_t dueUs=Deadline::earliestUs(timers_->nextSleepDeadlineUs(wall_,sampleUs),
        Deadline::wallDeadlineUs(wall_,reminders_->nextAlarm(),sampleUs));
    // A maintenance ceiling is NOT an application deadline. Always take the
    // earliest, subtract RTC quantization/setup time, and re-evaluate at entry.
    const int64_t housekeepingUs=sampleUs+1000000;
    uint64_t budget=Deadline::sleepBudgetUs(esp_timer_get_time(),dueUs,housekeepingUs);
    if(!budget){blocked_=DeadlineDue;delay(1);return;}
    // Clear a latched external alarm only after the organizer services have run;
    // their stored deadlines remain authoritative. Never enter with RTC INT low.
    if(!digitalRead(RTC_INT)){hw_->rtc.clearAlarm();armed_=0;blocked_=InputPending;return;}
    if(!entryRecorded_){trace("enter",budget);entryRecorded_=true;}

    WakeBackend backend;
    int err=ESP_OK;
    bool entered=false,restoreFailed=false;
    {
        SPIBusManager::Guard bus(hw_->spi,0);
        if(!bus){blocked_=SpiBusy;delay(1);return;}
        SleepTransition::WakeGuard<WakeBackend> wake(backend);
        err=wake.arm();
        // With CPU GPIO interrupts masked, DIO1 level remains the durable latch.
        // An arrival after this check still asserts the armed level wake source.
        radio_->latchProcessorWake();
        const uint64_t asserted=err==ESP_OK?wake.asserted():0;
        blocked_=doPreflightSleep(false);
        if(asserted)blocked_|=TransitionIrq;
        budget=Deadline::sleepBudgetUs(esp_timer_get_time(),dueUs,housekeepingUs);
        if(!budget)blocked_|=DeadlineDue;
        if(err==ESP_OK&&!blocked_) {
            err=wake.timer(budget);
            // The timer API/setup consumes time too; shorten once more before
            // entry. Never reuse the stale wall-clock/rounded countdown budget.
            budget=Deadline::sleepBudgetUs(esp_timer_get_time(),dueUs,housekeepingUs);
            if(!budget)blocked_|=DeadlineDue;
            else if(err==ESP_OK)err=wake.timer(budget);
            radio_->latchProcessorWake();
            if(!radio_->processorSleepReady()||wake.asserted())blocked_|=TransitionIrq;
            if(err==ESP_OK&&!blocked_) {
                const uint32_t tick=lv_tick_get();
                const int64_t start=esp_timer_get_time();
                ++attempts_;entered=true;awakeUs_+=start-lastWakeUs_;
                err=esp_light_sleep_start();
                const int64_t end=esp_timer_get_time();lastWakeUs_=end;
                lastSleepMs_=(end-start)/1000;lastTickAdvanceMs_=lv_tick_get()-tick;
                if(err==ESP_OK){sleepUs_+=end-start;++wakes_;lastCause_=esp_sleep_get_wakeup_cause();}
                // This is a sampled active mask, not an S3 latched wake-status API.
                lastWakePins_=wake.asserted();
                if(err==ESP_OK&&lastTickAdvanceMs_+10<lastSleepMs_){++errors_;experimental_=false;}
            }
        }
        // Latch BEFORE AND AFTER restoring edges. Do not clear pending packets,
        // reinitialize the radio, call radio.sleep(), or discard keyboard FIFO.
        radio_->latchProcessorWake();
        const int restoreError=wake.restore();
        if(restoreError){err=restoreError;restoreFailed=true;}
        radio_->latchProcessorWake();
    } // Release SPI before normal packet processing or any filesystem writes.
    if(err!=ESP_OK){++errors_;lastError_=err;experimental_=false;trace("error",budget);}
    if(restoreFailed) {
#if POCKETPDA_POWER_DIAGNOSTICS
        Serial.println("[PocketPDA][power] Wake interrupt restore failed; restart required. Sleep disabled.");
#endif
    }
    if(entered) {
        if((lastWakePins_&inputMask)||(lastCause_==ESP_SLEEP_WAKEUP_GPIO&&!lastWakePins_)) {
            lv_display_trigger_activity(nullptr);power_->update();
        }
        // Service RX/ACK before doing slow UI/SD work. The normal messaging
        // path remains responsible for decrypting, deduplicating and sending ACK.
        radio_->update();
        timers_->update();reminders_->update();
        if(attempts_==1||err!=ESP_OK)trace("return",budget);
    }
}

void SleepCoordinator::trace(const char *event,uint64_t plannedUs) {
#if POCKETPDA_POWER_DIAGNOSTICS || POCKETPDA_POWER_SD_LOG
    char row[256];
    snprintf(row,sizeof(row),"%lld,%llu,%s,%u,%lu,%lu,%llu,%lu,%016llX,%lu,%lu,%lu,%ld\n",
        static_cast<long long>(wall_),esp_timer_get_time()/1000ULL,event,experimental_,
        static_cast<unsigned long>(attempts_),static_cast<unsigned long>(blocked_),plannedUs,
        static_cast<unsigned long>(lastCause_),lastWakePins_,static_cast<unsigned long>(lastSleepMs_),
        static_cast<unsigned long>(lastTickAdvanceMs_),static_cast<unsigned long>(errors_),static_cast<long>(lastError_));
#if POCKETPDA_POWER_DIAGNOSTICS
    Serial.print("[PocketPDA][sleep] ");Serial.print(row);
#endif
#if POCKETPDA_POWER_SD_LOG
    if(hw_->storage.mounted()) {
        SPIBusManager::Guard guard(hw_->spi,0);if(!guard)return;
        SD.mkdir("/PocketPDA/logs");
        const char *path="/PocketPDA/logs/sleep.csv";
        File f=SD.open(path,FILE_READ);const bool rotate=f&&f.size()>=1024*1024;f.close();
        if(rotate){SD.remove("/PocketPDA/logs/sleep.previous.csv");if(!SD.rename(path,"/PocketPDA/logs/sleep.previous.csv"))return;}
        f=SD.open(path,FILE_APPEND);
        if(f){if(!f.size())f.println("epoch,uptime_ms,event,experimental,attempts,blocked_mask,planned_us,wake_cause,sampled_gpio_mask,sleep_ms,lv_tick_ms,errors,last_error");f.print(row);f.close();}
    }
#endif
#endif
}
