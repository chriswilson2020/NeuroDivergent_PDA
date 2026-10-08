#include "BatteryService.h"
#include "core/EventBus.h"
#include <Arduino.h>
#include <LilyGoLib.h>

void BatteryService::update() {
    if (!available_) return;
    const uint32_t now = millis();
    if (!lastChargeReadMs_ || now - lastChargeReadMs_ >= 1000) {
        lastChargeReadMs_ = now;
        const bool nextCharging = instance.getChargeStatus().charging;
        if (nextCharging != charging_) {
            charging_ = nextCharging;
            EventBus::instance().publish(SystemEvent::BatteryChanged);
        }
    }
    if (lastReadMs_ && now - lastReadMs_ < 30000) return;
    lastReadMs_ = now;
    if (!instance.gauge.refresh()) return;
    const uint8_t next = constrain(instance.gauge.getStateOfCharge(), 0, 100);
    valid_ = true;
    if (next != percent_) { percent_ = next; EventBus::instance().publish(SystemEvent::BatteryChanged); }
}
