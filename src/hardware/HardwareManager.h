#pragma once
#include "BatteryService.h"
#include "HapticService.h"
#include "RTCService.h"
#include "SPIBusManager.h"
#include "StorageService.h"
#include <stdint.h>

class HardwareManager {
public:
    bool begin(bool initRadio = false);
    bool enableRadio();
    bool radioAvailable() const { return (probeMask_ & 0x00000001UL) != 0; }
    void update();
    void setBrightness(uint8_t level);
    void setDisplaySleeping(bool sleeping);
    bool shutdown();
    uint32_t probeMask() const { return probeMask_; }
    RTCService rtc;
    HapticService haptic;
    BatteryService battery;
    SPIBusManager spi;
    StorageService storage;
private:
    uint32_t probeMask_ = 0;
    uint8_t brightness_ = 12;
    uint8_t keyboardBrightness_ = 127;
    bool displaySleeping_ = false;
};
