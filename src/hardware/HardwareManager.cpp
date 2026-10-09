#include "HardwareManager.h"
#include <Arduino.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <esp_heap_caps.h>

bool HardwareManager::begin(bool initRadio) {
    LilyGoDeviceInitOptions options = instance.getDefaultInitOptions();
    options.scanI2c = false;
    // This also installs LilyGoLib's shared-SPI callbacks for the runtime
    // TinyUSB mass-storage service used by Settings > USB DISK.
    options.initFatfs = true;
    options.initSensor = false;
    options.initNfc = false;
    options.initGps = false;
    options.initSd = false;
    options.initAudio = false;
    options.initCodec = false;
    options.initRadio = initRadio;
    probeMask_ = instance.begin(options);
    if ((probeMask_ & HW_GAUGE_ONLINE) && instance.gauge.refresh()) {
        constexpr uint16_t kPagerBatteryCapacityMah = 1500;
        const uint16_t designCapacity = instance.gauge.getDesignCapacity();
        if (designCapacity != kPagerBatteryCapacityMah) {
            // BQ27220 configuration is RAM-backed and returns to its 3000 mAh
            // default after losing power. LilyGoLib may skip restoring it when
            // an old NVS success record exists, so verify the live gauge value.
            const bool configured = instance.gauge.setNewCapacity(kPagerBatteryCapacityMah,
                                                                   kPagerBatteryCapacityMah);
            delay(100);
            instance.gauge.refresh();
            Serial.printf("[PocketPDA] gauge capacity restore=%u old=%umAh design=%umAh full=%umAh\n",
                          configured, designCapacity, instance.gauge.getDesignCapacity(),
                          instance.gauge.getFullChargeCapacity());
        }
    }
    if (probeMask_ & HW_PMU_ONLINE) {
        const bool enabled = instance.enableCharge();
        Serial.printf("[PocketPDA] charger enable=%u current=%umA\n", enabled, instance.getChargeCurrent());
    }
    beginLvglHelper(instance);
    // LV_Helper creates its default group after registering the input devices.
    // Rebind them so the Pager's encoder and keyboard drive that live group.
    lv_group_t *inputGroup = lv_group_get_default();
    if (!inputGroup) inputGroup = lv_group_create();
    lv_set_default_group(inputGroup);
    setBrightness(12);
    instance.kb.setRepeat(true);
    rtc.begin(probeMask_ & HW_RTC_ONLINE);
    haptic.begin(probeMask_ & HW_DRV_ONLINE);
    battery.begin(probeMask_ & HW_GAUGE_ONLINE);
    Serial.printf("[PocketPDA] probe=0x%08lx heap=%u psram=%u\n", static_cast<unsigned long>(probeMask_), ESP.getFreeHeap(), ESP.getFreePsram());
    return (probeMask_ & (HW_PSRAM_ONLINE | HW_RTC_ONLINE | HW_KEYBOARD_ONLINE)) == (HW_PSRAM_ONLINE | HW_RTC_ONLINE | HW_KEYBOARD_ONLINE);
}
bool HardwareManager::enableRadio() { if (radioAvailable()) return true; const bool ok=instance.initLoRa(); if(ok)probeMask_|=HW_RADIO_ONLINE; return ok; }
void HardwareManager::update() { instance.loop(); battery.update(); }
void HardwareManager::setBrightness(uint8_t level) { brightness_ = constrain(level, 0, 16); instance.setBrightness(brightness_); }
void HardwareManager::setDisplaySleeping(bool sleeping) {
    if (sleeping == displaySleeping_) return;
    displaySleeping_ = sleeping;
    if (sleeping) {
        if (probeMask_ & HW_KEYBOARD_ONLINE) {
            const uint8_t current = instance.kb.getBrightness();
            if (current) keyboardBrightness_ = current;
            instance.kb.setBrightness(0);
        }
        setBrightness(0);
        instance.sleepDisplay();
        return;
    }
    instance.wakeupDisplay();
    if (probeMask_ & HW_KEYBOARD_ONLINE) instance.kb.setBrightness(keyboardBrightness_);
}
bool HardwareManager::shutdown() {
    const uint8_t previousBrightness = brightness_;
    if (radioAvailable()) radio.sleep();
    storage.unmount();
    setBrightness(0);
    if (instance.shutdown()) return true;
    storage.mount(spi);
    setBrightness(previousBrightness);
    return false;
}
