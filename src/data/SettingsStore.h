#pragma once
#include "core/PowerManager.h"
#include <stdint.h>

struct DeviceSettings {
    uint8_t brightness = 12;
    uint8_t dimBrightness = 2;
    uint16_t dimSeconds = 30;
    uint16_t sleepSeconds = 120;
};

class SettingsStore {
public:
    bool load();
    bool save(const DeviceSettings &settings);
    const DeviceSettings &value() const { return settings_; }
    DeviceSettings &value() { return settings_; }
    PowerConfig powerConfig() const;
private:
    DeviceSettings settings_{};
};
