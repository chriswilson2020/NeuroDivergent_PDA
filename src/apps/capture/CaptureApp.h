#pragma once
#include "core/App.h"

class TaskStore;
class NoteStore;
class RTCService;

class CaptureApp : public App {
public:
    CaptureApp(TaskStore &tasks, NoteStore &notes, RTCService &rtc) : tasks_(tasks), notes_(notes), rtc_(rtc) {}
    const char *id() const override { return "capture"; }
    const char *title() const override { return "Capture"; }
    void create(lv_obj_t *parent) override;
    void resume() override {}
    void suspend() override {}
    void destroy() override;
    lv_obj_t *root() const override { return root_; }

private:
    void saveTask();
    void saveNote();
    void showResult(const char *text);
    static void taskClicked(lv_event_t *event);
    static void noteClicked(lv_event_t *event);

    TaskStore &tasks_;
    NoteStore &notes_;
    RTCService &rtc_;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *titleField_ = nullptr;
    lv_obj_t *detailField_ = nullptr;
    lv_obj_t *result_ = nullptr;
};
