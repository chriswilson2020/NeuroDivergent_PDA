#pragma once
#include "core/App.h"
#include "data/NoteStore.h"

class NotesApp:public App{
public:explicit NotesApp(NoteStore&store):store_(store){}const char*id()const override{return"notes";}const char*title()const override{return"Notes";}void create(lv_obj_t*)override;void resume()override{};void suspend()override;void destroy()override;lv_obj_t*root()const override{return root_;}
private:enum class View:uint8_t{List,Editor};void showList();void showEditor(uint32_t);void capture();static void newClicked(lv_event_t*);static void itemClicked(lv_event_t*);static void saveClicked(lv_event_t*);static void deleteClicked(lv_event_t*);static void backClicked(lv_event_t*);NoteStore&store_;lv_obj_t*root_=nullptr;lv_obj_t*titleField_=nullptr;lv_obj_t*bodyField_=nullptr;View view_=View::List;uint32_t editingId_=0;NoteRecord draft_{};char body_[2048]{};bool hasDraft_=false;};
