#pragma once
#include <stdint.h>
class HardwareManager;

enum class PowerState : uint8_t { ACTIVE, DIMMED, SLEEP };
struct PowerConfig { uint32_t dimAfterMs = 30000; uint32_t sleepAfterMs = 120000; uint8_t activeBrightness = 12; uint8_t dimBrightness = 2; };

class PowerManager {
public:
    void begin(HardwareManager &hardware, const PowerConfig &config = {});
    void setConfig(const PowerConfig &config);
    void update();
    PowerState state() const { return state_; }
    const PowerConfig &config() const { return config_; }
private:
    void transition(PowerState next);
    HardwareManager *hardware_ = nullptr;
    PowerConfig config_{};
    PowerState state_ = PowerState::ACTIVE;
};
