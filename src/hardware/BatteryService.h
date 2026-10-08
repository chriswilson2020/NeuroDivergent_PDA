#pragma once
#include <stdint.h>

class BatteryService {
public:
    void begin(bool available) { available_ = available; }
    void update();
    uint8_t percent() const { return percent_; }
    bool charging() const { return charging_; }
    bool available() const { return available_; }
private:
    bool available_ = false;
    bool charging_ = false;
    uint8_t percent_ = 0;
    uint32_t lastReadMs_ = 0;
    uint32_t lastChargeReadMs_ = 0;
};
