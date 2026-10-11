#include "LockScreen.h"
#include "FormWidgets.h"
#include "FormFocus.h"
#include "Theme.h"
#include "data/PasscodeStore.h"
#include <esp_timer.h>
#include <cstdio>
#include <initializer_list>

void LockScreen::begin(PasscodeStore &store,Changed changed,void *context,uint64_t now) {
    store_=&store;changed_=changed;context_=context;now_=now;
    root_=lv_obj_create(lv_layer_top());lv_obj_set_size(root_,480,222);lv_obj_set_pos(root_,0,0);
    lv_obj_set_style_bg_color(root_,Theme::color(0xE9E4D8),0);lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);
    lv_obj_set_style_pad_all(root_,0,0);lv_obj_set_style_border_width(root_,0,0);lv_obj_set_style_radius(root_,0,0);lv_obj_set_scrollable(root_,false);
    entry_=lv_obj_create(root_);lv_obj_remove_style_all(entry_);lv_obj_set_size(entry_,480,222);lv_obj_set_scrollable(entry_,false);
    auto *title=lv_label_create(entry_);lv_label_set_text(title,"POCKETPDA LOCKED");lv_obj_set_pos(title,18,12);
    lv_obj_set_style_text_font(title,&lv_font_montserrat_20,0);
    pin_=FormWidgets::text(entry_,"6 letters / numbers",18,47,210,42);
    lv_textarea_set_password_mode(pin_,true);lv_textarea_set_password_show_time(pin_,0);
    lv_textarea_set_max_length(pin_,6);lv_textarea_set_accepted_chars(pin_,"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    lv_obj_add_event_cb(pin_,submit,LV_EVENT_READY,this);
    auto *unlock=FormWidgets::button(entry_,"UNLOCK",18,98,210,36,true);lv_obj_add_event_cb(unlock,submit,LV_EVENT_CLICKED,this);
    status_=lv_label_create(entry_);lv_obj_set_pos(status_,18,145);lv_obj_set_width(status_,210);lv_label_set_long_mode(status_,LV_LABEL_LONG_WRAP);
    alert_=lv_label_create(root_);lv_obj_set_pos(alert_,18,207);lv_obj_set_width(alert_,444);lv_obj_set_style_text_align(alert_,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_font(alert_,&lv_font_montserrat_12,0);
    lv_label_set_text(alert_,"Alert waiting - unlock to view");lv_obj_set_hidden(alert_,true);
    auto *hint=lv_label_create(entry_);lv_label_set_text(hint,"Type letters. Case matters.\nNumbers: ALT or keypad.");lv_obj_set_pos(hint,18,173);lv_obj_set_style_text_font(hint,&lv_font_montserrat_12,0);
    group_=lv_group_create();lv_group_add_obj(group_,pin_);lv_group_add_obj(group_,unlock);
    static const char *labels[]={"1","2","3","4","5","6","7","8","9","DEL","0","OK"};
    for(unsigned i=0;i<12;++i) {
        auto *b=FormWidgets::button(entry_,labels[i],251+(i%3)*72,43+(i/3)*42,65,36);
        lv_obj_set_user_data(b,reinterpret_cast<void*>(uintptr_t(i+1)));
        lv_obj_add_event_cb(b,digit,LV_EVENT_CLICKED,this);lv_group_add_obj(group_,b);
    }
    auto *backButton=FormWidgets::button(entry_,"BACK",365,4,100,30);lv_obj_add_event_cb(backButton,backClicked,LV_EVENT_CLICKED,this);lv_group_add_obj(group_,backButton);
    ownerView_=lv_obj_create(root_);lv_obj_remove_style_all(ownerView_);lv_obj_set_pos(ownerView_,18,0);lv_obj_set_size(ownerView_,444,205);lv_obj_set_scrollable(ownerView_,false);
    auto *found=lv_label_create(ownerView_);lv_label_set_text(found,"If found, please contact");lv_obj_set_pos(found,0,8);lv_obj_set_width(found,444);
    lv_obj_set_style_text_font(found,&lv_font_montserrat_14,0);lv_obj_set_style_text_align(found,LV_TEXT_ALIGN_CENTER,0);
    auto *details=lv_obj_create(ownerView_);lv_obj_remove_style_all(details);lv_obj_set_pos(details,0,32);lv_obj_set_size(details,444,132);lv_obj_set_scrollable(details,false);
    lv_obj_set_flex_flow(details,LV_FLEX_FLOW_COLUMN);lv_obj_set_flex_align(details,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);lv_obj_set_style_pad_row(details,6,0);
    ownerName_=lv_label_create(details);ownerPhone_=lv_label_create(details);ownerEmail_=lv_label_create(details);
    for(auto *label:{ownerName_,ownerPhone_,ownerEmail_}) {
        lv_obj_set_width(label,444);lv_obj_set_style_text_font(label,&lv_font_montserrat_20,0);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    }
    lv_label_set_long_mode(ownerName_,LV_LABEL_LONG_SCROLL_CIRCULAR);lv_obj_set_height(ownerName_,25);
    lv_label_set_long_mode(ownerPhone_,LV_LABEL_LONG_SCROLL_CIRCULAR);lv_obj_set_height(ownerPhone_,25);
    lv_label_set_long_mode(ownerEmail_,LV_LABEL_LONG_WRAP);
    continue_=FormWidgets::button(ownerView_,"UNLOCK PAGER",112,172,220,32,true);lv_obj_add_event_cb(continue_,ownerClicked,LV_EVENT_CLICKED,this);
    ownerGroup_=lv_group_create();lv_group_add_obj(ownerGroup_,continue_);
    lv_obj_set_hidden(root_,true);preferencesChanged(now);
    if(store.enabled())lock();
}
void LockScreen::preferencesChanged(uint64_t now) { if(store_)policy_.configure(store_->enabled(),store_->timeoutSeconds(),now); }
void LockScreen::lock() {
    if(!store_||!store_->enabled()||locked())return;
    policy_.lock();lv_textarea_set_text(pin_,"");shownWait_=0;
    status(store_->fault()?"Lock settings error. Owner recovery required.":"Enter your passcode.");
    lv_obj_set_hidden(root_,false);lv_obj_move_foreground(root_);
    if(changed_)changed_(context_,true);
    showOwner();
}
void LockScreen::showOwner() {
    entering_=false;lv_textarea_set_text(pin_,"");
    const auto &owner=store_->owner();
    const bool empty=!*owner.name&&!*owner.phone&&!*owner.email;
    lv_label_set_text(ownerName_,empty?"Owner details not set":owner.name);
    lv_label_set_text(ownerPhone_,owner.phone);lv_label_set_text(ownerEmail_,owner.email);
    lv_obj_set_hidden(ownerName_,!*owner.name&&!empty);lv_obj_set_hidden(ownerPhone_,!*owner.phone);lv_obj_set_hidden(ownerEmail_,!*owner.email);
    lv_obj_set_hidden(entry_,true);lv_obj_set_hidden(ownerView_,false);ensureFocus();
    // Keep normal addresses large and wrapped; unusually long addresses scroll
    // at the same readable font size instead of overflowing or shrinking.
    lv_label_set_long_mode(ownerEmail_,LV_LABEL_LONG_WRAP);lv_obj_set_height(ownerEmail_,LV_SIZE_CONTENT);
    lv_obj_update_layout(ownerView_);
    if(lv_obj_get_height(ownerEmail_)>64){lv_label_set_long_mode(ownerEmail_,LV_LABEL_LONG_SCROLL_CIRCULAR);lv_obj_set_height(ownerEmail_,25);}
}
void LockScreen::showEntry() {
    entering_=true;entryActivity_=now_;lv_obj_set_hidden(ownerView_,true);lv_obj_set_hidden(entry_,false);
    FormFocus::enter(pin_);ensureFocus();
}
void LockScreen::activity(uint64_t now) {
    policy_.activity(now);now_=now;
    if(locked()){entryActivity_=now;if(!entering_)showEntry();}
}
void LockScreen::back() { if(locked())showOwner(); }
void LockScreen::ownerClicked(lv_event_t *e) { auto *self=static_cast<LockScreen*>(lv_event_get_user_data(e));if(!self->entering_)self->showEntry(); }
void LockScreen::backClicked(lv_event_t *e) { static_cast<LockScreen*>(lv_event_get_user_data(e))->back(); }
void LockScreen::update(uint64_t now) {
    now_=now;if(policy_.expired(now))lock();
    if(!locked())return;
    if(entering_&&now-entryActivity_>=15000)showOwner();
    const auto wait=policy_.waitSeconds(now);
    if(wait!=shownWait_) {
        shownWait_=wait;
        if(wait){char text[48];snprintf(text,sizeof(text),"Try again in %lu seconds.",static_cast<unsigned long>(wait));status(text);}
        else status("Enter your passcode.");
    }
    ensureFocus();
}
void LockScreen::ensureFocus() { if(locked()){auto *group=entering_?group_:ownerGroup_;FormFocus::ensure(entering_?entry_:ownerView_,nullptr,group);FormFocus::bindInputs(group);} }
void LockScreen::setAlertPending(bool pending) { if(alert_)lv_obj_set_hidden(alert_,!pending); }
void LockScreen::status(const char *text) { lv_label_set_text(status_,text); }
void LockScreen::tryUnlock() {
    now_=esp_timer_get_time()/1000;
    if(!locked()||!entering_||!policy_.canTry(now_))return;
    const char *pin=lv_textarea_get_text(pin_);
    if(!PasscodeStore::validPin(pin)){status("Enter six letters or numbers.");return;}
    const bool correct=store_->verify(pin);lv_textarea_set_text(pin_,"");
    if(!correct){policy_.failed(now_);status("Incorrect passcode.");FormFocus::enter(pin_);update(now_);return;}
    policy_.unlock(now_);lv_obj_set_hidden(root_,true);
    FormFocus::bindInputs(lv_group_get_default());
    if(changed_)changed_(context_,false);
}
void LockScreen::submit(lv_event_t *event) { static_cast<LockScreen*>(lv_event_get_user_data(event))->tryUnlock(); }
void LockScreen::digit(lv_event_t *event) {
    auto *self=static_cast<LockScreen*>(lv_event_get_user_data(event));
    const auto index=reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event)))-1;
    if(index==11){self->tryUnlock();return;}
    if(index==9)lv_textarea_delete_char(self->pin_);
    else lv_textarea_add_char(self->pin_,index==10?'0':char('1'+index));
}
