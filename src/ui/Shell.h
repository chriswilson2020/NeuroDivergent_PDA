#pragma once
#include "Launcher.h"
#include "StatusBar.h"
#include "core/AppManager.h"
#include "core/NotificationManager.h"
#include <lvgl.h>
class HardwareManager;

class Shell {
public:
    void begin(HardwareManager &hardware, App &todayApp, App &transitionApp, App &captureApp, App &calendarApp,
               App &tasksApp, App &assignmentsApp, App &packingApp, App &routinesApp, App &notesApp, App &clockApp, App &habitsApp, App &messagesApp, App &filesApp, App &settingsApp);
    void update();
    void goToday();
    void goTransition();
    void goCapture();
    void back();
    void toggleLauncher();
    void goLauncher();
    void launcherAction(const char *id, const char *label);
    bool openApp(const char *id);
    NotificationManager &notifications() { return notifications_; }
    void setMessageUnread(uint16_t count) { status_.setMessageUnread(count); }
private:
    static void screenKey(lv_event_t *event);
    void closeLauncher();
    lv_obj_t *focusBeforeLauncher_=nullptr;
    HardwareManager *hardware_ = nullptr;
    lv_obj_t *appArea_ = nullptr;
    StatusBar status_;
    Launcher launcher_;
    AppManager apps_;
    NotificationManager notifications_;
};
