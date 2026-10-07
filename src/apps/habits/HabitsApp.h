#pragma once

#include "core/App.h"
#include "data/HabitStore.h"

class RTCService;
class HapticService;

class HabitsApp : public App {
public:
    HabitsApp(HabitStore &store, RTCService &rtc, HapticService &haptic)
        : store_(store), rtc_(rtc), haptic_(haptic) {}
    const char *id() const override { return "habits"; }
    const char *title() const override { return "Habits"; }
    void create(lv_obj_t *parent) override;
    void resume() override;
    void suspend() override {}
    void destroy() override;
    lv_obj_t *root() const override { return root_; }

private:
    enum class View : uint8_t { Dashboard, Editor };
    enum class Mood : uint8_t { Calm, Proud, Happy, Sad };

    void showDashboard();
    void showEditor(uint32_t id);
    void refreshVisuals();
    void refreshDate();
    void updatePet();
    Mood mood() const;
    static void habitClicked(lv_event_t *event);
    static void editClicked(lv_event_t *event);
    static void addClicked(lv_event_t *event);
    static void saveClicked(lv_event_t *event);
    static void deleteClicked(lv_event_t *event);
    static void backClicked(lv_event_t *event);
    static void animationTick(lv_timer_t *timer);

    HabitStore &store_;
    RTCService &rtc_;
    HapticService &haptic_;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *petBody_ = nullptr;
    lv_obj_t *leftEye_ = nullptr;
    lv_obj_t *rightEye_ = nullptr;
    lv_obj_t *mouth_ = nullptr;
    lv_obj_t *heart_ = nullptr;
    lv_obj_t *petMessage_ = nullptr;
    lv_obj_t *progress_ = nullptr;
    lv_obj_t *nameField_ = nullptr;
    lv_obj_t *habitLabels_[HabitStore::kCapacity]{};
    lv_obj_t *streakLabels_[HabitStore::kCapacity]{};
    lv_timer_t *animationTimer_ = nullptr;
    View view_ = View::Dashboard;
    uint32_t editingId_ = 0;
    uint32_t today_ = 0;
    uint32_t yesterday_ = 0;
    uint8_t hour_ = 0;
    uint8_t animationPhase_ = 0;
    uint8_t celebrateTicks_ = 0;
};
