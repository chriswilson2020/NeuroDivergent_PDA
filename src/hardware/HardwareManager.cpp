#include "HardwareManager.h"
#include <Arduino.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <esp_heap_caps.h>

bool HardwareManager::begin() {
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
    options.initRadio = false;
    probeMask_ = instance.begin(options);
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
void HardwareManager::update() { instance.loop(); battery.update(); }
void HardwareManager::setBrightness(uint8_t level) { brightness_ = constrain(level, 0, 16); instance.setBrightness(brightness_); }
bool HardwareManager::shutdown() {
    const uint8_t previousBrightness = brightness_;
    storage.unmount();
    setBrightness(0);
    if (instance.shutdown()) return true;
    storage.mount(spi);
    setBrightness(previousBrightness);
    return false;
}
