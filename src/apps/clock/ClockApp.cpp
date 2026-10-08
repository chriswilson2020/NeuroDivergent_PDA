#include "ClockApp.h"
#include "core/TimerService.h"
#include "data/TimerStore.h"
#include "hardware/RTCService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
uint8_t effectFor(uint32_t index) { static const uint8_t effects[] = {1,47,15}; return effects[index<3?index:1]; }
uint32_t indexForEffect(uint8_t effect) { return effect==1?0:effect==15?2:1; }
}

void ClockApp::create(lv_obj_t *parent) {
    root_=lv_obj_create(parent);lv_obj_set_size(root_,LV_PCT(100),LV_PCT(100));lv_obj_set_style_pad_all(root_,5,0);
    lv_obj_set_style_border_width(root_,0,0);lv_obj_set_style_radius(root_,0,0);lv_obj_set_scrollable(root_,false);
    if(view_==View::Editor)showEditor(editingId_);else showList();timer_=lv_timer_create(tick,250,this);refresh();
}
void ClockApp::resume(){if(timer_)lv_timer_resume(timer_);refresh();}
void ClockApp::suspend(){if(timer_)lv_timer_pause(timer_);}
void ClockApp::destroy(){if(timer_){lv_timer_delete(timer_);timer_=nullptr;}if(root_){lv_obj_delete(root_);root_=nullptr;}timeLabel_=activeLabel_=nameField_=valueField_=typeDropdown_=vibeDropdown_=enabledCheck_=nullptr;}

void ClockApp::showList(){
    view_=View::List;editingId_=0;lv_obj_clean(root_);
    lv_obj_t *heading=lv_label_create(root_);lv_label_set_text(heading,"TIMERS & ALARMS");lv_obj_set_style_text_font(heading,&lv_font_montserrat_20,0);lv_obj_set_pos(heading,3,3);
    timeLabel_=lv_label_create(root_);lv_obj_set_style_text_font(timeLabel_,&lv_font_montserrat_18,0);lv_obj_align(timeLabel_,LV_ALIGN_TOP_RIGHT,-4,5);
    activeLabel_=lv_label_create(root_);lv_obj_set_width(activeLabel_,355);lv_obj_set_style_text_color(activeLabel_,Theme::color(0x1E6675),0);lv_obj_set_pos(activeLabel_,3,35);
    lv_obj_t *action=FormWidgets::button(root_,timers_.active()?"CANCEL":"+ NEW",379,29,90,32,timers_.active());lv_obj_add_event_cb(action,timers_.active()?cancelClicked:addClicked,LV_EVENT_CLICKED,this);
    lv_obj_t *list=lv_obj_create(root_);lv_obj_set_pos(list,0,66);lv_obj_set_size(list,469,118);lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(list,3,0);lv_obj_set_style_pad_row(list,4,0);lv_obj_set_style_border_width(list,0,0);
    for(size_t i=0;i<store_.count();++i){const auto &preset=store_.at(i);lv_obj_t *row=lv_obj_create(list);lv_obj_set_width(row,LV_PCT(100));lv_obj_set_height(row,39);lv_obj_set_style_pad_all(row,1,0);lv_obj_set_style_border_width(row,0,0);lv_obj_set_scrollable(row,false);
        lv_obj_t *main=lv_button_create(row);Theme::styleButton(main,i==0);lv_obj_set_size(main,363,35);lv_obj_align(main,LV_ALIGN_LEFT_MID,0,0);lv_obj_set_user_data(main,reinterpret_cast<void*>(static_cast<uintptr_t>(preset.id)));lv_obj_add_event_cb(main,presetClicked,LV_EVENT_CLICKED,this);
        char text[70];if(preset.type)snprintf(text,sizeof(text),"%s  %02u:%02u  %s",preset.name,preset.hour,preset.minute,preset.enabled?"ON":"OFF");else snprintf(text,sizeof(text),"%s  %u min",preset.name,preset.minutes);
        lv_obj_t *label=lv_label_create(main);lv_label_set_text(label,text);lv_obj_set_style_text_font(label,&lv_font_montserrat_14,0);lv_obj_align(label,LV_ALIGN_LEFT_MID,0,0);
        lv_obj_t *edit=FormWidgets::button(row,"EDIT",371,1,86,34);lv_obj_set_user_data(edit,reinterpret_cast<void*>(static_cast<uintptr_t>(preset.id)));lv_obj_add_event_cb(edit,editClicked,LV_EVENT_CLICKED,this);
    }
}

void ClockApp::showEditor(uint32_t id){
    view_=View::Editor;editingId_=id;lv_obj_clean(root_);TimerPreset preset{};if(id){if(auto *found=store_.find(id))preset=*found;}else strlcpy(preset.name,"New timer",sizeof(preset.name));
    nameField_=FormWidgets::text(root_,"Name",0,0,260,38);lv_textarea_set_text(nameField_,preset.name);
    typeDropdown_=lv_dropdown_create(root_);lv_dropdown_set_options(typeDropdown_,"Timer\nDaily alarm");lv_dropdown_set_selected(typeDropdown_,preset.type?1:0);lv_obj_set_pos(typeDropdown_,268,0);lv_obj_set_size(typeDropdown_,105,38);
    vibeDropdown_=lv_dropdown_create(root_);lv_dropdown_set_options(vibeDropdown_,"Gentle\nFocus\nUrgent");lv_dropdown_set_selected(vibeDropdown_,indexForEffect(preset.effect));lv_obj_set_pos(vibeDropdown_,379,0);lv_obj_set_size(vibeDropdown_,90,38);
    valueField_=FormWidgets::text(root_,"Minutes or HH:MM",0,49,260,40);char value[16];if(preset.type)snprintf(value,sizeof(value),"%02u:%02u",preset.hour,preset.minute);else snprintf(value,sizeof(value),"%u",preset.minutes);lv_textarea_set_text(valueField_,value);
    enabledCheck_=lv_checkbox_create(root_);lv_checkbox_set_text(enabledCheck_,"Alarm enabled");lv_obj_set_pos(enabledCheck_,280,57);if(preset.enabled)lv_obj_add_state(enabledCheck_,LV_STATE_CHECKED);
    lv_obj_t *hint=lv_label_create(root_);lv_label_set_text(hint,"Timer: minutes (1-1440)   Alarm: 24-hour HH:MM");lv_obj_set_style_text_color(hint,Theme::color(0x6C716F),0);lv_obj_set_style_text_font(hint,&lv_font_montserrat_12,0);lv_obj_set_pos(hint,3,101);
    lv_obj_t *save=FormWidgets::button(root_,"SAVE",0,135,120,42,true);lv_obj_add_event_cb(save,saveClicked,LV_EVENT_CLICKED,this);lv_obj_t *back=FormWidgets::button(root_,"BACK",128,135,100,42);lv_obj_add_event_cb(back,backClicked,LV_EVENT_CLICKED,this);
    if(id){lv_obj_t *del=FormWidgets::button(root_,"DELETE",349,135,120,42);lv_obj_add_event_cb(del,deleteClicked,LV_EVENT_CLICKED,this);}lv_group_focus_obj(nameField_);
}

void ClockApp::saveEditor(){
    const char *name=lv_textarea_get_text(nameField_);if(!name||!name[0])return;TimerPreset preset{};if(editingId_){if(auto *found=store_.find(editingId_))preset=*found;}
    preset.id=editingId_;preset.type=lv_dropdown_get_selected(typeDropdown_)?1:0;preset.enabled=lv_obj_has_state(enabledCheck_,LV_STATE_CHECKED);preset.effect=effectFor(lv_dropdown_get_selected(vibeDropdown_));strlcpy(preset.name,name,sizeof(preset.name));
    const char *value=lv_textarea_get_text(valueField_);if(preset.type){int h=0,m=0;if(sscanf(value,"%d:%d",&h,&m)!=2||h<0||h>23||m<0||m>59)return;preset.hour=h;preset.minute=m;}else{int minutes=atoi(value);if(minutes<1||minutes>1440)return;preset.minutes=minutes;}
    if(store_.upsert(preset))showList();
}

void ClockApp::refresh(){if(!root_||view_!=View::List)return;struct tm now{};rtc_.now(now);char time[12];strftime(time,sizeof(time),"%H:%M:%S",&now);lv_label_set_text(timeLabel_,time);if(timers_.active()){const int32_t left=timers_.remainingSeconds();char active[72];snprintf(active,sizeof(active),"%s  %02ld:%02ld remaining",timers_.activeName(),static_cast<long>(left/60),static_cast<long>(left%60));lv_label_set_text(activeLabel_,active);}else lv_label_set_text(activeLabel_,"Choose a preset to start a timer");}
void ClockApp::tick(lv_timer_t *timer){static_cast<ClockApp*>(lv_timer_get_user_data(timer))->refresh();}
void ClockApp::presetClicked(lv_event_t *event){auto *self=static_cast<ClockApp*>(lv_event_get_user_data(event));auto *preset=self->store_.find(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event)))));if(!preset)return;if(preset->type){preset->enabled=!preset->enabled;self->store_.save();self->showList();}else{self->timers_.start(*preset);self->showList();}}
void ClockApp::editClicked(lv_event_t *event){auto *self=static_cast<ClockApp*>(lv_event_get_user_data(event));self->showEditor(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event)))));}
void ClockApp::addClicked(lv_event_t *event){static_cast<ClockApp*>(lv_event_get_user_data(event))->showEditor(0);}
void ClockApp::cancelClicked(lv_event_t *event){auto *self=static_cast<ClockApp*>(lv_event_get_user_data(event));self->timers_.cancel();self->showList();}
void ClockApp::saveClicked(lv_event_t *event){static_cast<ClockApp*>(lv_event_get_user_data(event))->saveEditor();}
void ClockApp::backClicked(lv_event_t *event){static_cast<ClockApp*>(lv_event_get_user_data(event))->showList();}
void ClockApp::deleteClicked(lv_event_t *event){auto *self=static_cast<ClockApp*>(lv_event_get_user_data(event));self->store_.remove(self->editingId_);self->showList();}
