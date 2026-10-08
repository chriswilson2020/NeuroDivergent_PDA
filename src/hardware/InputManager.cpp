#include "InputManager.h"
#include "ui/Shell.h"
#include <LilyGoLib.h>
#include <LV_Helper.h>

Shell *InputManager::shell_ = nullptr;
void InputManager::begin(Shell &shell) {
    shell_ = &shell;
    // Space is LilyGo's native temporary symbol modifier.  Mirror the same
    // layer on the orange ALT key so symbols are comfortable to enter while
    // editing, without sacrificing context-aware launcher shortcuts.
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'q', altSymbol<'1'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'w', altSymbol<'2'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'e', altSymbol<'3'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'r', altSymbol<'4'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 't', altTodayShortcut); instance.kb.registerKeyCombo(ModifierKey::ALT, 'y', altSymbol<'6'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'u', altSymbol<'7'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'i', altSymbol<'8'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'o', altSymbol<'9'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'p', altSymbol<'0'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'a', altSymbol<'*'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 's', altSymbol<'/'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'd', altSymbol<'+'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'f', altSymbol<'-'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'g', altSymbol<'='>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'h', altSymbol<':'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'j', altSymbol<'\''>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'k', altSymbol<'\"'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'l', altLauncherShortcut);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'z', altSymbol<'_'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'x', altSymbol<'$'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'c', altCaptureShortcut); instance.kb.registerKeyCombo(ModifierKey::ALT, 'v', altSymbol<'?'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'b', altSymbol<'!'>); instance.kb.registerKeyCombo(ModifierKey::ALT, 'n', altTransitionShortcut);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 'm', altSymbol<'.'>);
    instance.kb.registerKeyCombo(ModifierKey::ALT, 0x1D, altDeleteShortcut);
    if (lv_indev_t *keyboard = lv_get_keyboard_indev()) {
        lv_indev_add_event_cb(keyboard, keyboardKey, LV_EVENT_KEY, nullptr);
    }
}
bool InputManager::insertIntoFocused(char character) { lv_obj_t *focused=lv_group_get_focused(lv_group_get_default());if(!focused||!lv_obj_check_type(focused,&lv_textarea_class))return false;lv_textarea_add_char(focused,character);return true; }
template<char Character> void InputManager::altSymbol() { insertIntoFocused(Character); }
void InputManager::todayShortcut() { if (shell_) shell_->goToday(); }
void InputManager::launcherShortcut() { if (shell_) shell_->toggleLauncher(); }
void InputManager::captureShortcut() { if (shell_) shell_->goCapture(); }
void InputManager::transitionShortcut() { if (shell_) shell_->goTransition(); }
void InputManager::altTodayShortcut() { if(!insertIntoFocused('5'))todayShortcut(); }
void InputManager::altLauncherShortcut() { if(!insertIntoFocused('@'))launcherShortcut(); }
void InputManager::altCaptureShortcut() { if(!insertIntoFocused(';'))captureShortcut(); }
void InputManager::altTransitionShortcut() { if(!insertIntoFocused(','))transitionShortcut(); }
void InputManager::altDeleteShortcut() { lv_obj_t *focused=lv_group_get_focused(lv_group_get_default());if(focused&&lv_obj_check_type(focused,&lv_textarea_class))lv_textarea_delete_char(focused); }
void InputManager::keyboardKey(lv_event_t *event) {
    lv_indev_t *keyboard = lv_get_keyboard_indev();
    if (!shell_ || !keyboard || lv_indev_get_state(keyboard) != LV_INDEV_STATE_PRESSED) return;
    const uint32_t key = lv_indev_get_key(keyboard);
    if (key != LV_KEY_BACKSPACE && key != '\b' && key != 0x7F && key != LV_KEY_ESC) return;

    // The keyboard Back key edits the active field.  Outside an input it is
    // the app-level Back control.  ESC, when available, is always navigation.
    lv_obj_t *focused = lv_group_get_focused(lv_group_get_default());
    if (key != LV_KEY_ESC && focused && lv_obj_check_type(focused, &lv_textarea_class)) return;

    lv_indev_stop_processing(keyboard);
    lv_indev_wait_release(keyboard);
    lv_event_stop_processing(event);
    shell_->back();
}
