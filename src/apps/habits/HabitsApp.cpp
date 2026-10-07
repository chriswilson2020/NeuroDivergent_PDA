#include "HabitsApp.h"
#include "hardware/HapticService.h"
#include "hardware/RTCService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace {
uint32_t dateCode(const struct tm &value) {
    return static_cast<uint32_t>(value.tm_year + 1900) * 10000u +
           static_cast<uint32_t>(value.tm_mon + 1) * 100u + static_cast<uint32_t>(value.tm_mday);
}

lv_obj_t *circle(lv_obj_t *parent, int x, int y, int size, uint32_t color) {
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, size, size);
    lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(object, Theme::color(color), 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_scrollable(object, false);
    return object;
}
}

void HabitsApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(root_, 5, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_scrollable(root_, false);
    refreshDate();
    showDashboard();
}

void HabitsApp::resume() {
    refreshDate();
    if (view_ == View::Dashboard && root_) refreshVisuals();
}

void HabitsApp::destroy() {
    if (animationTimer_) { lv_timer_delete(animationTimer_); animationTimer_ = nullptr; }
    if (root_) { lv_obj_delete(root_); root_ = nullptr; }
    petBody_ = leftEye_ = rightEye_ = mouth_ = heart_ = petMessage_ = progress_ = nameField_ = nullptr;
    memset(habitLabels_, 0, sizeof(habitLabels_));
    memset(streakLabels_, 0, sizeof(streakLabels_));
}

void HabitsApp::refreshDate() {
    struct tm now{};
    rtc_.now(now);
    hour_ = now.tm_hour;
    today_ = dateCode(now);
    struct tm previous = now;
    previous.tm_mday -= 1;
    previous.tm_isdst = -1;
    mktime(&previous);
    yesterday_ = dateCode(previous);
}

void HabitsApp::showDashboard() {
    view_ = View::Dashboard;
    editingId_ = 0;
    if (animationTimer_) { lv_timer_delete(animationTimer_); animationTimer_ = nullptr; }
    lv_obj_clean(root_);
    memset(habitLabels_, 0, sizeof(habitLabels_));
    memset(streakLabels_, 0, sizeof(streakLabels_));

    lv_obj_t *petPanel = lv_obj_create(root_);
    Theme::stylePanel(petPanel);
    lv_obj_set_pos(petPanel, 0, 0);
    lv_obj_set_size(petPanel, 170, 184);
    lv_obj_set_style_pad_all(petPanel, 4, 0);
    lv_obj_set_scrollable(petPanel, false);

    lv_obj_t *petName = lv_label_create(petPanel);
    lv_label_set_text(petName, "MOCHI");
    lv_obj_set_style_text_font(petName, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(petName, Theme::color(0x1E6675), 0);
    lv_obj_align(petName, LV_ALIGN_TOP_MID, 0, 0);

    petBody_ = circle(petPanel, 41, 27, 80, 0x7CCBC3);
    lv_obj_set_style_border_width(petBody_, 3, 0);
    lv_obj_set_style_border_color(petBody_, Theme::color(0x275C63), 0);
    circle(petBody_, 7, 4, 25, 0x7CCBC3);
    circle(petBody_, 48, 4, 25, 0x7CCBC3);
    leftEye_ = circle(petBody_, 22, 31, 9, 0x172128);
    rightEye_ = circle(petBody_, 49, 31, 9, 0x172128);
    circle(petBody_, 13, 48, 10, 0xF29BA2);
    circle(petBody_, 57, 48, 10, 0xF29BA2);
    mouth_ = lv_label_create(petBody_);
    lv_obj_set_style_text_font(mouth_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(mouth_, Theme::color(0x172128), 0);
    lv_obj_set_width(mouth_, 30);
    lv_obj_set_style_text_align(mouth_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(mouth_, 25, 43);
    heart_ = lv_label_create(petPanel);
    lv_label_set_text(heart_, "<3");
    lv_obj_set_style_text_font(heart_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(heart_, Theme::color(0xD84A63), 0);
    lv_obj_set_pos(heart_, 126, 29);

    petMessage_ = lv_label_create(petPanel);
    lv_obj_set_width(petMessage_, 158);
    lv_obj_set_style_text_align(petMessage_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(petMessage_, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(petMessage_, 1, 111);
    progress_ = lv_label_create(petPanel);
    lv_obj_set_width(progress_, 158);
    lv_obj_set_style_text_align(progress_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(progress_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(progress_, Theme::color(0x6C716F), 0);
    lv_obj_set_pos(progress_, 1, 135);

    lv_obj_t *heading = lv_label_create(root_);
    lv_label_set_text(heading, "DAILY HABITS");
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(heading, 181, 4);
    lv_obj_t *add = FormWidgets::button(root_, "+ HABIT", 382, 0, 87, 29, true);
    lv_obj_add_event_cb(add, addClicked, LV_EVENT_CLICKED, this);
    if (store_.count() >= HabitStore::kCapacity) lv_obj_add_state(add, LV_STATE_DISABLED);

    lv_obj_t *list = lv_obj_create(root_);
    lv_obj_set_pos(list, 176, 34);
    lv_obj_set_size(list, 293, 150);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 2, 0);
    lv_obj_set_style_pad_row(list, 3, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    if (!store_.count()) {
        lv_obj_t *empty = lv_label_create(list);
        lv_label_set_text(empty, "No habits yet - add one!");
        lv_obj_set_style_text_color(empty, Theme::color(0x6C716F), 0);
        lv_obj_center(empty);
    }
    for (size_t i = 0; i < store_.count(); ++i) {
        const HabitRecord &habit = store_.at(i);
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 37);
        lv_obj_set_style_pad_all(row, 1, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_scrollable(row, false);
        lv_obj_t *toggle = lv_button_create(row);
        Theme::styleButton(toggle, false);
        lv_obj_set_pos(toggle, 0, 0);
        lv_obj_set_size(toggle, 238, 34);
        lv_obj_set_user_data(toggle, reinterpret_cast<void *>(static_cast<uintptr_t>(habit.id)));
        lv_obj_add_event_cb(toggle, habitClicked, LV_EVENT_CLICKED, this);
        habitLabels_[i] = lv_label_create(toggle);
        lv_obj_set_style_text_font(habitLabels_[i], &lv_font_montserrat_12, 0);
        lv_obj_align(habitLabels_[i], LV_ALIGN_LEFT_MID, 0, 0);
        streakLabels_[i] = lv_label_create(toggle);
        lv_obj_set_style_text_font(streakLabels_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(streakLabels_[i], Theme::color(0xD88122), 0);
        lv_obj_align(streakLabels_[i], LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_t *edit = FormWidgets::button(row, "...", 244, 0, 41, 34);
        lv_obj_set_user_data(edit, reinterpret_cast<void *>(static_cast<uintptr_t>(habit.id)));
        lv_obj_add_event_cb(edit, editClicked, LV_EVENT_CLICKED, this);
    }
    refreshVisuals();
    animationTimer_ = lv_timer_create(animationTick, 160, this);
}

void HabitsApp::showEditor(uint32_t id) {
    view_ = View::Editor;
    editingId_ = id;
    if (animationTimer_) { lv_timer_delete(animationTimer_); animationTimer_ = nullptr; }
    lv_obj_clean(root_);
    lv_obj_t *heading = lv_label_create(root_);
    lv_label_set_text(heading, id ? "EDIT DAILY HABIT" : "NEW DAILY HABIT");
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(heading, 4, 4);
    lv_obj_t *hint = lv_label_create(root_);
    lv_label_set_text(hint, "Keep it small, kind, and achievable.");
    lv_obj_set_style_text_color(hint, Theme::color(0x6C716F), 0);
    lv_obj_set_pos(hint, 4, 36);
    nameField_ = FormWidgets::text(root_, "Habit name", 4, 63, 461, 42);
    lv_textarea_set_max_length(nameField_, 31);
    if (HabitRecord *habit = store_.find(id)) lv_textarea_set_text(nameField_, habit->name);
    lv_obj_t *save = FormWidgets::button(root_, "SAVE", 4, 124, 120, 45, true);
    lv_obj_add_event_cb(save, saveClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *back = FormWidgets::button(root_, "BACK", 132, 124, 105, 45);
    lv_obj_add_event_cb(back, backClicked, LV_EVENT_CLICKED, this);
    if (id) {
        lv_obj_t *remove = FormWidgets::button(root_, "DELETE", 345, 124, 120, 45);
        lv_obj_add_event_cb(remove, deleteClicked, LV_EVENT_CLICKED, this);
    }
    lv_group_focus_obj(nameField_);
}

HabitsApp::Mood HabitsApp::mood() const {
    if (celebrateTicks_) return Mood::Happy;
    if (!store_.count()) return Mood::Calm;
    size_t completed = 0;
    for (size_t i = 0; i < store_.count(); ++i) if (store_.completedToday(store_.at(i), today_)) ++completed;
    if (completed == store_.count()) return Mood::Happy;
    if (completed) return Mood::Proud;
    return hour_ >= 18 ? Mood::Sad : Mood::Calm;
}

void HabitsApp::refreshVisuals() {
    if (!root_ || view_ != View::Dashboard) return;
    size_t completed = 0;
    for (size_t i = 0; i < store_.count(); ++i) {
        const HabitRecord &habit = store_.at(i);
        const bool done = store_.completedToday(habit, today_);
        if (done) ++completed;
        if (habitLabels_[i]) {
            lv_label_set_text_fmt(habitLabels_[i], "%s %s", done ? "[x]" : "[ ]", habit.name);
            lv_obj_set_style_text_color(habitLabels_[i], Theme::color(done ? 0x1E6675 : 0x172128), 0);
        }
        if (streakLabels_[i]) {
            if (habit.streak > 1) lv_label_set_text_fmt(streakLabels_[i], "%ud", habit.streak);
            else lv_label_set_text(streakLabels_[i], "");
        }
    }
    lv_label_set_text_fmt(progress_, "%u / %u done today", static_cast<unsigned>(completed), static_cast<unsigned>(store_.count()));
    updatePet();
}

void HabitsApp::updatePet() {
    if (!petBody_) return;
    const Mood current = mood();
    uint32_t color = 0x7CCBC3;
    const char *mouth = "o";
    const char *message = "Ready when you are!";
    if (current == Mood::Proud) { color = 0x77C982; mouth = "u"; message = "We're doing it!"; }
    else if (current == Mood::Happy) { color = 0xF2B84B; mouth = "w"; message = "Best day ever!"; }
    else if (current == Mood::Sad) { color = 0x8FAAC9; mouth = "n"; message = "I missed you..."; }
    lv_obj_set_style_bg_color(petBody_, Theme::color(color), 0);
    // The first two children are Mochi's ears. Cheeks keep their pink color.
    for (uint32_t i = 0; i < 2; ++i) lv_obj_set_style_bg_color(lv_obj_get_child(petBody_, i), Theme::color(color), 0);
    lv_label_set_text(mouth_, mouth);
    lv_label_set_text(petMessage_, message);
    lv_obj_set_hidden(heart_, current != Mood::Happy);
}

void HabitsApp::habitClicked(lv_event_t *event) {
    auto *self = static_cast<HabitsApp *>(lv_event_get_user_data(event));
    const uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))));
    HabitRecord *habit = self->store_.find(id);
    const bool completing = habit && !self->store_.completedToday(*habit, self->today_);
    if (self->store_.toggleToday(id, self->today_, self->yesterday_)) {
        if (completing) { self->celebrateTicks_ = 18; self->haptic_.play(47); }
        self->refreshVisuals();
    }
}

void HabitsApp::editClicked(lv_event_t *event) {
    auto *self = static_cast<HabitsApp *>(lv_event_get_user_data(event));
    const uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))));
    self->showEditor(id);
}

void HabitsApp::addClicked(lv_event_t *event) {
    auto *self = static_cast<HabitsApp *>(lv_event_get_user_data(event));
    if (self->store_.count() < HabitStore::kCapacity) self->showEditor(0);
}

void HabitsApp::saveClicked(lv_event_t *event) {
    auto *self = static_cast<HabitsApp *>(lv_event_get_user_data(event));
    if (!self->nameField_) return;
    const char *name = lv_textarea_get_text(self->nameField_);
    if (!name || !name[0]) return;
    HabitRecord record{};
    record.id = self->editingId_;
    strlcpy(record.name, name, sizeof(record.name));
    if (self->store_.upsert(record)) self->showDashboard();
}

void HabitsApp::deleteClicked(lv_event_t *event) {
    auto *self = static_cast<HabitsApp *>(lv_event_get_user_data(event));
    if (self->store_.remove(self->editingId_)) self->showDashboard();
}

void HabitsApp::backClicked(lv_event_t *event) { static_cast<HabitsApp *>(lv_event_get_user_data(event))->showDashboard(); }

void HabitsApp::animationTick(lv_timer_t *timer) {
    auto *self = static_cast<HabitsApp *>(lv_timer_get_user_data(timer));
    if (!self || !self->petBody_) return;
    self->animationPhase_ = static_cast<uint8_t>((self->animationPhase_ + 1) % 32);
    if (self->celebrateTicks_) {
        --self->celebrateTicks_;
        if (!self->celebrateTicks_) self->refreshVisuals();
    }
    const Mood current = self->mood();
    int y = 27;
    if (current == Mood::Happy) y -= (self->animationPhase_ % 6 == 1 || self->animationPhase_ % 6 == 2) ? 4 : 0;
    else if (current == Mood::Proud) y -= self->animationPhase_ % 12 == 1 ? 2 : 0;
    else if (current == Mood::Sad) y += self->animationPhase_ % 16 < 8 ? 1 : 0;
    lv_obj_set_y(self->petBody_, y);
    const bool blink = self->animationPhase_ == 0 || self->animationPhase_ == 1;
    lv_obj_set_height(self->leftEye_, blink ? 2 : 9);
    lv_obj_set_height(self->rightEye_, blink ? 2 : 9);
    lv_obj_set_y(self->leftEye_, blink ? 35 : 31);
    lv_obj_set_y(self->rightEye_, blink ? 35 : 31);
    if (current == Mood::Happy) lv_obj_set_y(self->heart_, 25 - (self->animationPhase_ % 8));
}
