#include "MessagesApp.h"
#include "core/MessagingService.h"
#include "core/RadioSettingsInput.h"
#include "data/MessageStore.h"
#include "data/MessagingSettingsStore.h"
#include "ui/FormWidgets.h"
#include "ui/Shell.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace{void base(lv_obj_t*o){lv_obj_set_size(o,LV_PCT(100),LV_PCT(100));lv_obj_set_style_pad_all(o,5,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,0,0);lv_obj_set_scrollable(o,false);}const char*delivery(MessageDelivery s){switch(s){case MessageDelivery::Queued:return"queued";case MessageDelivery::Sending:return"sending";case MessageDelivery::Delivered:return"delivered";case MessageDelivery::Failed:return"FAILED";}return"";}}
void MessagesApp::create(lv_obj_t*p){root_=lv_obj_create(p);base(root_);refreshTimer_=lv_timer_create(refreshTimer,500,this);showInbox();}
void MessagesApp::resume(){if(!root_)return;if(view_==View::Conversation&&contact_)showConversation(contact_);else showInbox();}
void MessagesApp::destroy(){if(refreshTimer_){lv_timer_delete(refreshTimer_);refreshTimer_=nullptr;}if(root_){lv_obj_delete(root_);root_=nullptr;}text_=count_=name_=frequency_=sf_=power_=retry_=enabled_=notifications_=pairCode_=statusLabel_=nullptr;}
size_t MessagesApp::pairedCount()const{size_t n=0;for(const auto&c:settings_.value().contacts)if(c.deviceId)++n;return n;}
void MessagesApp::refreshTimer(lv_timer_t*t){auto*s=static_cast<MessagesApp*>(lv_timer_get_user_data(t));if(!s||s->view_!=View::Settings||!s->root_)return;const size_t contacts=s->pairedCount();if(contacts!=s->shownPairedCount_){s->showSettings();return;}if(s->pairButton_)lv_label_set_text(lv_obj_get_child(s->pairButton_,0),s->service_.pairing()?(s->service_.pairingSucceeded()?"PAIRED":"CANCEL"):"PAIR");
 if(s->statusLabel_){if(s->service_.pairing()&&!s->service_.pairingSucceeded())lv_label_set_text_fmt(s->statusLabel_,"Pairing: %lus left - same code on both pagers",static_cast<unsigned long>(s->service_.pairingSecondsRemaining()));else lv_label_set_text_fmt(s->statusLabel_,"ID %08lX  |  %s",static_cast<unsigned long>(s->settings_.value().deviceId&0xffffffff),s->service_.status());}}
void MessagesApp::refreshUnread(){shell_.setMessageUnread(store_.unreadCount());}
void MessagesApp::showInbox(){view_=View::Inbox;lv_obj_clean(root_);auto*h=lv_label_create(root_);lv_label_set_text(h,"MESSAGES");lv_obj_set_style_text_font(h,&lv_font_montserrat_20,0);lv_obj_set_pos(h,5,5);auto*cfg=FormWidgets::button(root_,"SETUP",366,0,103,34);lv_obj_add_event_cb(cfg,settingsClicked,LV_EVENT_CLICKED,this);auto*list=lv_obj_create(root_);lv_obj_set_pos(list,0,40);lv_obj_set_size(list,469,144);lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(list,3,0);lv_obj_set_style_pad_row(list,4,0);lv_obj_set_style_border_width(list,0,0);size_t contacts=0;for(const auto&c:settings_.value().contacts){if(!c.deviceId)continue;++contacts;size_t unread=0;const MessageRecord*last=nullptr;for(size_t i=0;i<store_.count();++i)if(store_.at(i).contactId==c.deviceId){last=&store_.at(i);unread+=store_.at(i).unread?1:0;}auto*b=lv_button_create(list);Theme::styleButton(b,contacts==1);lv_obj_set_size(b,LV_PCT(100),46);lv_obj_set_user_data(b,const_cast<MessagingContact*>(&c));lv_obj_add_event_cb(b,contactClicked,LV_EVENT_CLICKED,this);auto*l=lv_label_create(b);char line[128];if(unread)snprintf(line,sizeof(line),"%s  (%u unread)\n%s",c.name,static_cast<unsigned>(unread),last?last->text:"No messages yet");else snprintf(line,sizeof(line),"%s\n%s",c.name,last?last->text:"No messages yet");lv_label_set_text(l,line);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_set_width(l,420);lv_obj_align(l,LV_ALIGN_LEFT_MID,0,0);}if(!contacts){auto*l=lv_label_create(list);lv_label_set_text(l,"No paired pagers. Open SETUP on both devices\nand enter the same pairing code.");lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);}lv_group_focus_obj(cfg);refreshUnread();}
void MessagesApp::showConversation(uint64_t id){view_=View::Conversation;contact_=id;store_.markRead(id);refreshUnread();lv_obj_clean(root_);const auto*c=settings_.findContact(id);auto*h=lv_label_create(root_);lv_label_set_text(h,c?c->name:"MESSAGES");lv_obj_set_style_text_font(h,&lv_font_montserrat_20,0);lv_obj_set_pos(h,5,5);auto*compose=FormWidgets::button(root_,"REPLY",365,0,104,34,true);lv_obj_add_event_cb(compose,composeClicked,LV_EVENT_CLICKED,this);auto*back=FormWidgets::button(root_,"<",315,0,42,34);lv_obj_add_event_cb(back,backClicked,LV_EVENT_CLICKED,this);auto*list=lv_obj_create(root_);lv_obj_set_pos(list,0,40);lv_obj_set_size(list,469,144);lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(list,3,0);lv_obj_set_style_pad_row(list,4,0);lv_obj_set_style_border_width(list,0,0);size_t total=0;for(size_t i=0;i<store_.count();++i)if(store_.at(i).contactId==id)++total;size_t skip=total>12?total-12:0,shown=0;for(size_t i=0;i<store_.count()&&shown<12;++i){const auto&m=store_.at(i);if(m.contactId!=id)continue;if(skip){--skip;continue;}auto*b=lv_button_create(list);Theme::styleButton(b,shown==0);lv_obj_set_size(b,LV_PCT(100),44);lv_obj_set_user_data(b,reinterpret_cast<void*>(static_cast<uintptr_t>(m.localId)));lv_obj_add_event_cb(b,messageClicked,LV_EVENT_CLICKED,this);auto*l=lv_label_create(b);char line[190],when[10]="--:--";time_t stamp=m.timestamp;struct tm local{};if(localtime_r(&stamp,&local))strftime(when,sizeof(when),"%H:%M",&local);snprintf(line,sizeof(line),"%s %s  %s\n%s",m.direction==MessageDirection::Outgoing?">":"<",when,m.direction==MessageDirection::Outgoing?delivery(m.delivery):"received",m.text);lv_label_set_text(l,line);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_set_width(l,420);lv_obj_align(l,LV_ALIGN_LEFT_MID,0,0);++shown;}if(!shown){auto*l=lv_label_create(list);lv_label_set_text(l,"No messages yet - press REPLY");lv_obj_center(l);}lv_group_focus_obj(compose);}
void MessagesApp::showCompose(){view_=View::Compose;lv_obj_clean(root_);auto*h=lv_label_create(root_);lv_label_set_text(h,"NEW MESSAGE");lv_obj_set_style_text_font(h,&lv_font_montserrat_20,0);text_=FormWidgets::text(root_,"Type up to 160 characters",0,34,469,102,false);lv_textarea_set_max_length(text_,160);lv_obj_add_event_cb(text_,textChanged,LV_EVENT_VALUE_CHANGED,this);count_=lv_label_create(root_);lv_label_set_text(count_,"0 / 160");lv_obj_set_pos(count_,350,141);auto*send=FormWidgets::button(root_,"SEND",0,146,120,38,true);lv_obj_add_event_cb(send,sendClicked,LV_EVENT_CLICKED,this);auto*back=FormWidgets::button(root_,"BACK",128,146,100,38);lv_obj_add_event_cb(back,backClicked,LV_EVENT_CLICKED,this);lv_group_focus_obj(text_);}
void MessagesApp::showDetail(uint32_t id){view_=View::Detail;selected_=id;lv_obj_clean(root_);auto*m=store_.findLocal(id);auto*h=lv_label_create(root_);lv_label_set_text(h,"MESSAGE");lv_obj_set_style_text_font(h,&lv_font_montserrat_20,0);auto*l=lv_label_create(root_);lv_obj_set_pos(l,0,35);lv_obj_set_size(l,469,100);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);if(m){char value[230],when[24]="unknown time";time_t stamp=m->timestamp;struct tm local{};if(localtime_r(&stamp,&local))strftime(when,sizeof(when),"%Y-%m-%d %H:%M",&local);snprintf(value,sizeof(value),"%s - %s - %s\n\n%s",m->direction==MessageDirection::Outgoing?"Sent":"Received",when,m->direction==MessageDirection::Outgoing?delivery(m->delivery):"delivered",m->text);lv_label_set_text(l,value);}auto*back=FormWidgets::button(root_,"BACK",0,146,100,38);lv_obj_add_event_cb(back,backClicked,LV_EVENT_CLICKED,this);if(m&&m->delivery==MessageDelivery::Failed){auto*retry=FormWidgets::button(root_,"RETRY",108,146,100,38,true);lv_obj_add_event_cb(retry,retryClicked,LV_EVENT_CLICKED,this);}auto*del=FormWidgets::button(root_,"DELETE",349,146,120,38);lv_obj_add_event_cb(del,deleteClicked,LV_EVENT_CLICKED,this);lv_group_focus_obj(back);}
void MessagesApp::showSettings(){
 if(view_==View::Settings&&pairCode_)strlcpy(pairDraft_,lv_textarea_get_text(pairCode_),sizeof(pairDraft_));
 view_=View::Settings;lv_obj_clean(root_);
 auto*panel=lv_obj_create(root_);lv_obj_set_size(panel,469,184);lv_obj_set_style_pad_all(panel,4,0);lv_obj_set_scroll_dir(panel,LV_DIR_VER);
 auto*h=lv_label_create(panel);lv_label_set_text(h,"MESSAGING SETUP");lv_obj_set_style_text_font(h,&lv_font_montserrat_20,0);
 enabled_=lv_checkbox_create(panel);lv_checkbox_set_text(enabled_," Enable messaging");lv_obj_set_pos(enabled_,0,34);if(settings_.value().enabled)lv_obj_add_state(enabled_,LV_STATE_CHECKED);
 notifications_=lv_checkbox_create(panel);lv_checkbox_set_text(notifications_," Notifications");lv_obj_set_pos(notifications_,245,34);if(settings_.value().notifications)lv_obj_add_state(notifications_,LV_STATE_CHECKED);
 name_=FormWidgets::text(panel,"Device name",0,66,245,36);lv_textarea_set_max_length(name_,23);lv_textarea_set_text(name_,settings_.value().deviceName);
 auto*save=FormWidgets::button(panel,"SAVE",253,66,96,36,true);lv_obj_add_event_cb(save,saveSettings,LV_EVENT_CLICKED,this);
 auto*back=FormWidgets::button(panel,"BACK",357,66,96,36);lv_obj_add_event_cb(back,backClicked,LV_EVENT_CLICKED,this);
 pairCode_=FormWidgets::text(panel,"Same code on both (6-16 chars)",0,112,280,36);lv_textarea_set_max_length(pairCode_,16);lv_textarea_set_text(pairCode_,pairDraft_);
 pairButton_=FormWidgets::button(panel,service_.pairing()?(service_.pairingSucceeded()?"PAIRED":"CANCEL"):"PAIR",288,112,165,36,true);lv_obj_add_event_cb(pairButton_,pairClicked,LV_EVENT_CLICKED,this);
 statusLabel_=lv_label_create(panel);lv_label_set_text_fmt(statusLabel_,"ID %08lX  |  %s",static_cast<unsigned long>(settings_.value().deviceId&0xffffffff),service_.status());lv_obj_set_pos(statusLabel_,0,158);lv_obj_set_style_text_font(statusLabel_,&lv_font_montserrat_12,0);lv_obj_set_width(statusLabel_,453);
 auto*summary=lv_label_create(panel);lv_label_set_text_fmt(summary,"Radio: %lu kHz / SF%u",static_cast<unsigned long>(settings_.value().frequencyKhz),settings_.value().spreadingFactor);lv_obj_set_pos(summary,0,198);
 auto*advanced=FormWidgets::button(panel,"ADVANCED...",288,190,165,36);lv_obj_add_event_cb(advanced,advancedClicked,LV_EVENT_CLICKED,this);
 size_t row=236;shownPairedCount_=pairedCount();
 for(const auto&c:settings_.value().contacts)if(c.deviceId){
  auto*l=lv_label_create(panel);lv_label_set_text_fmt(l,"Paired: %s (%08lX)",c.name,static_cast<unsigned long>(c.deviceId&0xffffffff));lv_obj_set_pos(l,0,row+8);lv_obj_set_width(l,335);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
  auto*forget=FormWidgets::button(panel,"FORGET",345,row,110,36);lv_obj_set_user_data(forget,const_cast<MessagingContact*>(&c));lv_obj_add_event_cb(forget,forgetClicked,LV_EVENT_CLICKED,this);row+=42;
 }
 lv_group_focus_obj(enabled_);
}
void MessagesApp::advancedClicked(lv_event_t*e){
 auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));strlcpy(s->pairDraft_,lv_textarea_get_text(s->pairCode_),sizeof(s->pairDraft_));
 if(s->settings_.value().enabled!=lv_obj_has_state(s->enabled_,LV_STATE_CHECKED)||s->settings_.value().notifications!=lv_obj_has_state(s->notifications_,LV_STATE_CHECKED)||strcmp(s->settings_.value().deviceName,lv_textarea_get_text(s->name_))){
  s->shell_.notifications().show("UNSAVED SETUP","Save your name and notification changes before opening Advanced.");return;
 }
 s->showRadioSettings();
}
void MessagesApp::showRadioSettings(){
 view_=View::RadioSettings;lv_obj_clean(root_);
 auto*panel=lv_obj_create(root_);lv_obj_set_size(panel,469,184);lv_obj_set_style_pad_all(panel,4,0);lv_obj_set_scroll_dir(panel,LV_DIR_VER);
 auto*h=lv_label_create(panel);lv_label_set_text(h,"ADVANCED RADIO");lv_obj_set_style_text_font(h,&lv_font_montserrat_20,0);
 auto*warning=lv_label_create(panel);lv_label_set_text(warning,"Frequency and SF must match on BOTH pagers.\nChanges can stop message delivery.");lv_obj_set_pos(warning,0,30);lv_obj_set_width(warning,453);
 char number[24];
 auto*fl=lv_label_create(panel);lv_label_set_text(fl,"Frequency kHz (863000-870000)");lv_obj_set_pos(fl,0,76);
 frequency_=FormWidgets::text(panel,"Frequency",300,68,153,36);lv_textarea_set_accepted_chars(frequency_,"0123456789");snprintf(number,sizeof(number),"%lu",static_cast<unsigned long>(settings_.value().frequencyKhz));lv_textarea_set_text(frequency_,number);
 auto*sl=lv_label_create(panel);lv_label_set_text(sl,"Spreading factor (7-12)");lv_obj_set_pos(sl,0,118);
 sf_=FormWidgets::text(panel,"SF",300,110,153,36);lv_textarea_set_accepted_chars(sf_,"0123456789");snprintf(number,sizeof(number),"%u",settings_.value().spreadingFactor);lv_textarea_set_text(sf_,number);
 auto*pl=lv_label_create(panel);lv_label_set_text(pl,"Power dBm (2-14)");lv_obj_set_pos(pl,0,160);
 power_=FormWidgets::text(panel,"Power",300,152,153,36);lv_textarea_set_accepted_chars(power_,"0123456789");snprintf(number,sizeof(number),"%d",settings_.value().powerDbm);lv_textarea_set_text(power_,number);
 auto*rl=lv_label_create(panel);lv_label_set_text(rl,"Retry seconds (10-300)");lv_obj_set_pos(rl,0,202);
 retry_=FormWidgets::text(panel,"Retry",300,194,153,36);lv_textarea_set_accepted_chars(retry_,"0123456789");snprintf(number,sizeof(number),"%u",settings_.value().retrySeconds);lv_textarea_set_text(retry_,number);
 auto*save=FormWidgets::button(panel,"SAVE RADIO",0,240,220,36,true);lv_obj_add_event_cb(save,saveRadioSettings,LV_EVENT_CLICKED,this);
 auto*back=FormWidgets::button(panel,"CANCEL",233,240,220,36);lv_obj_add_event_cb(back,backClicked,LV_EVENT_CLICKED,this);lv_group_focus_obj(back);
}
void MessagesApp::saveRadioSettings(lv_event_t*e){
 auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));const auto previous=s->settings_.value();auto candidate=previous;
 const char*error=RadioSettingsInput::validate(lv_textarea_get_text(s->frequency_),lv_textarea_get_text(s->sf_),lv_textarea_get_text(s->power_),lv_textarea_get_text(s->retry_),candidate);
 if(error){s->shell_.notifications().show("RADIO SETTINGS NOT SAVED",error);return;}
 s->settings_.value()=candidate;
 if(!s->settings_.save()){s->settings_.value()=previous;s->shell_.notifications().show("SAVE FAILED","Previous radio settings were kept. Please try again.");return;}
 s->service_.stopPairing();s->service_.configurationChanged();s->showSettings();
}

void MessagesApp::contactClicked(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));auto*c=static_cast<MessagingContact*>(lv_obj_get_user_data(lv_event_get_target_obj(e)));if(c)s->showConversation(c->deviceId);}void MessagesApp::composeClicked(lv_event_t*e){static_cast<MessagesApp*>(lv_event_get_user_data(e))->showCompose();}void MessagesApp::messageClicked(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));s->showDetail(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)))));}void MessagesApp::sendClicked(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));if(s->service_.send(s->contact_,lv_textarea_get_text(s->text_)))s->showConversation(s->contact_);}void MessagesApp::deleteClicked(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));s->store_.remove(s->selected_);s->showConversation(s->contact_);}void MessagesApp::backClicked(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));if(s->view_==View::RadioSettings)s->showSettings();else if(s->view_==View::Inbox)s->shell_.back();else if(s->view_==View::Conversation||s->view_==View::Settings)s->showInbox();else s->showConversation(s->contact_);}void MessagesApp::settingsClicked(lv_event_t*e){static_cast<MessagesApp*>(lv_event_get_user_data(e))->showSettings();}
void MessagesApp::retryClicked(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));s->service_.retry(s->selected_);s->showConversation(s->contact_);}
void MessagesApp::saveSettings(lv_event_t*e){
 auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));const auto previous=s->settings_.value();auto candidate=previous;
 const char*name=lv_textarea_get_text(s->name_);
 if(!name||!*name){s->shell_.notifications().show("SETTINGS NOT SAVED","Please enter a device name.");return;}
 candidate.enabled=lv_obj_has_state(s->enabled_,LV_STATE_CHECKED);candidate.notifications=lv_obj_has_state(s->notifications_,LV_STATE_CHECKED);strlcpy(candidate.deviceName,name,sizeof(candidate.deviceName));
 s->settings_.value()=candidate;
 if(!s->settings_.save()){s->settings_.value()=previous;s->shell_.notifications().show("SAVE FAILED","Previous settings were kept. Please try again.");return;}
 s->service_.configurationChanged();s->showSettings();
}
void MessagesApp::pairClicked(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));if(s->service_.pairing())s->service_.stopPairing();else if(!s->service_.startPairing(lv_textarea_get_text(s->pairCode_))){s->shell_.notifications().show("PAIRING NOT STARTED","Enter the same 6-16 character code on both pagers. Check that the radio is available.");return;}s->showSettings();}
void MessagesApp::forgetClicked(lv_event_t*e){
 auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));
 auto*c=static_cast<MessagingContact*>(lv_obj_get_user_data(lv_event_get_target_obj(e)));
 if(!c)return;s->forgetTarget_=c->deviceId;char message[160];
 snprintf(message,sizeof(message),"Forget %s and its pairing key?\nMessage history stays. Pending sends will stop.\nTo start fresh, forget on both pagers.",c->name);
 s->shell_.notifications().show("FORGET PAGER?",message,"FORGET",confirmForget,s);
}
void MessagesApp::confirmForget(void*context){
 auto*s=static_cast<MessagesApp*>(context);
 if(s->service_.forgetContact(s->forgetTarget_)){s->showSettings();s->shell_.notifications().show("PAGER FORGOTTEN","Pair again on both pagers using the same code.");}
 else s->shell_.notifications().show("FORGET FAILED","The pairing key could not be saved. Nothing was forgotten.");
}
void MessagesApp::textChanged(lv_event_t*e){auto*s=static_cast<MessagesApp*>(lv_event_get_user_data(e));if(s->count_)lv_label_set_text_fmt(s->count_,"%u / 160",static_cast<unsigned>(strlen(lv_textarea_get_text(s->text_))));}
