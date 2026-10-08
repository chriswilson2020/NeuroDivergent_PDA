#include "SettingsApp.h"
#include "core/BackupService.h"
#include "core/PowerManager.h"
#include "hardware/HardwareManager.h"
#include "hardware/RTCService.h"
#include "hardware/UsbDiskService.h"
#include "ui/FormWidgets.h"
#include "ui/Shell.h"
#include "ui/Theme.h"
#include <SD.h>
#include <cstdio>
#include <time.h>

namespace {
uint16_t selectedDim(uint32_t index) { static const uint16_t values[] = {15, 30, 60}; return values[index < 3 ? index : 1]; }
uint16_t selectedSleep(uint32_t index) { static const uint16_t values[] = {120, 300, 600}; return values[index < 3 ? index : 0]; }
uint8_t selectedBrightness(uint32_t index) { static const uint8_t values[] = {6, 12, 16}; return values[index < 3 ? index : 1]; }
uint32_t nearestIndex(uint16_t value, const uint16_t *values) { uint32_t best = 0; uint16_t distance = 0xFFFF; for (uint32_t i = 0; i < 3; ++i) { uint16_t next = value > values[i] ? value - values[i] : values[i] - value; if (next < distance) { distance = next; best = i; } } return best; }
void caption(lv_obj_t *parent, const char *text, int x, int y) { lv_obj_t *label = lv_label_create(parent); lv_label_set_text(label, text); lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0); lv_obj_set_style_text_color(label, Theme::color(0x6C716F), 0); lv_obj_set_pos(label, x, y); }
}

void SettingsApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100)); lv_obj_set_style_pad_all(root_, 5, 0); lv_obj_set_style_border_width(root_, 0, 0); lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_scrollable(root_, false);
    struct tm now{}; rtc_.now(now); char date[16], time[10]; strftime(date, sizeof(date), "%Y-%m-%d", &now); strftime(time, sizeof(time), "%H:%M:%S", &now);
    caption(root_, "DATE", 2, 0); caption(root_, "TIME", 157, 0); caption(root_, "BRIGHTNESS", 252, 0);
    date_ = FormWidgets::text(root_, "YYYY-MM-DD", 0, 17, 150, 35); time_ = FormWidgets::text(root_, "HH:MM:SS", 155, 17, 90, 35); lv_textarea_set_text(date_, date); lv_textarea_set_text(time_, time);
    brightness_ = lv_dropdown_create(root_); lv_dropdown_set_options(brightness_, "Low\nNormal\nHigh"); lv_obj_set_pos(brightness_, 250, 17); lv_obj_set_size(brightness_, 110, 35); const uint16_t brightnessValues[] = {6,12,16}; lv_dropdown_set_selected(brightness_, nearestIndex(store_.value().brightness, brightnessValues));
    caption(root_, "DIM AFTER", 2, 57); caption(root_, "DISPLAY OFF", 157, 57);
    dim_ = lv_dropdown_create(root_); lv_dropdown_set_options(dim_, "15 sec\n30 sec\n60 sec"); lv_obj_set_pos(dim_, 0, 74); lv_obj_set_size(dim_, 145, 35); const uint16_t dimValues[] = {15,30,60}; lv_dropdown_set_selected(dim_, nearestIndex(store_.value().dimSeconds, dimValues));
    sleep_ = lv_dropdown_create(root_); lv_dropdown_set_options(sleep_, "2 min\n5 min\n10 min"); lv_obj_set_pos(sleep_, 155, 74); lv_obj_set_size(sleep_, 145, 35); const uint16_t sleepValues[] = {120,300,600}; lv_dropdown_set_selected(sleep_, nearestIndex(store_.value().sleepSeconds, sleepValues));
    lv_obj_t *storage = lv_label_create(root_); if (hardware_.storage.mounted()) { SPIBusManager::Guard guard(hardware_.spi); if (guard) lv_label_set_text_fmt(storage, "microSD: %llu MB", static_cast<unsigned long long>(SD.cardSize() / 1024 / 1024)); else lv_label_set_text(storage, "microSD: busy"); } else lv_label_set_text(storage, "microSD: not mounted"); lv_obj_set_style_text_font(storage, &lv_font_montserrat_12, 0); lv_obj_set_pos(storage, 315, 72);
    lv_obj_t *saveButton = FormWidgets::button(root_, "SAVE", 0, 130, 78, 45, true); lv_obj_add_event_cb(saveButton, saveClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *backupButton = FormWidgets::button(root_, "BACKUP", 84, 130, 88, 45); lv_obj_add_event_cb(backupButton, backupClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *restoreButton = FormWidgets::button(root_, "RESTORE", 178, 130, 91, 45); lv_obj_add_event_cb(restoreButton, restoreClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *usbButton = FormWidgets::button(root_, "USB DISK", 275, 130, 88, 45); lv_obj_add_event_cb(usbButton, usbDiskClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *shutdownButton = FormWidgets::button(root_, "POWER", 369, 130, 100, 45); lv_obj_add_event_cb(shutdownButton, shutdownClicked, LV_EVENT_CLICKED, this);
}
void SettingsApp::destroy() { if (root_) { lv_obj_delete(root_); root_ = nullptr; } date_ = time_ = brightness_ = dim_ = sleep_ = nullptr; }
void SettingsApp::save() {
    int year, month, day, hour, minute, second; if (sscanf(lv_textarea_get_text(date_), "%d-%d-%d", &year, &month, &day) != 3 || sscanf(lv_textarea_get_text(time_), "%d:%d:%d", &hour, &minute, &second) != 3 || year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) { shell_.notifications().show("INVALID DATE/TIME", "Use YYYY-MM-DD and HH:MM:SS"); return; }
    struct tm value{}; value.tm_year = year - 1900; value.tm_mon = month - 1; value.tm_mday = day; value.tm_hour = hour; value.tm_min = minute; value.tm_sec = second; value.tm_isdst = -1; mktime(&value);
    if (value.tm_year != year - 1900 || value.tm_mon != month - 1 || value.tm_mday != day) { shell_.notifications().show("INVALID DATE", "That day does not exist in the selected month."); return; }
    rtc_.set(value);
    DeviceSettings settings{}; settings.brightness = selectedBrightness(lv_dropdown_get_selected(brightness_)); settings.dimBrightness = 2; settings.dimSeconds = selectedDim(lv_dropdown_get_selected(dim_)); settings.sleepSeconds = selectedSleep(lv_dropdown_get_selected(sleep_)); store_.save(settings); power_.setConfig(store_.powerConfig()); shell_.notifications().show("SETTINGS SAVED", "Clock, brightness, and timeouts updated.");
}
void SettingsApp::saveClicked(lv_event_t *event) { static_cast<SettingsApp *>(lv_event_get_user_data(event))->save(); }
void SettingsApp::backupClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); if (self->backup_.create()) self->shell_.notifications().show("BACKUP READY", "Saved as PocketPDA-Backup.ppb. Use USB Disk Mode to copy it to a computer."); else self->shell_.notifications().show("BACKUP FAILED", self->backup_.lastError()); }
void SettingsApp::restoreClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); self->shell_.notifications().show("RESTORE BACKUP?", "Current organizer data will be replaced only after the backup passes validation.", "RESTORE", confirmRestore, self); }
void SettingsApp::confirmRestore(void *context) { auto *self = static_cast<SettingsApp *>(context); if (self->backup_.restore()) self->shell_.notifications().show("RESTORE COMPLETE", "Organizer data, routines, timers, files, and settings were restored."); else self->shell_.notifications().show("RESTORE FAILED", self->backup_.lastError()); }
void SettingsApp::usbDiskClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); if (!self->usbDisk_.begin(nullptr, nullptr)) self->shell_.notifications().show("USB DISK UNAVAILABLE", self->usbDisk_.lastError()); }
void SettingsApp::shutdownClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); self->shell_.notifications().show("SHUT DOWN?", "Disconnect USB-C first.\nPress SHUT DOWN to confirm.", "SHUT DOWN", confirmShutdown, self); }
void SettingsApp::confirmShutdown(void *context) { auto *self = static_cast<SettingsApp *>(context); if (!self->hardware_.shutdown()) { self->hardware_.setBrightness(self->power_.config().activeBrightness); self->shell_.notifications().show("USB-C CONNECTED", "Disconnect USB-C before shutting down."); } }
