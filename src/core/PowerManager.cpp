#include "PowerManager.h"
#include "EventBus.h"
#include "hardware/HardwareManager.h"
#include <lvgl.h>

void PowerManager::begin(HardwareManager &hardware, const PowerConfig &config) { hardware_ = &hardware; config_ = config; state_ = PowerState::ACTIVE; hardware_->setBrightness(config_.activeBrightness); }
void PowerManager::setConfig(const PowerConfig &config) {
    config_ = config;
    if (hardware_) hardware_->setBrightness(state_ == PowerState::ACTIVE ? config_.activeBrightness : state_ == PowerState::DIMMED ? config_.dimBrightness : 0);
}
void PowerManager::update() {
    if (!hardware_) return;
    const uint32_t idle = lv_display_get_inactive_time(nullptr);
    PowerState next = idle >= config_.sleepAfterMs ? PowerState::SLEEP : idle >= config_.dimAfterMs ? PowerState::DIMMED : PowerState::ACTIVE;
    transition(next);
}
void PowerManager::transition(PowerState next) {
    if (!hardware_ || next == state_) return;
    state_ = next;
    hardware_->setBrightness(next == PowerState::ACTIVE ? config_.activeBrightness : next == PowerState::DIMMED ? config_.dimBrightness : 0);
    EventBus::instance().publish(SystemEvent::PowerStateChanged);
}
