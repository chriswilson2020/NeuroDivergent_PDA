#pragma once
#include "core/App.h"
#include <stdint.h>
class MessageStore;class MessagingSettingsStore;class MessagingService;class Shell;
class MessagesApp:public App{
public:MessagesApp(MessageStore&s,MessagingSettingsStore&c,MessagingService&m,Shell&shell):store_(s),settings_(c),service_(m),shell_(shell){}const char*id()const override{return"messages";}const char*title()const override{return"Messages";}void create(lv_obj_t*)override;void resume()override;void suspend()override{}void destroy()override;lv_obj_t*root()const override{return root_;}
    void openConversation(uint64_t contactId){contact_=contactId;view_=View::Conversation;if(root_)showConversation(contactId);}
private:enum class View:uint8_t{Inbox,Conversation,Compose,Detail,Settings};void showInbox();void showConversation(uint64_t);void showCompose();void showDetail(uint32_t);void showSettings();void refreshUnread();size_t pairedCount()const;static void refreshTimer(lv_timer_t*);static void contactClicked(lv_event_t*);static void composeClicked(lv_event_t*);static void messageClicked(lv_event_t*);static void sendClicked(lv_event_t*);static void retryClicked(lv_event_t*);static void deleteClicked(lv_event_t*);static void backClicked(lv_event_t*);static void settingsClicked(lv_event_t*);static void saveSettings(lv_event_t*);static void pairClicked(lv_event_t*);static void textChanged(lv_event_t*);
    MessageStore&store_;MessagingSettingsStore&settings_;MessagingService&service_;Shell&shell_;lv_obj_t*root_=nullptr;lv_obj_t*text_=nullptr;lv_obj_t*count_=nullptr;lv_obj_t*name_=nullptr;lv_obj_t*frequency_=nullptr;lv_obj_t*sf_=nullptr;lv_obj_t*power_=nullptr;lv_obj_t*retry_=nullptr;lv_obj_t*enabled_=nullptr;lv_obj_t*notifications_=nullptr;lv_obj_t*pairCode_=nullptr;lv_obj_t*statusLabel_=nullptr;lv_timer_t*refreshTimer_=nullptr;View view_=View::Inbox;uint64_t contact_=0;uint32_t selected_=0;size_t shownPairedCount_=0;
};
