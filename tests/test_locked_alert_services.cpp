#include "core/TimerService.h"
#include "core/ReminderService.h"
#include "ui/Shell.h"
#include "hardware/RTCService.h"
#include "hardware/HapticService.h"
#include "data/CalendarStore.h"
#include "data/TaskStore.h"
#include "ui/FormFocus.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
uint32_t fakeMs=1000;int64_t fakeUs=1000000;
static unsigned haptics=0;
void HapticService::play(uint8_t){++haptics;}
int main() {
    setenv("TZ","UTC",1);tzset();
    lv_init();lv_display_create(480,222);auto *group=lv_group_create();lv_group_set_default(group);
    auto *encoder=lv_indev_create();lv_indev_set_type(encoder,LV_INDEV_TYPE_ENCODER);
    auto *lockGroup=lv_group_create();auto *pin=lv_textarea_create(lv_layer_top());lv_group_add_obj(lockGroup,pin);
    FormFocus::enter(pin);FormFocus::bindInputs(lockGroup);
    Shell shell;HapticService haptic;shell.n.begin(lv_screen_active(),haptic);shell.n.setLocked(true);
    RTCService rtc;tm t{};t.tm_year=126;t.tm_mon=9;t.tm_mday=10;t.tm_hour=8;t.tm_min=59;t.tm_sec=0;rtc.wall=mktime(&t);
    TimerStore presets;TimerPreset alarm;alarm.id=1;alarm.type=1;presets.presets.push_back(alarm);
    alarm.id=2;presets.presets.push_back(alarm);
    CalendarStore calendar;CalendarEvent lesson;lesson.id=10;lesson.reminderMinutes=0;calendar.events.push_back(lesson);
    lesson.id=11;calendar.events.push_back(lesson);
    TaskStore tasks;tasks.tasks.push_back(TaskRecord{});
    TimerService timers;timers.begin(presets,rtc,shell);
    TimerPreset countdown;countdown.minutes=1;assert(timers.start(countdown));
    ReminderService reminders;reminders.begin(calendar,tasks,rtc,shell);
    // Actual timer/reminder services cross 09:00 while notification UI is
    // locked: countdown + two alarms + two lessons + one task all fire.
    rtc.wall+=61;fakeMs+=61000;fakeUs+=61000000;
    timers.update();reminders.update();
    assert(!timers.active());assert(haptics==6);
    assert(shell.n.active()&&lv_indev_get_group(encoder)==lockGroup);
    shell.n.dismiss();assert(shell.n.active()); // cannot navigate via an alert
    fakeMs+=1000;fakeUs+=1000000;++rtc.wall;
    timers.update();reminders.update();assert(haptics==6); // no duplicate delivery
    shell.n.setLocked(false);
    unsigned dismissed=0;
    while(shell.n.active()){shell.n.dismiss();++dismissed;assert(dismissed<=6);}
    assert(dismissed==6); // all occurrences retained for viewing after unlock
    puts("Locked real services: countdown, simultaneous daily/calendar/task alarms, haptics and queued notifications passed");
}
