#include "LowBatteryService.h"
#include "hardware/BatteryService.h"
#include "ui/Shell.h"
#include <Arduino.h>
#include <cstdio>

void LowBatteryService::begin(BatteryService &battery, Shell &shell) {
    battery_ = &battery;
    shell_ = &shell;
}

void LowBatteryService::update() {
    if (!battery_ || !shell_ || (lastCheckMs_ && millis() - lastCheckMs_ < 2000)) return;
    lastCheckMs_ = millis();
    if (!battery_->available() || !battery_->valid()) return;

    const uint8_t percent = battery_->percent();
    if (battery_->charging() || percent > 20) {
        lastWarnedThreshold_ = 0;
        return;
    }

    const uint8_t threshold = percent <= 5 ? 5 : percent <= 10 ? 10 : 20;
    if (lastWarnedThreshold_ && threshold >= lastWarnedThreshold_) return;
    if (shell_->notifications().active()) return;

    char title[32];
    char detail[72];
    snprintf(title, sizeof(title), "LOW BATTERY: %u%%", percent);
    if (threshold == 5) snprintf(detail, sizeof(detail), "Charge now to avoid losing power.");
    else if (threshold == 10) snprintf(detail, sizeof(detail), "Connect USB power soon.");
    else snprintf(detail, sizeof(detail), "Battery is getting low. Charge when convenient.");
    shell_->notifications().show(title, detail);
    lastWarnedThreshold_ = threshold;
}
