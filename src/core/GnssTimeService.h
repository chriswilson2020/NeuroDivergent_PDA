#pragma once
#include "GnssTimeEngine.h"
#include <esp_timer.h>
class HardwareManager;class PowerManager;class SettingsStore;
class GnssTimeService:private GnssTime::Port {
public:
    GnssTimeService():engine_(*this){}
    void begin(HardwareManager &,PowerManager &,SettingsStore &);
    void update(bool usb);
    bool syncNow();
    const char *startError()const{return startError_;}
    void cancel(){engine_.cancel();}
    void preferencesChanged();
    bool active()const{return engine_.active();}
    const GnssTime::Engine &status()const{return engine_;}
private:
    int64_t monotonicUs()const override;
    bool readRtc(time_t &)override;
    bool writeRtc(time_t)override;
    bool save(const GnssTime::History &)override;
    bool power(bool)override;
    HardwareManager *hardware_=nullptr;PowerManager *power_=nullptr;SettingsStore *settings_=nullptr;
    GnssTime::Engine engine_;int64_t lastPollUs_=0;bool powered_=false;bool uart_=false;bool usb_=false;
    uint32_t baud_=38400;
    esp_timer_handle_t powerLease_=nullptr;
    const char *startError_="GNSS could not start.";
};
