#pragma once
#include "core/App.h"
#include "data/RoutineStore.h"

class HapticService;

class RoutinesApp : public App {
public:
    RoutinesApp(RoutineStore &store, HapticService &haptic) : store_(store), haptic_(haptic) {}
    const char *id() const override { return "routines"; }
    const char *title() const override { return "Routines"; }
    void create(lv_obj_t *parent) override;
    void resume() override {}
    void suspend() override;
    void destroy() override;
    lv_obj_t *root() const override { return root_; }

private:
    enum class View : uint8_t { List, Editor, Run };
    void showList();
    void showEditor(uint32_t id);
    void showRun(uint32_t id, bool reset = true);
    void captureEditor();
    static void addClicked(lv_event_t *event);
    static void runClicked(lv_event_t *event);
    static void editClicked(lv_event_t *event);
    static void saveClicked(lv_event_t *event);
    static void deleteClicked(lv_event_t *event);
    static void backClicked(lv_event_t *event);
    static void nextClicked(lv_event_t *event);
    static void restartClicked(lv_event_t *event);

    RoutineStore &store_;
    HapticService &haptic_;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *titleField_ = nullptr;
    lv_obj_t *stepsField_ = nullptr;
    View view_ = View::List;
    RoutineRecord draft_{};
    uint32_t editingId_ = 0;
    uint32_t runningId_ = 0;
    uint8_t currentStep_ = 0;
    bool hasDraft_ = false;
};
