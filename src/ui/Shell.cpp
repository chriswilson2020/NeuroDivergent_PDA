#include "Shell.h"
#include "Dialog.h"
#include "Theme.h"
#include "hardware/HardwareManager.h"
#include <cstdio>
#include <cstring>

void Shell::begin(HardwareManager &hardware, App &todayApp, App &transitionApp, App &captureApp, App &calendarApp,
                  App &tasksApp, App &assignmentsApp, App &routinesApp, App &notesApp, App &clockApp, App &habitsApp, App &filesApp, App &settingsApp) {
    hardware_ = &hardware;
    Theme::apply();
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_scrollable(screen, false);
    lv_obj_add_event_cb(screen, screenKey, LV_EVENT_KEY, this);
    lv_obj_t *statusHost = lv_obj_create(screen); lv_obj_set_pos(statusHost, 0, 0); status_.create(statusHost, hardware.rtc, hardware.battery);
    appArea_ = lv_obj_create(screen); lv_obj_set_pos(appArea_, 0, 28); lv_obj_set_size(appArea_, LV_PCT(100), 194); lv_obj_set_style_pad_all(appArea_, 0, 0); lv_obj_set_style_border_width(appArea_, 0, 0); lv_obj_set_style_radius(appArea_, 0, 0); lv_obj_set_scrollable(appArea_, false);
    apps_.begin(appArea_); apps_.registerApp(&todayApp); apps_.registerApp(&transitionApp); apps_.registerApp(&captureApp); apps_.registerApp(&calendarApp); apps_.registerApp(&tasksApp); apps_.registerApp(&assignmentsApp); apps_.registerApp(&routinesApp); apps_.registerApp(&notesApp); apps_.registerApp(&clockApp); apps_.registerApp(&habitsApp); apps_.registerApp(&filesApp); apps_.registerApp(&settingsApp);
    launcher_.create(appArea_, *this);
    notifications_.begin(screen, hardware.haptic);
    goToday();
}
void Shell::update() { status_.update(); status_.setNotification(notifications_.active()); }
void Shell::goToday() { launcher_.hide(); apps_.launch("today"); }
void Shell::goTransition() { launcher_.hide(); apps_.launch("transition"); }
void Shell::goCapture() { launcher_.hide(); apps_.launch("capture"); }
void Shell::back() {
    if (notifications_.active()) notifications_.dismiss();
    else if (launcher_.visible()) launcher_.hide();
    else toggleLauncher();
}
void Shell::toggleLauncher() { launcher_.toggle(); }
bool Shell::openApp(const char *id) { launcher_.hide(); return apps_.launch(id); }
void Shell::launcherAction(const char *id, const char *label) {
    if (openApp(id)) return;
    if (std::strcmp(id, "_haptic") == 0) { hardware_->haptic.play(47); notifications_.show("HAPTIC TEST", hardware_->haptic.available() ? "DRV2605 effect 47 played" : "Haptic driver not detected"); return; }
    launcher_.hide();
    char message[96]; snprintf(message, sizeof(message), "%s is not available.\n\nAlt+L opens the launcher.", label);
    notifications_.show(label, message);
}
void Shell::screenKey(lv_event_t *event) {
    auto *self = static_cast<Shell *>(lv_event_get_user_data(event));
    const uint32_t key = lv_event_get_key(event);
    if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE) {
        self->back();
    } else if (key == LV_KEY_NEXT) {
        lv_group_focus_next(lv_group_get_default());
    }
}
