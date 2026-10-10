#include "hardware/InputManager.h"
#include "ui/Shell.h"
#include "ui/FormFocus.h"
#include <cassert>
#include <cstdio>
#include <cstring>
lv_indev_t *testKeyboard=nullptr,*testEncoder=nullptr;
static lv_indev_state_t wheelState=LV_INDEV_STATE_RELEASED,keyState=LV_INDEV_STATE_RELEASED;
static uint32_t key=0;static int wheelDelta=0;
static void wheel(lv_indev_t *,lv_indev_data_t *d){d->state=wheelState;d->key=LV_KEY_ENTER;d->enc_diff=wheelDelta;wheelDelta=0;}
static void keyboard(lv_indev_t *,lv_indev_data_t *d){d->state=keyState;d->key=key;}
static void wheelRead(lv_indev_state_t state){wheelState=state;lv_indev_read(testEncoder);}
static void click(){wheelRead(LV_INDEV_STATE_PRESSED);wheelRead(LV_INDEV_STATE_RELEASED);}
static void hold(){wheelRead(LV_INDEV_STATE_PRESSED);lv_tick_inc(701);wheelRead(LV_INDEV_STATE_PRESSED);wheelRead(LV_INDEV_STATE_RELEASED);}
int main(){
    lv_init();lv_display_create(480,222);lv_group_t *g=lv_group_create();lv_group_set_default(g);
    testEncoder=lv_indev_create();lv_indev_set_type(testEncoder,LV_INDEV_TYPE_ENCODER);lv_indev_set_group(testEncoder,g);lv_indev_set_read_cb(testEncoder,wheel);
    testKeyboard=lv_indev_create();lv_indev_set_type(testKeyboard,LV_INDEV_TYPE_KEYPAD);lv_indev_set_group(testKeyboard,g);lv_indev_set_read_cb(testKeyboard,keyboard);
    auto *text=lv_textarea_create(lv_screen_active());lv_textarea_set_text(text,"Hello");
    auto *button=lv_button_create(lv_screen_active());Shell shell;InputManager input;input.begin(shell);
    lv_group_focus_obj(text);click();assert(lv_group_get_editing(g));
    click();assert(!lv_group_get_editing(g));assert(!strcmp(lv_textarea_get_text(text),"Hello"));
    wheelDelta=1;wheelRead(LV_INDEV_STATE_RELEASED);assert(lv_group_get_focused(g)==button);
    hold();assert(shell.homes==1&&!lv_group_get_editing(g));
    lv_group_focus_obj(text);click();assert(lv_group_get_editing(g));hold();assert(shell.homes==2);
    lv_group_set_editing(g,false);lv_group_focus_obj(button);
    key=LV_KEY_BACKSPACE;keyState=LV_INDEV_STATE_PRESSED;lv_indev_read(testKeyboard);
    keyState=LV_INDEV_STATE_RELEASED;lv_indev_read(testKeyboard);assert(shell.backs==1);
    lv_group_focus_obj(text);keyState=LV_INDEV_STATE_PRESSED;lv_indev_read(testKeyboard);
    keyState=LV_INDEV_STATE_RELEASED;lv_indev_read(testKeyboard);
    assert(shell.backs==1&&!strcmp(lv_textarea_get_text(text),"Hell"));
    // First entry into Settings used to inherit focus outside the new form.
    // Exercise the production form-entry helper with the actual LVGL encoder.
    for(int entry=0;entry<2;++entry){
        auto *launcher=lv_obj_create(lv_screen_active());
        auto *launchButton=lv_button_create(launcher);lv_group_focus_obj(launchButton);
        lv_group_set_editing(g,true);lv_obj_set_hidden(launcher,true);
        auto *form=lv_obj_create(lv_screen_active());
        auto *date=lv_textarea_create(form);auto *time=lv_textarea_create(form);
        FormFocus::enter(date);
        assert(lv_group_get_focused(g)==date&&!lv_group_get_editing(g));
        wheelDelta=1;wheelRead(LV_INDEV_STATE_RELEASED);
        assert(lv_group_get_focused(g)==time);
        lv_obj_delete(form);lv_obj_delete(launcher);
    }
    puts("LVGL wheel, keyboard and first-entry form focus tests passed");
}
