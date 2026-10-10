#include "ReminderService.h"
#include "Deadline.h"
#include "PowerOptions.h"
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
    lastWall_ = mktime(&value)-1;
    rtcRevision_ = rtc.revision();
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
    strlcpy(snoozedTitle_,shell_->notifications().actionTitle(),sizeof(snoozedTitle_));
    strlcpy(snoozedDetail_,shell_->notifications().actionDetail(),sizeof(snoozedDetail_));
    struct tm value{};
    rtc_->now(value);
    const time_t now = mktime(&value);
    snoozeAt_ = now + 5 * 60;
    reschedule(now);
}

void ReminderService::update() {
    if (!calendar_ || !tasks_ || !rtc_ || !shell_ || (lastCheckMs_ && millis() - lastCheckMs_ < 1000)) return;
    lastCheckMs_ = millis();
    struct tm value{};
    rtc_->now(value);
    const time_t now = mktime(&value);

    if (rtcRevision_ != rtc_->revision() || now < lastWall_) {
        rtcRevision_ = rtc_->revision(); lastWall_ = now-1;
        reschedule(now);
    }

    if (snoozeAt_ && now >= snoozeAt_) {
        snoozeAt_ = 0;
        setReminder(snoozedTitle_,snoozedDetail_);
    }

    calendar_->ensureWindowForDate(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday);
    // Include tomorrow's lessons whose advance reminder falls before midnight,
    // and yesterday's due events if processing resumes just after midnight.
    for (int offset=-1; offset<=1; ++offset) {
    struct tm day=value; day.tm_mday+=offset; day.tm_hour=12; day.tm_min=0; day.tm_sec=0; day.tm_isdst=-1; mktime(&day);
    for (size_t i = 0; i < calendar_->count(); ++i) {
        const auto &event = calendar_->at(i);
        if (!calendar_->occursOn(event, day.tm_year + 1900, day.tm_mon + 1, day.tm_mday)) continue;
        const time_t start = makeTime(day.tm_year + 1900, day.tm_mon + 1, day.tm_mday, event.startHour, event.startMinute);
        const time_t trigger = start - event.reminderMinutes * 60;
        if (Deadline::crossed(trigger, lastWall_, now)) {
#if POCKETPDA_POWER_DIAGNOSTICS
            Serial.printf("[PocketPDA][deadline] calendar id=%lu late_s=%lld\n",static_cast<unsigned long>(event.id),static_cast<long long>(now-trigger));
#endif
            char title[64], detail[96];
            const int minutes = static_cast<int>((start - now + 59) / 60);
            snprintf(title, sizeof(title), "%s IN %d MIN", event.title, minutes < 0 ? 0 : minutes);
            snprintf(detail, sizeof(detail), "%s\n%02u:%02u - %02u:%02u", event.location,
                     event.startHour, event.startMinute, event.endHour, event.endMinute);
            setReminder(title, detail);
        }
    }
    }

    for (size_t i = 0; i < tasks_->count(); ++i) {
        const auto &task = tasks_->at(i);
        if (task.completed || !task.reminder || !task.dueYear) continue;
        const time_t due = makeTime(task.dueYear, task.dueMonth, task.dueDay, 9, 0);
        if (Deadline::crossed(due, lastWall_, now)) {
#if POCKETPDA_POWER_DIAGNOSTICS
            Serial.printf("[PocketPDA][deadline] task id=%lu late_s=%lld\n",static_cast<unsigned long>(task.id),static_cast<long long>(now-due));
#endif
            char title[64];
            snprintf(title, sizeof(title), "TASK DUE: %s", task.title);
            setReminder(title, task.notes[0] ? task.notes : "Due today");
        }
    }

    lastWall_ = now;

    if (!lastScheduleMs_ || millis() - lastScheduleMs_ >= 300000 || (nextAlarm_ && now >= nextAlarm_)) {
        lastScheduleMs_ = millis();
        reschedule(now);
    }
}

bool ReminderService::prepareSleep() {
    if(!calendar_||!tasks_||!rtc_||rtc_->revision()!=rtcRevision_)return false;
    // Fresh SD-backed calendar scan includes edits outside the RAM window.
    // Use the last processed time so due-but-not-yet-dispatched reminders veto.
    return reschedule(lastWall_);
}

bool ReminderService::reschedule(time_t now) {
    time_t best = 0;
    const bool calendarReady=calendar_->nextReminderAfter(now, best);
    for (size_t i = 0; i < tasks_->count(); ++i) {
        const auto &task = tasks_->at(i);
        if (task.completed || !task.reminder || !task.dueYear) continue;
        const time_t candidate = makeTime(task.dueYear, task.dueMonth, task.dueDay, 9, 0);
        if (candidate > now && (!best || candidate < best)) best = candidate;
    }
    if (snoozeAt_ > now && (!best || snoozeAt_ < best)) best = snoozeAt_;
    nextAlarm_ = best;
    // The sleep coordinator owns the single hardware RTC alarm.
    return calendarReady;
}
