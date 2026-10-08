#include "TransitionApp.h"
#include "data/CalendarStore.h"
#include "hardware/RTCService.h"
#include "ui/Theme.h"
#include <cstdio>

void TransitionApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root_, Theme::color(0xE9E4D8), 0); lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_style_pad_all(root_, 8, 0); lv_obj_set_scrollable(root_, false);
    phase_ = lv_label_create(root_); lv_obj_set_width(phase_, LV_PCT(100)); lv_obj_set_style_text_align(phase_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(phase_, &lv_font_montserrat_20, 0); lv_obj_set_style_text_color(phase_, Theme::color(0xD06C1D), 0); lv_obj_align(phase_, LV_ALIGN_TOP_MID, 0, 0);
    countdown_ = lv_label_create(root_); lv_obj_set_width(countdown_, LV_PCT(100)); lv_obj_set_style_text_align(countdown_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(countdown_, &lv_font_montserrat_42, 0); lv_obj_align(countdown_, LV_ALIGN_TOP_MID, 0, 28);
    title_ = lv_label_create(root_); lv_obj_set_size(title_, LV_PCT(100), 50); lv_label_set_long_mode(title_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_style_text_font(title_, &lv_font_montserrat_24, 0); lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 82);
    detail_ = lv_label_create(root_); lv_obj_set_width(detail_, LV_PCT(100)); lv_obj_set_style_text_align(detail_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(detail_, Theme::color(0x1E6675), 0); lv_obj_set_style_text_font(detail_, &lv_font_montserrat_18, 0); lv_obj_align(detail_, LV_ALIGN_BOTTOM_MID, 0, -7);
    timer_ = lv_timer_create(timerTick, 1000, this); refresh();
}

void TransitionApp::resume() { if (timer_) lv_timer_resume(timer_); refresh(); }
void TransitionApp::suspend() { if (timer_) lv_timer_pause(timer_); }
void TransitionApp::destroy() {
    if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; }
    if (root_) { lv_obj_delete(root_); root_ = nullptr; }
    phase_ = countdown_ = title_ = detail_ = nullptr;
}
void TransitionApp::timerTick(lv_timer_t *timer) { static_cast<TransitionApp *>(lv_timer_get_user_data(timer))->refresh(); }

void TransitionApp::refresh() {
    if (!root_) return;
    struct tm now{}; rtc_.now(now); calendar_.ensureWindowForDate(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
    const int currentMinute = now.tm_hour * 60 + now.tm_min;
    const CalendarEvent *next = nullptr; int nextMinute = 0;
    for (size_t i = 0; i < calendar_.count(); ++i) {
        const auto &event = calendar_.at(i);
        if (!calendar_.occursOn(event, now.tm_year + 1900, now.tm_mon + 1, now.tm_mday)) continue;
        const int start = event.startHour * 60 + event.startMinute;
        if (start > currentMinute && (!next || start < nextMinute)) { next = &event; nextMinute = start; }
    }
    if (!next) {
        lv_label_set_text(phase_, "ALL DONE FOR TODAY"); lv_label_set_text(countdown_, "--:--");
        lv_label_set_text(title_, "No more transitions"); lv_label_set_text(detail_, "You can relax or check tomorrow's calendar."); return;
    }
    const int seconds = (nextMinute - currentMinute) * 60 - now.tm_sec;
    const int minutes = seconds > 0 ? (seconds + 59) / 60 : 0;
    lv_label_set_text(phase_, minutes <= 5 ? "LEAVE NOW" : minutes <= 10 ? "PACK UP" : "UP NEXT");
    char countdown[24]; snprintf(countdown, sizeof(countdown), "%02d:%02d", seconds > 0 ? seconds / 60 : 0, seconds > 0 ? seconds % 60 : 0); lv_label_set_text(countdown_, countdown);
    lv_label_set_text(title_, next->title);
    char detail[64]; snprintf(detail, sizeof(detail), "%s  -  starts %02u:%02u", next->location[0] ? next->location : "No room", next->startHour, next->startMinute); lv_label_set_text(detail_, detail);
}
