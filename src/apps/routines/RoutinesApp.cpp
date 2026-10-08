#include "RoutinesApp.h"
#include "hardware/HapticService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

void RoutinesApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(root_, 5, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_scrollable(root_, false);
    if (view_ == View::Editor && hasDraft_) showEditor(editingId_);
    else if (view_ == View::Run && runningId_) showRun(runningId_, false);
    else showList();
}

void RoutinesApp::suspend() { if (view_ == View::Editor) captureEditor(); }
void RoutinesApp::destroy() {
    if (root_) { lv_obj_delete(root_); root_ = nullptr; }
    titleField_ = stepsField_ = nullptr;
}

void RoutinesApp::showList() {
    view_ = View::List; hasDraft_ = false; lv_obj_clean(root_);
    lv_obj_t *heading = lv_label_create(root_); lv_label_set_text(heading, "ROUTINES");
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0); lv_obj_set_pos(heading, 5, 5);
    lv_obj_t *add = FormWidgets::button(root_, "+ ROUTINE", 355, 0, 114, 34, true);
    lv_obj_add_event_cb(add, addClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *list = lv_obj_create(root_); lv_obj_set_pos(list, 0, 40); lv_obj_set_size(list, 469, 144);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_all(list, 3, 0);
    lv_obj_set_style_pad_row(list, 4, 0); lv_obj_set_style_border_width(list, 0, 0);
    if (!store_.count()) {
        lv_obj_t *empty = lv_label_create(list); lv_label_set_text(empty, "No routines - press + ROUTINE");
        lv_obj_set_style_text_color(empty, Theme::color(0x6C716F), 0); lv_obj_center(empty); return;
    }
    for (size_t i = 0; i < store_.count(); ++i) {
        const auto &routine = store_.at(i);
        lv_obj_t *row = lv_obj_create(list); lv_obj_set_width(row, LV_PCT(100)); lv_obj_set_height(row, 42);
        lv_obj_set_style_pad_all(row, 2, 0); lv_obj_set_style_border_width(row, 0, 0); lv_obj_set_scrollable(row, false);
        lv_obj_t *run = lv_button_create(row); Theme::styleButton(run, i == 0); lv_obj_set_size(run, 365, 36); lv_obj_align(run, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_user_data(run, reinterpret_cast<void *>(static_cast<uintptr_t>(routine.id))); lv_obj_add_event_cb(run, runClicked, LV_EVENT_CLICKED, this);
        char label[64]; snprintf(label, sizeof(label), "%s  (%u steps)", routine.title, routine.stepCount);
        lv_obj_t *text = lv_label_create(run); lv_label_set_text(text, label); lv_obj_align(text, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t *edit = FormWidgets::button(row, "EDIT", 373, 1, 84, 34); lv_obj_set_user_data(edit, reinterpret_cast<void *>(static_cast<uintptr_t>(routine.id)));
        lv_obj_add_event_cb(edit, editClicked, LV_EVENT_CLICKED, this);
    }
}

void RoutinesApp::showEditor(uint32_t id) {
    view_ = View::Editor; editingId_ = id; lv_obj_clean(root_);
    if (!hasDraft_) {
        draft_ = {};
        if (id) { if (auto *found = store_.find(id)) draft_ = *found; }
        hasDraft_ = true;
    }
    titleField_ = FormWidgets::text(root_, "Routine name", 0, 0, 469, 38);
    stepsField_ = FormWidgets::text(root_, "One step per line (up to 8)", 0, 44, 469, 83, false);
    lv_textarea_set_text(titleField_, draft_.title);
    char steps[420]{};
    for (uint8_t i = 0; i < draft_.stepCount; ++i) {
        if (i) strlcat(steps, "\n", sizeof(steps));
        strlcat(steps, draft_.steps[i], sizeof(steps));
    }
    lv_textarea_set_text(stepsField_, steps);
    lv_obj_t *save = FormWidgets::button(root_, "SAVE", 0, 134, 120, 42, true); lv_obj_add_event_cb(save, saveClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *back = FormWidgets::button(root_, "BACK", 128, 134, 100, 42); lv_obj_add_event_cb(back, backClicked, LV_EVENT_CLICKED, this);
    if (id) { lv_obj_t *del = FormWidgets::button(root_, "DELETE", 349, 134, 120, 42); lv_obj_add_event_cb(del, deleteClicked, LV_EVENT_CLICKED, this); }
    lv_group_focus_obj(titleField_);
}

void RoutinesApp::captureEditor() {
    if (!titleField_ || !stepsField_) return;
    strlcpy(draft_.title, lv_textarea_get_text(titleField_), sizeof(draft_.title));
    draft_.stepCount = 0;
    char buffer[420]; strlcpy(buffer, lv_textarea_get_text(stepsField_), sizeof(buffer));
    char *line = strtok(buffer, "\n");
    while (line && draft_.stepCount < 8) {
        while (*line == ' ' || *line == '\t') ++line;
        if (*line) strlcpy(draft_.steps[draft_.stepCount++], line, sizeof(draft_.steps[0]));
        line = strtok(nullptr, "\n");
    }
    hasDraft_ = true;
}

void RoutinesApp::showRun(uint32_t id, bool reset) {
    RoutineRecord *routine = store_.find(id);
    if (!routine) { showList(); return; }
    view_ = View::Run; runningId_ = id; if (reset) currentStep_ = 0; lv_obj_clean(root_);
    lv_obj_t *title = lv_label_create(root_); lv_label_set_text(title, routine->title); lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_width(title, 360); lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);
    char progress[32]; snprintf(progress, sizeof(progress), "%u / %u", routine->stepCount ? currentStep_ + 1 : 0, routine->stepCount);
    lv_obj_t *counter = lv_label_create(root_); lv_label_set_text(counter, progress); lv_obj_set_style_text_color(counter, Theme::color(0x1E6675), 0); lv_obj_align(counter, LV_ALIGN_TOP_RIGHT, -4, 7);
    lv_obj_t *card = lv_obj_create(root_); Theme::stylePanel(card); lv_obj_set_size(card, 445, 88); lv_obj_align(card, LV_ALIGN_CENTER, 0, -4); lv_obj_set_scrollable(card, false);
    lv_obj_t *step = lv_label_create(card); lv_obj_set_width(step, LV_PCT(100)); lv_obj_set_style_text_align(step, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(step, &lv_font_montserrat_20, 0); lv_label_set_long_mode(step, LV_LABEL_LONG_WRAP);
    lv_label_set_text(step, routine->stepCount ? routine->steps[currentStep_] : "Add steps before running this routine."); lv_obj_center(step);
    lv_obj_t *back = FormWidgets::button(root_, "BACK", 0, 143, 95, 38); lv_obj_add_event_cb(back, backClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *restart = FormWidgets::button(root_, "RESTART", 104, 143, 105, 38); lv_obj_add_event_cb(restart, restartClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *next = FormWidgets::button(root_, currentStep_ + 1 < routine->stepCount ? "DONE / NEXT" : "FINISH", 286, 143, 183, 38, true);
    lv_obj_add_event_cb(next, nextClicked, LV_EVENT_CLICKED, this); lv_group_focus_obj(next);
}

void RoutinesApp::addClicked(lv_event_t *event) { auto *self = static_cast<RoutinesApp *>(lv_event_get_user_data(event)); self->hasDraft_ = false; self->showEditor(0); }
void RoutinesApp::runClicked(lv_event_t *event) { auto *self = static_cast<RoutinesApp *>(lv_event_get_user_data(event)); self->showRun(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))))); }
void RoutinesApp::editClicked(lv_event_t *event) { auto *self = static_cast<RoutinesApp *>(lv_event_get_user_data(event)); self->hasDraft_ = false; self->showEditor(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))))); }
void RoutinesApp::saveClicked(lv_event_t *event) { auto *self = static_cast<RoutinesApp *>(lv_event_get_user_data(event)); self->captureEditor(); if (self->draft_.title[0] && self->draft_.stepCount) { self->draft_.id = self->editingId_; self->store_.upsert(self->draft_); self->showList(); } }
void RoutinesApp::deleteClicked(lv_event_t *event) { auto *self = static_cast<RoutinesApp *>(lv_event_get_user_data(event)); self->store_.remove(self->editingId_); self->showList(); }
void RoutinesApp::backClicked(lv_event_t *event) { static_cast<RoutinesApp *>(lv_event_get_user_data(event))->showList(); }
void RoutinesApp::restartClicked(lv_event_t *event) { auto *self = static_cast<RoutinesApp *>(lv_event_get_user_data(event)); self->showRun(self->runningId_, true); }
void RoutinesApp::nextClicked(lv_event_t *event) {
    auto *self = static_cast<RoutinesApp *>(lv_event_get_user_data(event));
    auto *routine = self->store_.find(self->runningId_); if (!routine) { self->showList(); return; }
    self->haptic_.play(47);
    if (self->currentStep_ + 1 >= routine->stepCount) { self->showList(); return; }
    ++self->currentStep_; self->showRun(self->runningId_, false);
}
