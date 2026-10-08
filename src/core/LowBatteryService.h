#pragma once
#include <stdint.h>

class BatteryService;
class Shell;

class LowBatteryService {
public:
    void begin(BatteryService &battery, Shell &shell);
    void update();

private:
    BatteryService *battery_ = nullptr;
    Shell *shell_ = nullptr;
    uint32_t lastCheckMs_ = 0;
    uint8_t lastWarnedThreshold_ = 0;
};
