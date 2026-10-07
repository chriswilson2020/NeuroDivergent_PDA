#pragma once
#include "core/App.h"
#include "data/TaskStore.h"
class RTCService;

class TasksApp : public App {
public:
    TasksApp(TaskStore &store, RTCService &rtc) : store_(store), rtc_(rtc) {}
    const char *id() const override{return "tasks";} const char *title() const override{return "Tasks";}
    void create(lv_obj_t *parent) override;void resume()override{};void suspend()override;void destroy()override;lv_obj_t*root()const override{return root_;}
private:
    enum class View:uint8_t{List,Editor};void showList();void showEditor(uint32_t id);void capture();bool parse();
    static void newClicked(lv_event_t*);static void itemClicked(lv_event_t*);static void completedChanged(lv_event_t*);static void saveClicked(lv_event_t*);static void deleteClicked(lv_event_t*);static void backClicked(lv_event_t*);
    TaskStore&store_;RTCService&rtc_;lv_obj_t*root_=nullptr;lv_obj_t*titleField_=nullptr;lv_obj_t*notesField_=nullptr;lv_obj_t*dueField_=nullptr;lv_obj_t*reminderField_=nullptr;lv_obj_t*priorityField_=nullptr;lv_obj_t*recurringField_=nullptr;
    View view_=View::List;uint32_t editingId_=0;TaskRecord draft_{};bool hasDraft_=false;
};
