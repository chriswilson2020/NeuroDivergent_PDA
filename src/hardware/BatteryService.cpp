#include "BatteryService.h"
#include "core/EventBus.h"
#include <Arduino.h>
#include <LilyGoLib.h>
#include <Wire.h>

namespace {
bool requestGaugeOcv() {
    Wire.beginTransmission(0x55); // BQ27220 7-bit address
    Wire.write(0x00);             // Control()/ManufacturerAccess register
    Wire.write(0x0C);             // OCV_CMD, little-endian
    Wire.write(0x00);
    return Wire.endTransmission() == 0;
}
}

void BatteryService::update() {
    if (!available_) return;
    const uint32_t now = millis();
    if (!lastChargeReadMs_ || now - lastChargeReadMs_ >= 1000) {
        lastChargeReadMs_ = now;
        const auto status = instance.getChargeStatus();
        if (status.charging != charging_ || status.vbusPresent != usbPresent_ || status.chargeDone != chargeDone_ ||
            status.fault != chargeFault_ || status.faultCode != chargeFaultCode_) {
            charging_ = status.charging;
            usbPresent_ = status.vbusPresent;
            chargeDone_ = status.chargeDone;
            chargeFault_ = status.fault;
            chargeFaultCode_ = status.faultCode;
            EventBus::instance().publish(SystemEvent::BatteryChanged);
        }
    }
    if (!lastReadMs_ || now - lastReadMs_ >= 5000) {
        lastReadMs_ = now;
        if (instance.gauge.refresh()) {
            const uint8_t next = constrain(instance.gauge.getStateOfCharge(), 0, 100);
            voltageMv_ = instance.gauge.getVoltage();
            currentMa_ = static_cast<int16_t>(instance.gauge.getCurrent());
            valid_ = true;
            if (next != percent_) { percent_ = next; EventBus::instance().publish(SystemEvent::BatteryChanged); }

            // The BQ27220 can retain an invalid coulomb count across a long charge,
            // leaving SOC low even after the charger has safely terminated. TI
            // recommends OCV_CMD while current is stable below C/20 to re-establish
            // SOC. Require three consecutive relaxed, full-voltage samples so this
            // cannot run during an ordinary charge or transient.
            const bool clearlyFullButGaugeLow = usbPresent_ && chargeDone_ && !chargeFault_ &&
                voltageMv_ >= 4150 && currentMa_ >= -30 && currentMa_ <= 30 && percent_ < 90;
            if (!ocvCorrectionRequested_ && clearlyFullButGaugeLow) {
                if (++fullMismatchSamples_ >= 3) {
                    ocvCorrectionRequested_ = requestGaugeOcv();
                    fullMismatchSamples_ = 0;
                    Serial.printf("[PocketPDA] gauge OCV correction requested=%u soc=%u%% vbat=%umV ibat=%dmA\n",
                                  ocvCorrectionRequested_, percent_, voltageMv_, currentMa_);
                }
            } else if (!clearlyFullButGaugeLow) {
                fullMismatchSamples_ = 0;
            }
        }
    }
    if (usbPresent_ && (!lastDiagnosticMs_ || now - lastDiagnosticMs_ >= 60000)) {
        lastDiagnosticMs_ = now;
        Serial.printf("[PocketPDA] power soc=%u%% vbat=%umV ibat=%dmA remain=%umAh full=%umAh design=%umAh usb=%u charging=%u done=%u fault=0x%02lx limit=%umA\n",
                      percent_, voltageMv_, currentMa_, instance.gauge.getRemainingCapacity(),
                      instance.gauge.getFullChargeCapacity(), instance.gauge.getDesignCapacity(),
                      usbPresent_, charging_, chargeDone_,
                      static_cast<unsigned long>(chargeFaultCode_), instance.getChargeCurrent());
    }
}
