#include "ReminderService.h"
#include "data/CalendarStore.h"
#include "data/TaskStore.h"
#include "hardware/RTCService.h"
#include "ui/Shell.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace {
time_t makeTime(int year, int month, int day, int hour, int minute) {
    struct tm value{};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_isdst = -1;
    return mktime(&value);
}
}

void ReminderService::begin(CalendarStore &calendar, TaskStore &tasks, RTCService &rtc, Shell &shell) {
    calendar_ = &calendar;
    tasks_ = &tasks;
    rtc_ = &rtc;
    shell_ = &shell;
    struct tm value{};
    rtc.now(value);
    reschedule(mktime(&value));
}

void ReminderService::setReminder(const char *title, const char *detail) {
    strlcpy(reminderTitle_, title, sizeof(reminderTitle_));
    strlcpy(reminderDetail_, detail, sizeof(reminderDetail_));
    presentReminder();
}

void ReminderService::presentReminder() {
    if (!shell_) return;
    shell_->notifications().show(reminderTitle_, reminderDetail_, "SNOOZE 5", snoozeCurrent, this);
}

void ReminderService::snoozeCurrent(void *context) {
    static_cast<ReminderService *>(context)->snooze();
}

void ReminderService::snooze() {
    if (!rtc_) return;
    struct tm value{};
    rtc_->now(value);
    const time_t now = mktime(&value);
    snoozeAt_ = now + 5 * 60;
    reschedule(now);
}

void ReminderService::update() {
    if (!calendar_ || !tasks_ || !rtc_ || !shell_ || (lastCheckMs_ && millis() - lastCheckMs_ < 5000)) return;
    lastCheckMs_ = millis();
    struct tm value{};
    rtc_->now(value);
    const time_t now = mktime(&value);

    if (snoozeAt_ && now >= snoozeAt_) {
        snoozeAt_ = 0;
        presentReminder();
    }

    calendar_->ensureWindowForDate(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday);
    for (size_t i = 0; i < calendar_->count(); ++i) {
        const auto &event = calendar_->at(i);
        if (!calendar_->occursOn(event, value.tm_year + 1900, value.tm_mon + 1, value.tm_mday)) continue;
        const time_t start = makeTime(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday, event.startHour, event.startMinute);
        const time_t trigger = start - event.reminderMinutes * 60;
        const uint64_t key = (static_cast<uint64_t>(event.id) << 32) | static_cast<uint32_t>(trigger);
        if (now >= trigger && now < trigger + 60 && lastTrigger_ != key) {
            lastTrigger_ = key;
            char title[64], detail[96];
            const int minutes = static_cast<int>((start - now + 59) / 60);
            snprintf(title, sizeof(title), "%s IN %d MIN", event.title, minutes < 0 ? 0 : minutes);
            snprintf(detail, sizeof(detail), "%s\n%02u:%02u - %02u:%02u", event.location,
                     event.startHour, event.startMinute, event.endHour, event.endMinute);
            setReminder(title, detail);
        }
    }

    for (size_t i = 0; i < tasks_->count(); ++i) {
        const auto &task = tasks_->at(i);
        if (task.completed || !task.reminder || !task.dueYear) continue;
        const time_t due = makeTime(task.dueYear, task.dueMonth, task.dueDay, 9, 0);
        const uint64_t key = (1ULL << 63) | (static_cast<uint64_t>(task.id) << 32) | static_cast<uint32_t>(due);
        if (now >= due && now < due + 60 && lastTrigger_ != key) {
            lastTrigger_ = key;
            char title[64];
            snprintf(title, sizeof(title), "TASK DUE: %s", task.title);
            setReminder(title, task.notes[0] ? task.notes : "Due today");
        }
    }

    if (!lastScheduleMs_ || millis() - lastScheduleMs_ >= 300000 || (nextAlarm_ && now >= nextAlarm_)) {
        lastScheduleMs_ = millis();
        reschedule(now);
    }
}

void ReminderService::reschedule(time_t now) {
    time_t best = 0;
    calendar_->nextReminderAfter(now, best);
    for (size_t i = 0; i < tasks_->count(); ++i) {
        const auto &task = tasks_->at(i);
        if (task.completed || !task.reminder || !task.dueYear) continue;
        const time_t candidate = makeTime(task.dueYear, task.dueMonth, task.dueDay, 9, 0);
        if (candidate > now && (!best || candidate < best)) best = candidate;
    }
    if (snoozeAt_ > now && (!best || snoozeAt_ < best)) best = snoozeAt_;
    nextAlarm_ = best;
    if (best) rtc_->scheduleAlarm(best);
    else rtc_->clearAlarm();
}
