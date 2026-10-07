#include "StatusBar.h"
#include "Theme.h"
#include "hardware/RTCService.h"
#include "hardware/BatteryService.h"
#include <Arduino.h>
#include <cstdio>

void StatusBar::create(lv_obj_t *parent, RTCService &rtc, BatteryService &battery) {
    rtc_ = &rtc; battery_ = &battery;
    lv_obj_set_size(parent, LV_PCT(100), 28);
    lv_obj_set_style_bg_color(parent, Theme::color(0x172128), 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_pad_hor(parent, 9, 0);
    lv_obj_set_style_pad_ver(parent, 3, 0);
    lv_obj_set_scrollable(parent, false);
    time_ = lv_label_create(parent); lv_obj_set_style_text_color(time_, lv_color_white(), 0); lv_obj_set_style_text_font(time_, &lv_font_montserrat_16, 0); lv_obj_align(time_, LV_ALIGN_LEFT_MID, 0, 0);
    date_ = lv_label_create(parent); lv_obj_set_style_text_color(date_, Theme::color(0xC7D6D9), 0); lv_obj_align(date_, LV_ALIGN_CENTER, 0, 0);
    notification_ = lv_label_create(parent); lv_label_set_text(notification_, LV_SYMBOL_BELL); lv_obj_set_style_text_color(notification_, Theme::color(0xE59C35), 0); lv_obj_align(notification_, LV_ALIGN_RIGHT_MID, -57, 0); lv_obj_set_hidden(notification_, true);
    batteryLabel_ = lv_label_create(parent); lv_obj_set_style_text_color(batteryLabel_, lv_color_white(), 0); lv_obj_align(batteryLabel_, LV_ALIGN_RIGHT_MID, 0, 0);
    update();
}
void StatusBar::update() {
    if (!rtc_ || (lastUpdateMs_ && millis() - lastUpdateMs_ < 1000)) return;
    lastUpdateMs_ = millis();
    struct tm value{}; rtc_->now(value);
    char timeBuf[8], dateBuf[24];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &value);
    strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &value);
    lv_label_set_text(time_, timeBuf); lv_label_set_text(date_, dateBuf);
    if (battery_ && battery_->available()) lv_label_set_text_fmt(batteryLabel_, "%d%% " LV_SYMBOL_BATTERY_FULL, battery_->percent());
    else lv_label_set_text(batteryLabel_, "--% " LV_SYMBOL_BATTERY_EMPTY);
}
void StatusBar::setNotification(bool active) { if (notification_) lv_obj_set_hidden(notification_, !active); }
