#include "CaptureApp.h"
#include "data/NoteStore.h"
#include "data/TaskStore.h"
#include "hardware/RTCService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <cstring>

void CaptureApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(root_, 5, 0); lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_scrollable(root_, false);
    lv_obj_t *heading = lv_label_create(root_); lv_label_set_text(heading, "QUICK CAPTURE");
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0); lv_obj_set_pos(heading, 4, 4);
    result_ = lv_label_create(root_); lv_obj_set_width(result_, 230); lv_obj_set_style_text_align(result_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(result_, Theme::color(0x1E6675), 0); lv_obj_align(result_, LV_ALIGN_TOP_RIGHT, -4, 8);
    titleField_ = FormWidgets::text(root_, "What do you need to remember?", 0, 38, 469, 40);
    detailField_ = FormWidgets::text(root_, "Optional details", 0, 84, 469, 52, false);
    lv_obj_t *task = FormWidgets::button(root_, "SAVE AS TASK", 0, 143, 225, 38, true); lv_obj_add_event_cb(task, taskClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *note = FormWidgets::button(root_, "SAVE AS NOTE", 244, 143, 225, 38); lv_obj_add_event_cb(note, noteClicked, LV_EVENT_CLICKED, this);
    lv_group_focus_obj(titleField_);
}

void CaptureApp::destroy() {
    if (root_) { lv_obj_delete(root_); root_ = nullptr; }
    titleField_ = detailField_ = result_ = nullptr;
}

void CaptureApp::showResult(const char *text) {
    lv_label_set_text(result_, text);
    lv_textarea_set_text(titleField_, ""); lv_textarea_set_text(detailField_, "");
    lv_group_focus_obj(titleField_);
}

void CaptureApp::saveTask() {
    const char *title = lv_textarea_get_text(titleField_); if (!title || !title[0]) return;
    TaskRecord task{}; strlcpy(task.title, title, sizeof(task.title));
    strlcpy(task.notes, lv_textarea_get_text(detailField_), sizeof(task.notes)); task.priority = 1;
    struct tm now{}; rtc_.now(now); task.dueYear = now.tm_year + 1900; task.dueMonth = now.tm_mon + 1; task.dueDay = now.tm_mday;
    showResult(tasks_.upsert(task) ? "Saved to Tasks" : "Could not save task");
}

void CaptureApp::saveNote() {
    const char *title = lv_textarea_get_text(titleField_); if (!title || !title[0]) return;
    NoteRecord note{}; strlcpy(note.title, title, sizeof(note.title));
    showResult(notes_.upsert(note, lv_textarea_get_text(detailField_)) ? "Saved to Notes" : "Could not save note");
}

void CaptureApp::taskClicked(lv_event_t *event) { static_cast<CaptureApp *>(lv_event_get_user_data(event))->saveTask(); }
void CaptureApp::noteClicked(lv_event_t *event) { static_cast<CaptureApp *>(lv_event_get_user_data(event))->saveNote(); }
