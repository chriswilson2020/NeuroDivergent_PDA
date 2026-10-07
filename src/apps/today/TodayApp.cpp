#include "TodayApp.h"
#include "data/CalendarStore.h"
#include "hardware/RTCService.h"
#include "ui/Theme.h"
#include <cstdio>

void TodayApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100)); lv_obj_set_style_bg_color(root_, Theme::color(0xE9E4D8), 0); lv_obj_set_style_border_width(root_, 0, 0); lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_style_pad_all(root_, 7, 0); lv_obj_set_scrollable(root_, false);
    lv_obj_t *now = lv_obj_create(root_); Theme::stylePanel(now); lv_obj_set_size(now, 282, 180); lv_obj_align(now, LV_ALIGN_LEFT_MID, 0, 0); lv_obj_set_scrollable(now, false);
    lv_obj_t *kicker = lv_label_create(now); lv_label_set_text(kicker, "NOW"); lv_obj_set_style_text_color(kicker, Theme::color(0x1E6675), 0); lv_obj_set_style_text_font(kicker, &lv_font_montserrat_14, 0); lv_obj_align(kicker, LV_ALIGN_TOP_LEFT, 0, 0);
    nowTitle_ = lv_label_create(now); lv_obj_set_width(nowTitle_, LV_PCT(100)); lv_obj_set_style_text_font(nowTitle_, &lv_font_montserrat_30, 0); lv_obj_set_style_text_color(nowTitle_, Theme::color(0x172128), 0); lv_obj_align(nowTitle_, LV_ALIGN_TOP_LEFT, 0, 23);
    nowRoom_ = lv_label_create(now); lv_obj_set_style_text_font(nowRoom_, &lv_font_montserrat_42, 0); lv_obj_set_style_text_color(nowRoom_, Theme::color(0xD06C1D), 0); lv_obj_align(nowRoom_, LV_ALIGN_LEFT_MID, 0, 15);
    nowTime_ = lv_label_create(now); lv_obj_set_style_text_font(nowTime_, &lv_font_montserrat_18, 0); lv_obj_align(nowTime_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *next = lv_obj_create(root_); Theme::stylePanel(next); lv_obj_set_size(next, 177, 180); lv_obj_align(next, LV_ALIGN_RIGHT_MID, 0, 0); lv_obj_set_scrollable(next, false);
    lv_obj_t *nextKicker = lv_label_create(next); lv_label_set_text(nextKicker, "NEXT"); lv_obj_set_style_text_color(nextKicker, Theme::color(0x1E6675), 0); lv_obj_align(nextKicker, LV_ALIGN_TOP_LEFT, 0, 0);
    nextTitle_ = lv_label_create(next); lv_obj_set_width(nextTitle_, LV_PCT(100)); lv_obj_set_style_text_font(nextTitle_, &lv_font_montserrat_20, 0); lv_obj_align(nextTitle_, LV_ALIGN_TOP_LEFT, 0, 28);
    nextDetail_ = lv_label_create(next); lv_obj_set_width(nextDetail_, LV_PCT(100)); lv_obj_set_style_text_font(nextDetail_, &lv_font_montserrat_16, 0); lv_obj_align(nextDetail_, LV_ALIGN_TOP_LEFT, 0, 57);
    countdown_ = lv_label_create(next); lv_obj_set_width(countdown_, LV_PCT(100)); lv_obj_set_style_text_font(countdown_, &lv_font_montserrat_18, 0); lv_obj_set_style_text_color(countdown_, Theme::color(0xD06C1D), 0); lv_obj_set_style_text_align(countdown_, LV_TEXT_ALIGN_LEFT, 0); lv_obj_align(countdown_, LV_ALIGN_BOTTOM_LEFT, 0, -3);
    timer_ = lv_timer_create(timerTick, 1000, this);
    refresh();
}
void TodayApp::resume() { if (timer_) lv_timer_resume(timer_); refresh(); }
void TodayApp::suspend() { if (timer_) lv_timer_pause(timer_); }
void TodayApp::destroy() {
    if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; }
    if (root_) { lv_obj_delete(root_); root_ = nullptr; }
    nowTitle_ = nowRoom_ = nowTime_ = nextTitle_ = nextDetail_ = countdown_ = nullptr;
}
void TodayApp::timerTick(lv_timer_t *timer) { static_cast<TodayApp *>(lv_timer_get_user_data(timer))->refresh(); }
void TodayApp::refresh() {
    if (!root_) return;
    struct tm value{}; rtc_.now(value);
    calendar_.ensureWindowForDate(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday);
    const int minute = value.tm_hour * 60 + value.tm_min;
    const CalendarEvent *current = nullptr, *next = nullptr;
    uint16_t currentStart = 0, currentEnd = 0, nextStart = 0;
    for (size_t i = 0; i < calendar_.count(); ++i) {
        const CalendarEvent &event = calendar_.at(i);
        if (!calendar_.occursOn(event, value.tm_year + 1900, value.tm_mon + 1, value.tm_mday)) continue;
        const uint16_t start = event.startHour * 60 + event.startMinute;
        const uint16_t end = event.endHour * 60 + event.endMinute;
        if (minute >= start && minute < end && (!current || start > currentStart)) { current = &event; currentStart = start; currentEnd = end; }
        if (minute < start && (!next || start < nextStart)) { next = &event; nextStart = start; }
    }
    char timeText[32], detail[40], countdown[48];
    if (current) {
        lv_label_set_text(nowTitle_, current->title); lv_label_set_text(nowRoom_, current->location[0] ? current->location : "-");
        snprintf(timeText, sizeof(timeText), "%02u:%02u - %02u:%02u", current->startHour, current->startMinute, current->endHour, current->endMinute);
        const int remaining = currentEnd - minute;
        snprintf(countdown, sizeof(countdown), "%d min remaining", remaining);
    } else {
        lv_label_set_text(nowTitle_, "FREE"); lv_label_set_text(nowRoom_, "-");
        snprintf(timeText, sizeof(timeText), "No event now");
        if (next) { const int until = nextStart - minute; snprintf(countdown, sizeof(countdown), "Starts in %dh %02dm", until / 60, until % 60); }
        else snprintf(countdown, sizeof(countdown), "Nothing else today");
    }
    lv_label_set_text(nowTime_, timeText);
    if (next) {
        lv_label_set_text(nextTitle_, next->title); snprintf(detail, sizeof(detail), "%s  %02u:%02u", next->location[0] ? next->location : "-", next->startHour, next->startMinute); lv_label_set_text(nextDetail_, detail);
    } else { lv_label_set_text(nextTitle_, "Clear"); lv_label_set_text(nextDetail_, "No more events"); }
    lv_label_set_text(countdown_, countdown);
}
