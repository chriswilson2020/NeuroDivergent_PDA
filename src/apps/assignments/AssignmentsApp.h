#pragma once
#include "core/App.h"
#include "data/AssignmentStore.h"
#include <stdint.h>

class RTCService;
class HapticService;

class AssignmentsApp : public App {
public:
    AssignmentsApp(AssignmentStore &store, RTCService &rtc, HapticService &haptic) : store_(store), rtc_(rtc), haptic_(haptic) {}
    const char *id() const override { return "assignments"; }
    const char *title() const override { return "Assignments"; }
    void create(lv_obj_t *parent) override;
    void resume() override {}
    void suspend() override;
    void destroy() override;
    lv_obj_t *root() const override { return root_; }

private:
    enum class View : uint8_t { List, Editor };
    void showList(uint32_t preferredId = 0);
    void showEditor(uint32_t id);
    void captureEditor();
    uint32_t bestNext(uint32_t preferredId) const;
    static void addClicked(lv_event_t *event);
    static void itemClicked(lv_event_t *event);
    static void editClicked(lv_event_t *event);
    static void doneClicked(lv_event_t *event);
    static void saveClicked(lv_event_t *event);
    static void backClicked(lv_event_t *event);
    static void deleteClicked(lv_event_t *event);

    AssignmentStore &store_;
    RTCService &rtc_;
    HapticService &haptic_;
    View view_ = View::List;
    uint32_t editingId_ = 0;
    AssignmentRecord draft_{};
    bool hasDraft_ = false;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *titleField_ = nullptr;
    lv_obj_t *dueField_ = nullptr;
    lv_obj_t *effortDropdown_ = nullptr;
    lv_obj_t *priorityDropdown_ = nullptr;
    lv_obj_t *stepsField_ = nullptr;
};
