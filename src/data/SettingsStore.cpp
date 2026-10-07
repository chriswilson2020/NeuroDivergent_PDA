#include "SettingsStore.h"
#include <Preferences.h>

bool SettingsStore::load() {
    Preferences preferences;
    if (!preferences.begin("pocketpda", true)) return false;
    settings_.brightness = preferences.getUChar("brightness", 12);
    settings_.dimBrightness = preferences.getUChar("dimLevel", 2);
    settings_.dimSeconds = preferences.getUShort("dimSeconds", 30);
    settings_.sleepSeconds = preferences.getUShort("sleepSeconds", 120);
    preferences.end();
    if (settings_.brightness < 1 || settings_.brightness > 16) settings_.brightness = 12;
    if (settings_.dimBrightness > settings_.brightness) settings_.dimBrightness = 2;
    if (settings_.dimSeconds < 5 || settings_.dimSeconds > 3600) settings_.dimSeconds = 30;
    if (settings_.sleepSeconds <= settings_.dimSeconds || settings_.sleepSeconds > 7200) settings_.sleepSeconds = 120;
    return true;
}

bool SettingsStore::save(const DeviceSettings &settings) {
    Preferences preferences;
    if (!preferences.begin("pocketpda", false)) return false;
    settings_ = settings;
    bool ok = preferences.putUChar("brightness", settings_.brightness) == sizeof(uint8_t);
    ok = preferences.putUChar("dimLevel", settings_.dimBrightness) == sizeof(uint8_t) && ok;
    ok = preferences.putUShort("dimSeconds", settings_.dimSeconds) == sizeof(uint16_t) && ok;
    ok = preferences.putUShort("sleepSeconds", settings_.sleepSeconds) == sizeof(uint16_t) && ok;
    preferences.end();
    return ok;
}

PowerConfig SettingsStore::powerConfig() const {
    return {static_cast<uint32_t>(settings_.dimSeconds) * 1000,
            static_cast<uint32_t>(settings_.sleepSeconds) * 1000,
            settings_.brightness, settings_.dimBrightness};
}
