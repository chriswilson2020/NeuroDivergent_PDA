#pragma once
#include <lvgl.h>
class RTCService;
class BatteryService;

class StatusBar {
public:
    void create(lv_obj_t *parent, RTCService &rtc, BatteryService &battery);
    void update();
    void setNotification(bool active);
private:
    RTCService *rtc_ = nullptr;
    BatteryService *battery_ = nullptr;
    lv_obj_t *time_ = nullptr;
    lv_obj_t *date_ = nullptr;
    lv_obj_t *batteryLabel_ = nullptr;
    lv_obj_t *notification_ = nullptr;
    uint32_t lastUpdateMs_ = 0;
};
