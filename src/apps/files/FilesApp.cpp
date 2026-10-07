#include "FilesApp.h"
#include "hardware/SPIBusManager.h"
#include "hardware/StorageService.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <SD.h>
#include <cstdio>
#include <cstring>

void FilesApp::create(lv_obj_t *parent) { root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100)); lv_obj_set_style_pad_all(root_, 5, 0); lv_obj_set_style_border_width(root_, 0, 0); lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_scrollable(root_, false); if (view_ == View::Preview && previewPath_[0]) showPreview(strrchr(previewPath_, '/') ? strrchr(previewPath_, '/') + 1 : previewPath_); else showList(); }
void FilesApp::destroy() { if (root_) { lv_obj_delete(root_); root_ = nullptr; } }
void FilesApp::makePath(const char *name, char *output, size_t capacity) const { snprintf(output, capacity, "%s/%s", currentPath_, name ? name : ""); }
void FilesApp::loadEntries() {
    count_ = 0; if (!storage_.mounted()) return; SPIBusManager::Guard guard(bus_); if (!guard) return; File directory = SD.open(currentPath_); if (!directory || !directory.isDirectory()) { if (directory) directory.close(); return; }
    while (count_ < 32) { File file = directory.openNextFile(); if (!file) break; const char *full = file.name(); const char *base = strrchr(full, '/'); base = base ? base + 1 : full; if (base[0] && strcmp(base, ".") && strcmp(base, "..")) { strlcpy(entries_[count_].name, base, sizeof(entries_[count_].name)); entries_[count_].directory = file.isDirectory(); entries_[count_].size = entries_[count_].directory ? 0 : file.size(); ++count_; } file.close(); }
    directory.close();
}
void FilesApp::showList() {
    view_ = View::List; previewPath_[0] = 0; lv_obj_clean(root_); loadEntries();
    lv_obj_t *path = lv_label_create(root_); lv_label_set_text(path, currentPath_); lv_obj_set_width(path, 350); lv_obj_set_style_text_font(path, &lv_font_montserrat_14, 0); lv_obj_set_pos(path, 5, 7);
    if (strcmp(currentPath_, "/PocketPDA/files")) { lv_obj_t *up = FormWidgets::button(root_, "UP", 379, 0, 90, 34); lv_obj_add_event_cb(up, upClicked, LV_EVENT_CLICKED, this); }
    lv_obj_t *list = lv_obj_create(root_); lv_obj_set_pos(list, 0, 40); lv_obj_set_size(list, 469, 144); lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_all(list, 3, 0); lv_obj_set_style_pad_row(list, 4, 0); lv_obj_set_style_border_width(list, 0, 0);
    if (!storage_.mounted()) { lv_obj_t *label = lv_label_create(list); lv_label_set_text(label, "microSD not mounted"); lv_obj_center(label); return; }
    if (!count_) { lv_obj_t *label = lv_label_create(list); lv_label_set_text(label, "Folder is empty\nCopy files to /PocketPDA/files"); lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(label); return; }
    for (size_t i = 0; i < count_; ++i) { lv_obj_t *button = lv_button_create(list); Theme::styleButton(button, i == 0); lv_obj_set_width(button, LV_PCT(100)); lv_obj_set_height(button, 38); lv_obj_set_user_data(button, reinterpret_cast<void *>(static_cast<uintptr_t>(i + 1))); lv_obj_add_event_cb(button, entryClicked, LV_EVENT_CLICKED, this); char line[96]; if (entries_[i].directory) snprintf(line, sizeof(line), "[DIR]  %s", entries_[i].name); else snprintf(line, sizeof(line), "%-38s %lu B", entries_[i].name, static_cast<unsigned long>(entries_[i].size)); lv_obj_t *label = lv_label_create(button); lv_label_set_text(label, line); lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0); lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0); }
}
void FilesApp::showPreview(const char *name) {
    view_ = View::Preview; if (!previewPath_[0]) makePath(name, previewPath_, sizeof(previewPath_)); preview_[0] = 0; uint32_t fileSize = 0;
    if (storage_.mounted()) { SPIBusManager::Guard guard(bus_); if (guard) { File file = SD.open(previewPath_, FILE_READ); if (file) { fileSize = file.size(); size_t read = file.read(reinterpret_cast<uint8_t *>(preview_), sizeof(preview_) - 1); preview_[read] = 0; file.close(); } } }
    lv_obj_clean(root_); const char *base = strrchr(previewPath_, '/'); base = base ? base + 1 : previewPath_; lv_obj_t *title = lv_label_create(root_); char heading[100]; snprintf(heading, sizeof(heading), "%s  (%lu B)", base, static_cast<unsigned long>(fileSize)); lv_label_set_text(title, heading); lv_obj_set_width(title, 315); lv_obj_set_pos(title, 5, 7);
    lv_obj_t *back = FormWidgets::button(root_, "BACK", 323, 0, 70, 34); lv_obj_add_event_cb(back, backClicked, LV_EVENT_CLICKED, this); lv_obj_t *remove = FormWidgets::button(root_, "DELETE", 399, 0, 70, 34); lv_obj_add_event_cb(remove, deleteClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *text = lv_textarea_create(root_); lv_obj_set_pos(text, 0, 40); lv_obj_set_size(text, 469, 144); lv_textarea_set_text(text, preview_[0] ? preview_ : "Empty file"); lv_textarea_set_one_line(text, false); lv_obj_set_click_focusable(text, false); lv_obj_add_state(text, LV_STATE_DISABLED); lv_obj_set_style_text_font(text, &lv_font_montserrat_12, 0);
}
void FilesApp::goUp() {
    const size_t rootLength = strlen("/PocketPDA/files");
    char *slash = strrchr(currentPath_, '/');
    if (slash && slash >= currentPath_ + rootLength) *slash = 0;
    showList();
}
void FilesApp::entryClicked(lv_event_t *event) { auto *self = static_cast<FilesApp *>(lv_event_get_user_data(event)); size_t encoded = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))); if (!encoded || encoded - 1 >= self->count_) return; Entry &entry = self->entries_[encoded - 1]; if (entry.directory) { char next[sizeof(self->currentPath_)]; self->makePath(entry.name, next, sizeof(next)); strlcpy(self->currentPath_, next, sizeof(self->currentPath_)); self->showList(); } else self->showPreview(entry.name); }
void FilesApp::upClicked(lv_event_t *event) { static_cast<FilesApp *>(lv_event_get_user_data(event))->goUp(); }
void FilesApp::backClicked(lv_event_t *event) { static_cast<FilesApp *>(lv_event_get_user_data(event))->showList(); }
void FilesApp::deleteClicked(lv_event_t *event) { auto *self = static_cast<FilesApp *>(lv_event_get_user_data(event)); if (self->storage_.mounted()) { SPIBusManager::Guard guard(self->bus_); if (guard) SD.remove(self->previewPath_); } self->showList(); }
