#include "InputManager.h"
#include "ui/Shell.h"
#include <LilyGoLib.h>
#include <LV_Helper.h>

Shell *InputManager::shell_ = nullptr;
lv_indev_read_cb_t InputManager::keyboardRead_=nullptr,InputManager::encoderRead_=nullptr;
void InputManager::read(lv_indev_t *input,lv_indev_data_t *data,lv_indev_read_cb_t original) {
    // Check expiry BEFORE the driver invokes ALT combo callbacks or LVGL
    // dispatches the wake-up key to the formerly focused app control.
    if(shell_)shell_->locked();
    if(original)original(input,data);
    if(shell_&&(data->state==LV_INDEV_STATE_PRESSED||data->enc_diff))shell_->userActivity();
}
void InputManager::readKeyboard(lv_indev_t *i,lv_indev_data_t *d) { read(i,d,keyboardRead_); }
void InputManager::readEncoder(lv_indev_t *i,lv_indev_data_t *d) { read(i,d,encoderRead_); }
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
        if(lv_indev_get_read_cb(keyboard)!=readKeyboard)keyboardRead_=lv_indev_get_read_cb(keyboard);
        lv_indev_set_read_cb(keyboard,readKeyboard);
        lv_indev_add_event_cb(keyboard, keyboardKey, LV_EVENT_KEY, nullptr);
    }
    if (lv_indev_t *encoder = lv_get_encoder_indev()) {
        if(lv_indev_get_read_cb(encoder)!=readEncoder)encoderRead_=lv_indev_get_read_cb(encoder);
        lv_indev_set_read_cb(encoder,readEncoder);
        lv_indev_add_event_cb(encoder, encoderShortClicked, LV_EVENT_SHORT_CLICKED, nullptr);
        lv_indev_add_event_cb(encoder, encoderLongPressed, LV_EVENT_LONG_PRESSED, nullptr);
        lv_indev_set_long_press_time(encoder,700);
    }
}
void InputManager::encoderShortClicked(lv_event_t *event) {
    lv_indev_t *encoder = lv_get_encoder_indev();
    if (!encoder) return;
    lv_group_t *group = lv_indev_get_group(encoder);
    lv_obj_t *focused = group ? lv_group_get_focused(group) : nullptr;
    // LVGL enters text editing on the first wheel release. A subsequent
    // short click finishes it. Process on release so a held press can still
    // reach the global launcher shortcut without first being swallowed.
    // Keep dropdown/slider activation and keyboard Enter semantics unchanged.
    if (!group || !lv_group_get_editing(group) || !focused ||
        !lv_obj_check_type(focused, &lv_textarea_class)) return;
    lv_group_set_editing(group, false);
    lv_obj_remove_state(focused, LV_STATE_PRESSED);
    lv_indev_stop_processing(encoder);
    lv_indev_reset(encoder,nullptr);
    lv_event_stop_processing(event);
}
void InputManager::encoderLongPressed(lv_event_t *event) {
    lv_indev_t *encoder=lv_get_encoder_indev();if(!encoder||!shell_)return;
    // Reset stops LVGL's subsequent built-in long-press edit-mode toggle.
    lv_indev_reset(encoder,nullptr);lv_indev_wait_release(encoder);
    lv_event_stop_processing(event);shell_->goLauncher();
}
bool InputManager::insertIntoFocused(char character) { if(shell_)shell_->userActivity(); auto *keyboard=lv_get_keyboard_indev();auto *group=keyboard?lv_indev_get_group(keyboard):lv_group_get_default();lv_obj_t *focused=group?lv_group_get_focused(group):nullptr;if(!focused||!lv_obj_check_type(focused,&lv_textarea_class))return false;lv_textarea_add_char(focused,character);return true; }
template<char Character> void InputManager::altSymbol() { insertIntoFocused(Character); }
void InputManager::todayShortcut() { if (shell_) shell_->goToday(); }
void InputManager::launcherShortcut() { if (shell_) shell_->toggleLauncher(); }
void InputManager::captureShortcut() { if (shell_) shell_->goCapture(); }
void InputManager::transitionShortcut() { if (shell_) shell_->goTransition(); }
void InputManager::altTodayShortcut() { if(!insertIntoFocused('5'))todayShortcut(); }
void InputManager::altLauncherShortcut() { if(!insertIntoFocused('@'))launcherShortcut(); }
void InputManager::altCaptureShortcut() { if(!insertIntoFocused(';'))captureShortcut(); }
void InputManager::altTransitionShortcut() { if(!insertIntoFocused(','))transitionShortcut(); }
void InputManager::altDeleteShortcut() { if(shell_)shell_->userActivity(); auto *keyboard=lv_get_keyboard_indev();auto *group=keyboard?lv_indev_get_group(keyboard):lv_group_get_default();lv_obj_t *focused=group?lv_group_get_focused(group):nullptr;if(focused&&lv_obj_check_type(focused,&lv_textarea_class))lv_textarea_delete_char(focused); }
void InputManager::keyboardKey(lv_event_t *event) {
    lv_indev_t *keyboard = lv_get_keyboard_indev();
    if (!shell_ || !keyboard || lv_indev_get_state(keyboard) != LV_INDEV_STATE_PRESSED) return;
    const uint32_t key = lv_indev_get_key(keyboard);
    if (key != LV_KEY_BACKSPACE && key != '\b' && key != 0x7F && key != LV_KEY_ESC) return;

    // The keyboard Back key edits the active field.  Outside an input it is
    // the app-level Back control.  ESC, when available, is always navigation.
    lv_group_t *group=lv_indev_get_group(keyboard);
    lv_obj_t *focused = group?lv_group_get_focused(group):nullptr;
    if (key != LV_KEY_ESC && focused && lv_obj_check_type(focused, &lv_textarea_class)) return;

    lv_indev_stop_processing(keyboard);
    lv_indev_wait_release(keyboard);
    lv_event_stop_processing(event);
    if(group)lv_group_set_editing(group,false);
    shell_->back();
}
