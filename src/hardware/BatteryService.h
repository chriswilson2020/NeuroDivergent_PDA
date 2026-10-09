#pragma once
#include <stdint.h>

class BatteryService {
public:
    void begin(bool available) { available_ = available; }
    void update();
    uint8_t percent() const { return percent_; }
    bool charging() const { return charging_; }
    bool usbPresent() const { return usbPresent_; }
    bool chargeDone() const { return chargeDone_; }
    bool chargeFault() const { return chargeFault_; }
    uint32_t chargeFaultCode() const { return chargeFaultCode_; }
    uint16_t voltageMv() const { return voltageMv_; }
    int16_t currentMa() const { return currentMa_; }
    bool available() const { return available_; }
    bool valid() const { return valid_; }
private:
    bool available_ = false;
    bool charging_ = false;
    bool usbPresent_ = false;
    bool chargeDone_ = false;
    bool chargeFault_ = false;
    bool valid_ = false;
    uint8_t percent_ = 0;
    uint16_t voltageMv_ = 0;
    int16_t currentMa_ = 0;
    uint32_t chargeFaultCode_ = 0;
    uint32_t lastReadMs_ = 0;
    uint32_t lastChargeReadMs_ = 0;
    uint32_t lastDiagnosticMs_ = 0;
    uint8_t fullMismatchSamples_ = 0;
    bool ocvCorrectionRequested_ = false;
};
