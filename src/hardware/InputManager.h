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
    static void altTodayShortcut();
    static void altLauncherShortcut();
    static void altDeleteShortcut();
    static void keyboardKey(lv_event_t *event);
    static Shell *shell_;
};
