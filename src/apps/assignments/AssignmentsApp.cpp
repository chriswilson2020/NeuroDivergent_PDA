#include "AssignmentsApp.h"
#include "hardware/HapticService.h"
#include "hardware/RTCService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace {
uint16_t effortFor(uint32_t index){static const uint16_t values[]={10,15,30,45,60,90};return values[index<6?index:2];}
uint32_t effortIndex(uint16_t value){static const uint16_t values[]={10,15,30,45,60,90};uint32_t best=0;uint16_t distance=0xffff;for(uint32_t i=0;i<6;++i){uint16_t d=value>values[i]?value-values[i]:values[i]-value;if(d<distance){distance=d;best=i;}}return best;}
uint32_t dateKey(const AssignmentRecord &record){return record.dueYear?static_cast<uint32_t>(record.dueYear)*10000+record.dueMonth*100+record.dueDay:0xffffffffu;}
}

void AssignmentsApp::create(lv_obj_t *parent){root_=lv_obj_create(parent);lv_obj_set_size(root_,LV_PCT(100),LV_PCT(100));lv_obj_set_style_pad_all(root_,5,0);lv_obj_set_style_border_width(root_,0,0);lv_obj_set_style_radius(root_,0,0);lv_obj_set_scrollable(root_,false);if(view_==View::Editor&&hasDraft_)showEditor(editingId_);else showList();}
void AssignmentsApp::suspend(){if(view_==View::Editor)captureEditor();}
void AssignmentsApp::destroy(){if(root_){lv_obj_delete(root_);root_=nullptr;}titleField_=dueField_=effortDropdown_=priorityDropdown_=stepsField_=nullptr;}

uint32_t AssignmentsApp::bestNext(uint32_t preferredId) const{
    if(preferredId){for(size_t i=0;i<store_.count();++i){const auto &r=store_.at(i);if(r.id==preferredId&&!r.completed&&r.stepCount)return r.id;}}
    const AssignmentRecord *best=nullptr;for(size_t i=0;i<store_.count();++i){const auto &r=store_.at(i);if(r.completed||!r.stepCount)continue;if(!best||dateKey(r)<dateKey(*best)||(dateKey(r)==dateKey(*best)&&r.priority>best->priority))best=&r;}return best?best->id:0;
}

void AssignmentsApp::showList(uint32_t preferredId){
    view_=View::List;hasDraft_=false;lv_obj_clean(root_);lv_obj_t *heading=lv_label_create(root_);lv_label_set_text(heading,"ASSIGNMENT PLANNER");lv_obj_set_style_text_font(heading,&lv_font_montserrat_20,0);lv_obj_set_pos(heading,3,3);
    lv_obj_t *add=FormWidgets::button(root_,"+ NEW",385,0,84,31,true);lv_obj_add_event_cb(add,addClicked,LV_EVENT_CLICKED,this);
    const uint32_t nextId=bestNext(preferredId);AssignmentRecord *next=nextId?store_.find(nextId):nullptr;
    lv_obj_t *card=lv_obj_create(root_);Theme::stylePanel(card);lv_obj_set_pos(card,0,34);lv_obj_set_size(card,469,68);lv_obj_set_scrollable(card,false);
    if(next){lv_obj_t *name=lv_label_create(card);lv_label_set_text(name,next->title);lv_obj_set_style_text_font(name,&lv_font_montserrat_14,0);lv_obj_set_width(name,320);lv_obj_set_pos(name,5,0);
        lv_obj_t *step=lv_label_create(card);char action[80];snprintf(action,sizeof(action),"NEXT: %s",next->steps[next->currentStep]);lv_label_set_text(step,action);lv_obj_set_width(step,320);lv_label_set_long_mode(step,LV_LABEL_LONG_DOT);lv_obj_set_style_text_color(step,Theme::color(0x1E6675),0);lv_obj_set_pos(step,5,21);
        lv_obj_t *meta=lv_label_create(card);char detail[72];snprintf(detail,sizeof(detail),"Due %04d-%02u-%02u  |  %u min  |  step %u/%u",next->dueYear,next->dueMonth,next->dueDay,next->effortMinutes,next->currentStep+1,next->stepCount);lv_label_set_text(meta,detail);lv_obj_set_style_text_font(meta,&lv_font_montserrat_12,0);lv_obj_set_pos(meta,5,43);
        lv_obj_t *done=FormWidgets::button(card,"DONE STEP",346,0,104,29,true);lv_obj_set_user_data(done,reinterpret_cast<void*>(static_cast<uintptr_t>(next->id)));lv_obj_add_event_cb(done,doneClicked,LV_EVENT_CLICKED,this);
        lv_obj_t *edit=FormWidgets::button(card,"EDIT",346,33,104,29);lv_obj_set_user_data(edit,reinterpret_cast<void*>(static_cast<uintptr_t>(next->id)));lv_obj_add_event_cb(edit,editClicked,LV_EVENT_CLICKED,this);
    }else{lv_obj_t *empty=lv_label_create(card);lv_label_set_text(empty,store_.count()?"Everything is complete.":"No assignments yet - press + NEW");lv_obj_set_style_text_font(empty,&lv_font_montserrat_16,0);lv_obj_center(empty);}
    lv_obj_t *list=lv_obj_create(root_);lv_obj_set_pos(list,0,107);lv_obj_set_size(list,469,77);lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(list,2,0);lv_obj_set_style_pad_row(list,3,0);lv_obj_set_style_border_width(list,0,0);
    for(size_t i=0;i<store_.count();++i){const auto &r=store_.at(i);lv_obj_t *button=lv_button_create(list);Theme::styleButton(button,r.id==nextId);lv_obj_set_width(button,LV_PCT(100));lv_obj_set_height(button,34);lv_obj_set_user_data(button,reinterpret_cast<void*>(static_cast<uintptr_t>(r.id)));lv_obj_add_event_cb(button,itemClicked,LV_EVENT_CLICKED,this);char text[96];snprintf(text,sizeof(text),"%s%s  -  due %02u/%02u",r.completed?"[DONE] ":"",r.title,r.dueDay,r.dueMonth);lv_obj_t *label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_set_style_text_font(label,&lv_font_montserrat_12,0);lv_obj_align(label,LV_ALIGN_LEFT_MID,0,0);}
}

void AssignmentsApp::showEditor(uint32_t id){
    view_=View::Editor;editingId_=id;lv_obj_clean(root_);if(!hasDraft_){draft_={};if(id){if(auto *found=store_.find(id))draft_=*found;}else{struct tm due{};rtc_.now(due);due.tm_mday+=7;due.tm_hour=12;due.tm_isdst=-1;mktime(&due);draft_.dueYear=due.tm_year+1900;draft_.dueMonth=due.tm_mon+1;draft_.dueDay=due.tm_mday;}hasDraft_=true;}
    titleField_=FormWidgets::text(root_,"Assignment title",0,0,469,36);lv_textarea_set_text(titleField_,draft_.title);
    dueField_=FormWidgets::text(root_,"YYYY-MM-DD",0,42,145,36);char date[16];snprintf(date,sizeof(date),"%04d-%02u-%02u",draft_.dueYear,draft_.dueMonth,draft_.dueDay);lv_textarea_set_text(dueField_,date);
    effortDropdown_=lv_dropdown_create(root_);lv_dropdown_set_options(effortDropdown_,"10 min\n15 min\n30 min\n45 min\n60 min\n90 min");lv_dropdown_set_selected(effortDropdown_,effortIndex(draft_.effortMinutes));lv_obj_set_pos(effortDropdown_,152,42);lv_obj_set_size(effortDropdown_,112,36);
    priorityDropdown_=lv_dropdown_create(root_);lv_dropdown_set_options(priorityDropdown_,"Low\nNormal\nHigh");lv_dropdown_set_selected(priorityDropdown_,draft_.priority);lv_obj_set_pos(priorityDropdown_,271,42);lv_obj_set_size(priorityDropdown_,110,36);
    stepsField_=FormWidgets::text(root_,"One small action per line (up to 6)",0,84,469,49,false);char steps[300]{};for(uint8_t i=0;i<draft_.stepCount;++i){if(i)strlcat(steps,"\n",sizeof(steps));strlcat(steps,draft_.steps[i],sizeof(steps));}lv_textarea_set_text(stepsField_,steps);
    lv_obj_t *save=FormWidgets::button(root_,"SAVE",0,140,120,38,true);lv_obj_add_event_cb(save,saveClicked,LV_EVENT_CLICKED,this);lv_obj_t *back=FormWidgets::button(root_,"BACK",128,140,100,38);lv_obj_add_event_cb(back,backClicked,LV_EVENT_CLICKED,this);if(id){lv_obj_t *del=FormWidgets::button(root_,"DELETE",349,140,120,38);lv_obj_add_event_cb(del,deleteClicked,LV_EVENT_CLICKED,this);}lv_group_focus_obj(titleField_);
}

void AssignmentsApp::captureEditor(){
    if(!titleField_)return;strlcpy(draft_.title,lv_textarea_get_text(titleField_),sizeof(draft_.title));int y=0,m=0,d=0;if(sscanf(lv_textarea_get_text(dueField_),"%d-%d-%d",&y,&m,&d)==3){draft_.dueYear=y;draft_.dueMonth=m;draft_.dueDay=d;}draft_.effortMinutes=effortFor(lv_dropdown_get_selected(effortDropdown_));draft_.priority=lv_dropdown_get_selected(priorityDropdown_);
    draft_.stepCount=0;char buffer[300];strlcpy(buffer,lv_textarea_get_text(stepsField_),sizeof(buffer));char *line=strtok(buffer,"\n");while(line&&draft_.stepCount<6){while(*line==' '||*line=='\t')++line;size_t length=strlen(line);while(length&&(line[length-1]=='\r'||line[length-1]==' '))line[--length]=0;if(*line)strlcpy(draft_.steps[draft_.stepCount++],line,sizeof(draft_.steps[0]));line=strtok(nullptr,"\n");}if(draft_.currentStep>=draft_.stepCount)draft_.currentStep=draft_.stepCount?draft_.stepCount-1:0;hasDraft_=true;
}

void AssignmentsApp::addClicked(lv_event_t *event){auto *self=static_cast<AssignmentsApp*>(lv_event_get_user_data(event));self->hasDraft_=false;self->showEditor(0);}
void AssignmentsApp::itemClicked(lv_event_t *event){auto *self=static_cast<AssignmentsApp*>(lv_event_get_user_data(event));const uint32_t id=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))));auto *record=self->store_.find(id);if(record&&record->completed){self->hasDraft_=false;self->showEditor(id);}else self->showList(id);}
void AssignmentsApp::editClicked(lv_event_t *event){auto *self=static_cast<AssignmentsApp*>(lv_event_get_user_data(event));self->hasDraft_=false;self->showEditor(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event)))));}
void AssignmentsApp::doneClicked(lv_event_t *event){auto *self=static_cast<AssignmentsApp*>(lv_event_get_user_data(event));const uint32_t id=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))));self->store_.completeNextStep(id);self->haptic_.play(47);self->showList();}
void AssignmentsApp::saveClicked(lv_event_t *event){auto *self=static_cast<AssignmentsApp*>(lv_event_get_user_data(event));self->captureEditor();if(self->draft_.title[0]&&self->draft_.stepCount&&self->draft_.dueYear>=2024&&self->draft_.dueMonth>=1&&self->draft_.dueMonth<=12&&self->draft_.dueDay>=1&&self->draft_.dueDay<=31){self->draft_.id=self->editingId_;if(self->store_.upsert(self->draft_))self->showList(self->draft_.id);}}
void AssignmentsApp::backClicked(lv_event_t *event){static_cast<AssignmentsApp*>(lv_event_get_user_data(event))->showList();}
void AssignmentsApp::deleteClicked(lv_event_t *event){auto *self=static_cast<AssignmentsApp*>(lv_event_get_user_data(event));self->store_.remove(self->editingId_);self->showList();}
