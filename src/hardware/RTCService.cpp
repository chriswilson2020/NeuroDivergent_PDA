#include "RTCService.h"
#include <LilyGoLib.h>
#include <cstdio>
#include <cstring>

namespace {
bool validDate(const struct tm &value) {
    const int year = value.tm_year + 1900;
    return year >= 2024 && year <= 2099 && value.tm_mon >= 0 && value.tm_mon < 12 &&
           value.tm_mday >= 1 && value.tm_mday <= 31 && value.tm_hour >= 0 && value.tm_hour < 24 &&
           value.tm_min >= 0 && value.tm_min < 60 && value.tm_sec >= 0 && value.tm_sec < 60;
}

void firmwareBuildTime(struct tm &value) {
    char monthName[4]{};
    int day = 1, year = 2024, hour = 0, minute = 0, second = 0;
    sscanf(__DATE__, "%3s %d %d", monthName, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *month = strstr(months, monthName);
    value = {};
    value.tm_year = year - 1900;
    value.tm_mon = month ? static_cast<int>((month - months) / 3) : 0;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = -1;
    mktime(&value);
}
}

bool RTCService::begin(bool available) {
    available_ = available;
    if (!available_) return false;
    struct tm value{};
    instance.rtc.getDateTime(&value);
    if (!validDate(value)) {
        firmwareBuildTime(value);
        instance.rtc.setDateTime(value);
        instance.rtc.hwClockRead();
        Serial.printf("[PocketPDA] RTC initialized to firmware time %04d-%02d-%02d %02d:%02d:%02d\n",
                      value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
                      value.tm_hour, value.tm_min, value.tm_sec);
    }
    return true;
}
bool RTCService::now(struct tm &value) const {
    if (available_) instance.rtc.getDateTime(&value);
    if (!available_ || !validDate(value)) firmwareBuildTime(value);
    if (!available_) return false;
    return true;
}
bool RTCService::set(const struct tm &value) {
    if (!available_) return false;
    instance.rtc.setDateTime(value);
    instance.rtc.hwClockRead();
    ++revision_;
    return true;
}
bool RTCService::scheduleAlarm(time_t when) {
    if (!available_) return false;
    static constexpr uint8_t kNoAlarm = 0xFF;
    struct tm value{}; localtime_r(&when, &value);
    instance.rtc.disableAlarm(); instance.rtc.resetAlarm();
    instance.rtc.setAlarm(value.tm_hour, value.tm_min, value.tm_sec, value.tm_mday, kNoAlarm);
    instance.rtc.enableAlarm(); return true;
}
void RTCService::clearAlarm() { if (available_) { instance.rtc.disableAlarm(); instance.rtc.resetAlarm(); } }
