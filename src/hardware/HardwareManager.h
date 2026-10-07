#pragma once
#include "BatteryService.h"
#include "HapticService.h"
#include "RTCService.h"
#include "SPIBusManager.h"
#include "StorageService.h"
#include <stdint.h>

class HardwareManager {
public:
    bool begin();
    void update();
    void setBrightness(uint8_t level);
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
};
