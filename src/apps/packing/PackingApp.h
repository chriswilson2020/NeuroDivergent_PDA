#pragma once
#include "core/App.h"
#include "data/PackingStore.h"
class CalendarStore;class RTCService;class HapticService;
class PackingApp:public App{
public:PackingApp(PackingStore&s,CalendarStore&c,RTCService&r,HapticService&h):store_(s),calendar_(c),rtc_(r),haptic_(h){}const char*id()const override{return"packing";}const char*title()const override{return"Packing";}void create(lv_obj_t*)override;void resume()override{}void suspend()override;void destroy()override;lv_obj_t*root()const override{return root_;}
private:enum class View:uint8_t{Pack,List,Editor};void showPack();void showList();void showEditor(uint32_t);void capture();static void templatesClicked(lv_event_t*);static void itemChecked(lv_event_t*);static void templateClicked(lv_event_t*);static void addClicked(lv_event_t*);static void saveClicked(lv_event_t*);static void backPackClicked(lv_event_t*);static void backListClicked(lv_event_t*);static void deleteClicked(lv_event_t*);PackingStore&store_;CalendarStore&calendar_;RTCService&rtc_;HapticService&haptic_;View view_=View::Pack;uint32_t editingId_=0;PackingTemplate draft_{};PackingState state_{};bool hasDraft_=false;lv_obj_t*root_=nullptr,*keyword_=nullptr,*title_=nullptr,*items_=nullptr;};
