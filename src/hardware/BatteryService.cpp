#include "BatteryService.h"
#include "core/EventBus.h"
#include <Arduino.h>
#include <LilyGoLib.h>

void BatteryService::update() {
    if (!available_ || (lastReadMs_ && millis() - lastReadMs_ < 30000)) return;
    lastReadMs_ = millis();
    if (!instance.gauge.refresh()) return;
    const uint8_t next = constrain(instance.gauge.getStateOfCharge(), 0, 100);
    if (next != percent_) { percent_ = next; EventBus::instance().publish(SystemEvent::BatteryChanged); }
}
