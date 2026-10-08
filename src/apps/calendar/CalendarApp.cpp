#include "CalendarApp.h"
#include "hardware/RTCService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace {
constexpr uint8_t kRepeatValues[] = {
    0, 1, static_cast<uint8_t>(0x80 | 0x7F), static_cast<uint8_t>(0x80 | 0x3E), 2,
    static_cast<uint8_t>(0x80 | 0x02), static_cast<uint8_t>(0x80 | 0x04), static_cast<uint8_t>(0x80 | 0x08),
    static_cast<uint8_t>(0x80 | 0x10), static_cast<uint8_t>(0x80 | 0x20), static_cast<uint8_t>(0x80 | 0x40),
    static_cast<uint8_t>(0x80 | 0x01), static_cast<uint8_t>(0x80 | 0x2A), static_cast<uint8_t>(0x80 | 0x14)
};
uint16_t repeatIndex(uint8_t value) {
    for (uint16_t i = 0; i < sizeof(kRepeatValues); ++i) if (kRepeatValues[i] == value) return i;
    return value ? 1 : 0;
}
}

void CalendarApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100)); lv_obj_set_style_pad_all(root_, 5, 0); lv_obj_set_style_border_width(root_, 0, 0); lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_scrollable(root_, false);
    if (!year_) { struct tm now{}; rtc_.now(now); year_=now.tm_year+1900; month_=now.tm_mon+1; day_=now.tm_mday; if(year_<2024||year_>2099){year_=2024;month_=1;day_=1;} }
    if (view_ == View::Editor && hasDraft_) showEditor(editingId_); else showAgenda();
}
void CalendarApp::resume() {}
void CalendarApp::suspend() { if (view_ == View::Editor) captureDraft(); }
void CalendarApp::destroy() { if(root_){lv_obj_delete(root_);root_=nullptr;} titleField_=locationField_=dateField_=startField_=endField_=reminderField_=repeatField_=nullptr; }

void CalendarApp::showAgenda() {
    view_=View::Agenda; hasDraft_=false; store_.ensureWindowForDate(year_,month_,day_); lv_obj_clean(root_);
    auto *prev=FormWidgets::button(root_,"<",0,0,40,32);lv_obj_set_user_data(prev,reinterpret_cast<void*>(static_cast<intptr_t>(-1)));lv_obj_add_event_cb(prev,navClicked,LV_EVENT_CLICKED,this);
    auto *next=FormWidgets::button(root_,">",330,0,40,32);lv_obj_set_user_data(next,reinterpret_cast<void*>(1));lv_obj_add_event_cb(next,navClicked,LV_EVENT_CLICKED,this);
    auto *add=FormWidgets::button(root_,"+ EVENT",377,0,92,32,true);lv_obj_add_event_cb(add,newClicked,LV_EVENT_CLICKED,this);
    lv_obj_t *date=lv_label_create(root_);char dateText[40];struct tm t{};t.tm_year=year_-1900;t.tm_mon=month_-1;t.tm_mday=day_;t.tm_isdst=-1;mktime(&t);strftime(dateText,sizeof(dateText),"%A  %d %b %Y",&t);lv_label_set_text(date,dateText);lv_obj_set_style_text_font(date,&lv_font_montserrat_16,0);lv_obj_set_width(date,275);lv_obj_set_style_text_align(date,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_pos(date,47,7);
    lv_obj_t *list=lv_obj_create(root_);lv_obj_set_pos(list,0,38);lv_obj_set_size(list,469,146);lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(list,3,0);lv_obj_set_style_pad_row(list,4,0);lv_obj_set_style_border_width(list,0,0);
    size_t shown=0;
    for(size_t i=0;i<store_.count();++i){const auto &ev=store_.at(i);if(!store_.occursOn(ev,year_,month_,day_))continue;char line[96];const char*repeat=CalendarStore::recurrenceLabel(ev.recurrence);snprintf(line,sizeof(line),"%02u:%02u  %-18s  %s%s%s",ev.startHour,ev.startMinute,ev.title,ev.location,repeat[0]?"  [":"",repeat[0]?repeat:"");if(repeat[0])strlcat(line,"]",sizeof(line));lv_obj_t *b=lv_button_create(list);Theme::styleButton(b,shown==0);lv_obj_set_width(b,LV_PCT(100));lv_obj_set_height(b,38);lv_obj_set_user_data(b,reinterpret_cast<void*>(static_cast<uintptr_t>(ev.id)));lv_obj_add_event_cb(b,eventClicked,LV_EVENT_CLICKED,this);lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,line);lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);lv_obj_align(l,LV_ALIGN_LEFT_MID,0,0);++shown;}
    if(!shown){lv_obj_t *empty=lv_label_create(list);lv_label_set_text(empty,"No events - press + EVENT");lv_obj_set_style_text_color(empty,Theme::color(0x6C716F),0);lv_obj_center(empty);}
    lv_group_t *group=lv_group_get_default();if(group)lv_group_set_editing(group,false);lv_group_focus_obj(agendaFocus_>0?next:agendaFocus_<0?prev:add);agendaFocus_=0;
}
void CalendarApp::showEditor(uint32_t id) {
    view_=View::Editor; editingId_=id; lv_obj_clean(root_);
    if(!hasDraft_){draft_={};struct tm now{};rtc_.now(now);draft_.year=year_;draft_.month=month_;draft_.day=day_;draft_.startHour=now.tm_hour;draft_.startMinute=(now.tm_min/5)*5;draft_.endHour=(now.tm_hour+1)%24;draft_.endMinute=draft_.startMinute;draft_.reminderMinutes=5;if(id){auto *found=store_.find(id);if(found)draft_=*found;}hasDraft_=true;}
    titleField_=FormWidgets::text(root_,"Title",0,0,215,38);locationField_=FormWidgets::text(root_,"Location",220,0,120,38);dateField_=FormWidgets::text(root_,"YYYY-MM-DD",345,0,124,38);
    startField_=FormWidgets::text(root_,"Start HH:MM",0,44,108,38);endField_=FormWidgets::text(root_,"End HH:MM",113,44,108,38);reminderField_=FormWidgets::text(root_,"Reminder min",226,44,112,38);
    repeatField_=lv_dropdown_create(root_);lv_dropdown_set_options(repeatField_,"Once\nWeekly\nDaily\nWeekdays\nMonthly\nMon\nTue\nWed\nThu\nFri\nSat\nSun\nM/W/F\nT/Th");lv_obj_set_pos(repeatField_,343,44);lv_obj_set_size(repeatField_,126,38);
    char buf[32];lv_textarea_set_text(titleField_,draft_.title);lv_textarea_set_text(locationField_,draft_.location);snprintf(buf,sizeof(buf),"%04d-%02u-%02u",draft_.year,draft_.month,draft_.day);lv_textarea_set_text(dateField_,buf);snprintf(buf,sizeof(buf),"%02u:%02u",draft_.startHour,draft_.startMinute);lv_textarea_set_text(startField_,buf);snprintf(buf,sizeof(buf),"%02u:%02u",draft_.endHour,draft_.endMinute);lv_textarea_set_text(endField_,buf);snprintf(buf,sizeof(buf),"%u",draft_.reminderMinutes);lv_textarea_set_text(reminderField_,buf);lv_dropdown_set_selected(repeatField_,repeatIndex(draft_.recurrence));
    lv_obj_t *hint=lv_label_create(root_);lv_label_set_text(hint,"Numbers: hold SPACE or orange ALT + Q-P (1-0)");lv_obj_set_style_text_color(hint,Theme::color(0x6C716F),0);lv_obj_set_style_text_font(hint,&lv_font_montserrat_12,0);lv_obj_set_pos(hint,3,99);
    auto *save=FormWidgets::button(root_,"SAVE",0,130,120,45,true);lv_obj_add_event_cb(save,saveClicked,LV_EVENT_CLICKED,this);auto *cancel=FormWidgets::button(root_,"BACK",128,130,100,45);lv_obj_add_event_cb(cancel,cancelClicked,LV_EVENT_CLICKED,this);if(editingId_){auto *del=FormWidgets::button(root_,"DELETE",349,130,120,45);lv_obj_add_event_cb(del,deleteClicked,LV_EVENT_CLICKED,this);}lv_group_t *group=lv_group_get_default();if(group)lv_group_set_editing(group,false);lv_group_focus_obj(titleField_);
}
void CalendarApp::captureDraft(){if(!titleField_)return;strlcpy(draft_.title,lv_textarea_get_text(titleField_),sizeof(draft_.title));strlcpy(draft_.location,lv_textarea_get_text(locationField_),sizeof(draft_.location));parseDraft();hasDraft_=true;}
bool CalendarApp::parseDraft(){int y,m,d,sh,sm,eh,em,rem;if(sscanf(lv_textarea_get_text(dateField_),"%d-%d-%d",&y,&m,&d)!=3||sscanf(lv_textarea_get_text(startField_),"%d:%d",&sh,&sm)!=2||sscanf(lv_textarea_get_text(endField_),"%d:%d",&eh,&em)!=2)return false;if(y<2020||m<1||m>12||d<1||d>31||sh<0||sh>23||eh<0||eh>23||sm<0||sm>59||em<0||em>59)return false;rem=atoi(lv_textarea_get_text(reminderField_));draft_.year=y;draft_.month=m;draft_.day=d;draft_.startHour=sh;draft_.startMinute=sm;draft_.endHour=eh;draft_.endMinute=em;draft_.reminderMinutes=constrain(rem,0,1440);const uint16_t repeat=lv_dropdown_get_selected(repeatField_);draft_.recurrence=repeat<sizeof(kRepeatValues)?kRepeatValues[repeat]:0;return true;}
void CalendarApp::changeDay(int delta){struct tm t{};t.tm_year=year_-1900;t.tm_mon=month_-1;t.tm_mday=day_+delta;t.tm_isdst=-1;mktime(&t);year_=t.tm_year+1900;month_=t.tm_mon+1;day_=t.tm_mday;}
void CalendarApp::deferAgenda(){if(redrawPending_)return;redrawPending_=true;lv_async_call(showAgendaAsync,this);}
void CalendarApp::deferEditor(uint32_t id){if(redrawPending_)return;pendingEditorId_=id;redrawPending_=true;lv_async_call(showEditorAsync,this);}
void CalendarApp::showAgendaAsync(void *context){auto*self=static_cast<CalendarApp*>(context);self->redrawPending_=false;if(self->root_&&lv_obj_is_valid(self->root_))self->showAgenda();}
void CalendarApp::showEditorAsync(void *context){auto*self=static_cast<CalendarApp*>(context);self->redrawPending_=false;if(self->root_&&lv_obj_is_valid(self->root_))self->showEditor(self->pendingEditorId_);}
void CalendarApp::navClicked(lv_event_t *e){auto*self=static_cast<CalendarApp*>(lv_event_get_user_data(e));int delta=static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e))));self->changeDay(delta);self->agendaFocus_=delta;self->deferAgenda();}
void CalendarApp::newClicked(lv_event_t *e){auto*self=static_cast<CalendarApp*>(lv_event_get_user_data(e));self->hasDraft_=false;self->deferEditor(0);}
void CalendarApp::eventClicked(lv_event_t *e){auto*self=static_cast<CalendarApp*>(lv_event_get_user_data(e));self->store_.ensureWindowForDate(self->year_,self->month_,self->day_);self->hasDraft_=false;self->deferEditor(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)))));}
void CalendarApp::saveClicked(lv_event_t *e){auto*self=static_cast<CalendarApp*>(lv_event_get_user_data(e));self->captureDraft();if(self->draft_.title[0]&&self->parseDraft()){self->draft_.id=self->editingId_;self->store_.upsert(self->draft_);self->year_=self->draft_.year;self->month_=self->draft_.month;self->day_=self->draft_.day;self->deferAgenda();}}
void CalendarApp::deleteClicked(lv_event_t *e){auto*self=static_cast<CalendarApp*>(lv_event_get_user_data(e));self->store_.remove(self->editingId_);self->deferAgenda();}
void CalendarApp::cancelClicked(lv_event_t *e){static_cast<CalendarApp*>(lv_event_get_user_data(e))->deferAgenda();}
