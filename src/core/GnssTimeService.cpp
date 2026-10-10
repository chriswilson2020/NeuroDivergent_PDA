#include "GnssTimeService.h"
#include "hardware/HardwareManager.h"
#include "hardware/GnssPowerReadback.h"
#include "PowerManager.h"
#include "data/SettingsStore.h"
#include "PowerOptions.h"
#include <LilyGoLib.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <Wire.h>
#include <esp_system.h>
#include <SD.h>

void GnssTimeService::begin(HardwareManager &hardware,PowerManager &powerManager,SettingsStore &settings) {
    hardware_=&hardware;power_=&powerManager;settings_=&settings;
    // Independent task-dispatched safety lease. If the application loop stalls
    // or misses cleanup, reset rather than leave GNSS searching indefinitely.
    // Do not perform concurrent expander/I2C operations from the timer task.
    esp_timer_create_args_t lease{};lease.callback=[](void*){esp_restart();};
    lease.name="gnss-power-lease";lease.dispatch_method=ESP_TIMER_TASK;
    if(esp_timer_create(&lease,&powerLease_)!=ESP_OK)powerLease_=nullptr;
    GnssTime::History history{};Preferences p;
    if(p.begin("pda-sync-hist",true)) {
        if(p.getBytesLength("history")==sizeof(history))p.getBytes("history",&history,sizeof(history));
        p.end();
    }
    engine_.begin(settings.timeSync(),history);
}
int64_t GnssTimeService::monotonicUs()const{return esp_timer_get_time();}
bool GnssTimeService::readRtc(time_t &utc){return hardware_&&hardware_->rtc.utcNow(utc);}
bool GnssTimeService::writeRtc(time_t utc){return hardware_&&hardware_->rtc.setUtc(utc);}
bool GnssTimeService::save(const GnssTime::History &history) {
    Preferences p;if(!p.begin("pda-sync-hist",false))return false;
    const bool ok=p.putBytes("history",&history,sizeof(history))==sizeof(history);p.end();
#if POCKETPDA_GNSS_DIAGNOSTICS
    Serial.printf("[PocketPDA][gnss] result=%s drift_s=%lld elapsed_s=%lu\n",GnssTime::resultName(history.result),history.drift,
        static_cast<unsigned long>(engine_.elapsedSeconds()));
    if(hardware_&&hardware_->storage.mounted()){
        SPIBusManager::Guard guard(hardware_->spi);
        if(guard){
            SD.mkdir("/PocketPDA/logs");const char *path="/PocketPDA/logs/gnss.csv";
            File f=SD.open(path,FILE_READ);const bool rotate=f&&f.size()>=262144;f.close();
            if(rotate){SD.remove("/PocketPDA/logs/gnss.previous.csv");SD.rename(path,"/PocketPDA/logs/gnss.previous.csv");}
            f=SD.open(path,FILE_APPEND);
            if(f){if(!f.size())f.println("attempt_utc,success_utc,result,drift_known,drift_seconds,elapsed_seconds,validated_samples,ubx_frames,soc,vbat_mv");
                f.printf("%lld,%lld,%u,%u,%lld,%lu,%u,%u,%u,%u\n",history.attempted,history.successful,static_cast<unsigned>(history.result),
                    history.driftKnown,history.drift,static_cast<unsigned long>(engine_.elapsedSeconds()),engine_.samples(),engine_.frames(),hardware_->battery.percent(),hardware_->battery.voltageMv());f.close();}
        }
    }
#endif
    return ok;
}
bool GnssTimeService::power(bool enabled) {
    if(!hardware_||!(hardware_->probeMask()&HW_EXPAND_ONLINE)){
        if(enabled)startError_="GNSS power controller was not detected.";
        return !enabled;
    }
    // Own Serial1 directly: initGPS() starts LilyGoLib's probing/reader task.
    // Neither the radio's power rail nor SPI/IRQ state is touched here.
    if(!enabled) {
        if(uart_){Serial1.end();uart_=false;}
        // Avoid back-powering the switched-off receiver through UART/PPS.
        pinMode(4,INPUT);pinMode(12,INPUT);pinMode(13,INPUT);
        instance.io.digitalWrite(EXPANDS_GPS_RST,LOW);
        instance.powerControl(POWER_GPS,false);
        const auto readback=GnssPowerReadback::verify(Wire,EXPANDS_GPS_EN,false);
        const bool ok=readback==GnssPowerReadback::Result::Ok;powered_=!ok;
        if(!ok)startError_=GnssPowerReadback::error(readback);
        if(ok&&powerLease_)esp_timer_stop(powerLease_);
#if POCKETPDA_GNSS_DIAGNOSTICS
        Serial.printf("[PocketPDA][gnss] power=off verified=%u error=%s\n",ok,ok?"none":GnssPowerReadback::error(readback));
#endif
        return ok;
    }
    if(!powerLease_){startError_="GNSS safety timer is unavailable.";return false;}
    esp_timer_stop(powerLease_);
    if(esp_timer_start_once(powerLease_,uint64_t(engine_.maximumSeconds()+5)*1000000)!=ESP_OK){startError_="GNSS safety timer could not start.";return false;}
    instance.io.digitalWrite(EXPANDS_GPS_RST,HIGH);
    instance.powerControl(POWER_GPS,true);
    const auto readback=GnssPowerReadback::verify(Wire,EXPANDS_GPS_EN,true);
    if(readback!=GnssPowerReadback::Result::Ok){
        startError_=GnssPowerReadback::error(readback);
        Serial.printf("[PocketPDA][gnss] power=on failed: %s\n",startError_);
        return false;
    }
    powered_=true;baud_=38400;Serial1.setRxBufferSize(2048);Serial1.begin(baud_,SERIAL_8N1,4,12);uart_=true;
    lastPollUs_=0;
#if POCKETPDA_GNSS_DIAGNOSTICS
    Serial.println("[PocketPDA][gnss] power=on uart=38400 rx=4 tx=12");
#endif
    return true;
}
void GnssTimeService::update(bool usb) {
    usb_=usb;if(!hardware_||!power_)return;
    const bool awake=power_->state()!=PowerState::SLEEP;
    engine_.update(awake,usb,hardware_->battery.valid(),hardware_->battery.percent());
    if(!engine_.active()||!uart_)return;
    // Bounded parser work so LoRa ACKs, input and reminders keep running.
    for(unsigned i=0;i<256&&Serial1.available()&&engine_.active();++i)engine_.feed(Serial1.read());
    if(!engine_.active())return;
    // LilyGo defaults to 38400; unconfigured receivers may still use 9600.
    // Change baud only if no checksum-valid UBX response has been seen.
    if(!engine_.frames()&&engine_.elapsedSeconds()>=4&&baud_==38400) {
        baud_=9600;Serial1.updateBaudRate(baud_);lastPollUs_=0;
#if POCKETPDA_GNSS_DIAGNOSTICS
        Serial.println("[PocketPDA][gnss] probing uart=9600");
#endif
    }
    const int64_t now=esp_timer_get_time();
    if(engine_.state()!=GnssTime::State::PoweringOn&&(!lastPollUs_||now-lastPollUs_>=1000000)) {
        lastPollUs_=now;
        static constexpr uint8_t poll[]={0xb5,0x62,0x01,0x21,0,0,0x22,0x67};
        Serial1.write(poll,sizeof(poll));
    }
}
bool GnssTimeService::syncNow() {
    if(!hardware_||!power_){startError_="GNSS service is not initialized.";return false;}
    if(engine_.active()){startError_="GNSS is already running or powering down. Try CANCEL first.";return false;}
    if(usb_){startError_="Exit USB Disk Mode before starting GNSS.";return false;}
    if(power_->state()==PowerState::SLEEP){startError_="Wake the display before starting GNSS.";return false;}
    if(!hardware_->battery.valid()){startError_="Battery reading is unavailable. GNSS was not powered on.";return false;}
    if(hardware_->battery.percent()<5){startError_="Battery is below 5%. Charge before starting GNSS.";return false;}
    startError_="GNSS could not start.";
    const bool ok=engine_.start(true,true,false,true,hardware_->battery.percent());
    if(!ok&&engine_.history().result==GnssTime::Result::StorageError)startError_="Could not save GNSS attempt history to internal storage.";
    return ok;
}
void GnssTimeService::preferencesChanged(){if(settings_)engine_.configure(settings_->timeSync());}
