#pragma once
#include <stdint.h>
#include <lvgl.h>
class Shell;

class InputManager {
public:
    void begin(Shell &shell);
private:
    template<char Character> static void altSymbol();
    static bool insertIntoFocused(char character);
    static void todayShortcut();
    static void launcherShortcut();
    static void captureShortcut();
    static void transitionShortcut();
    static void altTodayShortcut();
    static void altLauncherShortcut();
    static void altDeleteShortcut();
    static void altCaptureShortcut();
    static void altTransitionShortcut();
    static void keyboardKey(lv_event_t *event);
    static void encoderShortClicked(lv_event_t *event);
    static void encoderLongPressed(lv_event_t *event);
    static Shell *shell_;
    static lv_indev_read_cb_t keyboardRead_,encoderRead_;
    static void readKeyboard(lv_indev_t *,lv_indev_data_t *);
    static void readEncoder(lv_indev_t *,lv_indev_data_t *);
    static void read(lv_indev_t *,lv_indev_data_t *,lv_indev_read_cb_t);
};
