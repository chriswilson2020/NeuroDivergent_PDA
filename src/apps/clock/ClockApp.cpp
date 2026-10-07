#include "ClockApp.h"
#include "hardware/RTCService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cstdio>

void ClockApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100)); lv_obj_set_style_pad_all(root_, 7, 0); lv_obj_set_style_border_width(root_, 0, 0); lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_scrollable(root_, false);
    lv_obj_t *clockPanel = lv_obj_create(root_); Theme::stylePanel(clockPanel); lv_obj_set_pos(clockPanel, 0, 0); lv_obj_set_size(clockPanel, 270, 180); lv_obj_set_scrollable(clockPanel, false);
    lv_obj_t *clockTitle = lv_label_create(clockPanel); lv_label_set_text(clockTitle, "RTC CLOCK"); lv_obj_set_style_text_color(clockTitle, Theme::color(0x1E6675), 0); lv_obj_align(clockTitle, LV_ALIGN_TOP_MID, 0, 0);
    timeLabel_ = lv_label_create(clockPanel); lv_obj_set_style_text_font(timeLabel_, &lv_font_montserrat_42, 0); lv_obj_align(timeLabel_, LV_ALIGN_CENTER, 0, -10);
    dateLabel_ = lv_label_create(clockPanel); lv_obj_set_style_text_font(dateLabel_, &lv_font_montserrat_16, 0); lv_obj_align(dateLabel_, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_t *timerPanel = lv_obj_create(root_); Theme::stylePanel(timerPanel); lv_obj_set_pos(timerPanel, 277, 0); lv_obj_set_size(timerPanel, 192, 180); lv_obj_set_scrollable(timerPanel, false);
    lv_obj_t *timerTitle = lv_label_create(timerPanel); lv_label_set_text(timerTitle, "STOPWATCH"); lv_obj_set_style_text_color(timerTitle, Theme::color(0x1E6675), 0); lv_obj_align(timerTitle, LV_ALIGN_TOP_MID, 0, 0);
    stopwatchLabel_ = lv_label_create(timerPanel); lv_obj_set_style_text_font(stopwatchLabel_, &lv_font_montserrat_24, 0); lv_obj_align(stopwatchLabel_, LV_ALIGN_CENTER, 0, -18);
    lv_obj_t *toggle = FormWidgets::button(timerPanel, running_ ? "STOP" : "START", 0, 105, 78, 42, true); toggleLabel_ = lv_obj_get_child(toggle, 0); lv_obj_add_event_cb(toggle, toggleClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *reset = FormWidgets::button(timerPanel, "RESET", 84, 105, 78, 42); lv_obj_add_event_cb(reset, resetClicked, LV_EVENT_CLICKED, this);
    timer_ = lv_timer_create(tick, 200, this);
    refresh();
}
void ClockApp::resume() { if (timer_) lv_timer_resume(timer_); refresh(); }
void ClockApp::suspend() { if (timer_) lv_timer_pause(timer_); }
void ClockApp::destroy() { if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; } if (root_) { lv_obj_delete(root_); root_ = nullptr; } timeLabel_ = dateLabel_ = stopwatchLabel_ = toggleLabel_ = nullptr; }
uint32_t ClockApp::elapsedMs() const { return accumulatedMs_ + (running_ ? millis() - runStartedMs_ : 0); }
void ClockApp::refresh() {
    if (!root_) return;
    struct tm value{}; rtc_.now(value); char time[12], date[32]; strftime(time, sizeof(time), "%H:%M:%S", &value); strftime(date, sizeof(date), "%A, %d %B %Y", &value); lv_label_set_text(timeLabel_, time); lv_label_set_text(dateLabel_, date);
    const uint32_t elapsed = elapsedMs(); char stopwatch[24]; snprintf(stopwatch, sizeof(stopwatch), "%02lu:%02lu.%01lu", static_cast<unsigned long>(elapsed / 60000), static_cast<unsigned long>((elapsed / 1000) % 60), static_cast<unsigned long>((elapsed / 100) % 10)); lv_label_set_text(stopwatchLabel_, stopwatch);
}
void ClockApp::tick(lv_timer_t *timer) { static_cast<ClockApp *>(lv_timer_get_user_data(timer))->refresh(); }
void ClockApp::toggleClicked(lv_event_t *event) { auto *self = static_cast<ClockApp *>(lv_event_get_user_data(event)); if (self->running_) { self->accumulatedMs_ = self->elapsedMs(); self->running_ = false; } else { self->runStartedMs_ = millis(); self->running_ = true; } lv_label_set_text(self->toggleLabel_, self->running_ ? "STOP" : "START"); self->refresh(); }
void ClockApp::resetClicked(lv_event_t *event) { auto *self = static_cast<ClockApp *>(lv_event_get_user_data(event)); self->accumulatedMs_ = 0; self->runStartedMs_ = millis(); self->refresh(); }
