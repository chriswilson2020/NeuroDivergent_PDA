#include "SettingsApp.h"
#include "core/BackupService.h"
#include "core/PowerManager.h"
#include "core/GnssTimeService.h"
#include "hardware/HardwareManager.h"
#include "hardware/RTCService.h"
#include "hardware/UsbDiskService.h"
#include "ui/FormWidgets.h"
#include "ui/FormFocus.h"
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
    auto *sync=FormWidgets::button(root_,"TIME SYNC",315,94,145,30);lv_obj_add_event_cb(sync,syncClicked,LV_EVENT_CLICKED,this);
    lv_obj_t *saveButton = FormWidgets::button(root_, "SAVE", 0, 130, 78, 45, true); lv_obj_add_event_cb(saveButton, saveClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *backupButton = FormWidgets::button(root_, "BACKUP", 84, 130, 88, 45); lv_obj_add_event_cb(backupButton, backupClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *restoreButton = FormWidgets::button(root_, "RESTORE", 178, 130, 91, 45); lv_obj_add_event_cb(restoreButton, restoreClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *usbButton = FormWidgets::button(root_, "USB DISK", 275, 130, 88, 45); lv_obj_add_event_cb(usbButton, usbDiskClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *shutdownButton = FormWidgets::button(root_, "POWER", 369, 130, 100, 45); lv_obj_add_event_cb(shutdownButton, shutdownClicked, LV_EVENT_CLICKED, this);
    FormFocus::enter(date_);
}
void SettingsApp::destroy() { if(syncTimer_){lv_timer_delete(syncTimer_);syncTimer_=nullptr;} if (root_) { lv_obj_delete(root_); root_ = nullptr; } date_ = time_ = brightness_ = dim_ = sleep_ = nullptr;syncAuto_=syncInterval_=syncZone_=syncStatus_=nullptr; }
void SettingsApp::save() {
    int year, month, day, hour, minute, second; if (sscanf(lv_textarea_get_text(date_), "%d-%d-%d", &year, &month, &day) != 3 || sscanf(lv_textarea_get_text(time_), "%d:%d:%d", &hour, &minute, &second) != 3 || year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) { shell_.notifications().show("INVALID DATE/TIME", "Use YYYY-MM-DD and HH:MM:SS"); return; }
    struct tm value{}; value.tm_year = year - 1900; value.tm_mon = month - 1; value.tm_mday = day; value.tm_hour = hour; value.tm_min = minute; value.tm_sec = second; value.tm_isdst = -1;
    if (value.tm_year != year - 1900 || value.tm_mon != month - 1 || value.tm_mday != day) { shell_.notifications().show("INVALID DATE", "That day does not exist in the selected month."); return; }
    gnss_.cancel();
    if(!rtc_.set(value)){shell_.notifications().show("CLOCK NOT SET","Invalid date, nonexistent DST time, or RTC readback failure.");return;}
    DeviceSettings settings{}; settings.brightness = selectedBrightness(lv_dropdown_get_selected(brightness_)); settings.dimBrightness = 2; settings.dimSeconds = selectedDim(lv_dropdown_get_selected(dim_)); settings.sleepSeconds = selectedSleep(lv_dropdown_get_selected(sleep_)); store_.save(settings); power_.setConfig(store_.powerConfig()); shell_.notifications().show("SETTINGS SAVED", "Clock, brightness, and timeouts updated.");
}
void SettingsApp::saveClicked(lv_event_t *event) { static_cast<SettingsApp *>(lv_event_get_user_data(event))->save(); }
void SettingsApp::backupClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); self->gnss_.cancel(); if (self->backup_.create()) self->shell_.notifications().show("BACKUP READY", "Saved as PocketPDA-Backup.ppb. Use USB Disk Mode to copy it to a computer."); else self->shell_.notifications().show("BACKUP FAILED", self->backup_.lastError()); }
void SettingsApp::restoreClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); self->shell_.notifications().show("RESTORE BACKUP?", "Current organizer data will be replaced only after the backup passes validation.", "RESTORE", confirmRestore, self); }
void SettingsApp::confirmRestore(void *context) { auto *self = static_cast<SettingsApp *>(context); self->gnss_.cancel(); if (self->backup_.restore()) self->shell_.notifications().show("RESTORE COMPLETE", "Organizer data, routines, timers, files, and settings were restored."); else self->shell_.notifications().show("RESTORE FAILED", self->backup_.lastError()); }
void SettingsApp::usbDiskClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); if (!self->usbDisk_.begin(nullptr, nullptr)) self->shell_.notifications().show("USB DISK UNAVAILABLE", self->usbDisk_.lastError()); }
void SettingsApp::shutdownClicked(lv_event_t *event) { auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event)); self->shell_.notifications().show("SHUT DOWN?", "Disconnect USB-C first.\nPress SHUT DOWN to confirm.", "SHUT DOWN", confirmShutdown, self); }
void SettingsApp::confirmShutdown(void *context) { auto *self = static_cast<SettingsApp *>(context); self->gnss_.cancel(); if (!self->hardware_.shutdown()) { self->hardware_.setBrightness(self->power_.config().activeBrightness); self->shell_.notifications().show("USB-C CONNECTED", "Disconnect USB-C before shutting down."); } }

void SettingsApp::syncClicked(lv_event_t *e){static_cast<SettingsApp*>(lv_event_get_user_data(e))->showSync();}
void SettingsApp::showSync() {
    lv_obj_clean(root_);caption(root_,"TIME SYNCHRONISATION",0,0);
    syncAuto_=lv_checkbox_create(root_);lv_checkbox_set_text(syncAuto_,"Auto");lv_obj_set_pos(syncAuto_,0,20);
    if(store_.timeSync().automatic)lv_obj_add_state(syncAuto_,LV_STATE_CHECKED);
    syncInterval_=lv_dropdown_create(root_);lv_obj_set_pos(syncInterval_,95,17);lv_obj_set_size(syncInterval_,130,32);
    lv_dropdown_set_options(syncInterval_,"1 hour\n4 hours\n8 hours\n12 hours\n24 hours");
    static const uint16_t intervals[]={1,4,8,12,24};
    for(unsigned i=0;i<5;++i)if(intervals[i]==store_.timeSync().intervalHours)lv_dropdown_set_selected(syncInterval_,i);
    syncZone_=lv_dropdown_create(root_);lv_obj_set_pos(syncZone_,235,17);lv_obj_set_size(syncZone_,224,32);
    lv_dropdown_set_options(syncZone_,"Europe/Amsterdam\nUTC");lv_dropdown_set_selected(syncZone_,store_.timeSync().timezone);
    syncStatus_=lv_label_create(root_);lv_obj_set_pos(syncStatus_,0,54);lv_obj_set_size(syncStatus_,469,86);
    lv_obj_set_style_text_font(syncStatus_,&lv_font_montserrat_12,0);
    auto *now=FormWidgets::button(root_,"SYNC NOW",0,144,126,33,true);lv_obj_add_event_cb(now,syncNowClicked,LV_EVENT_CLICKED,this);
    auto *cancel=FormWidgets::button(root_,"CANCEL",134,144,102,33);lv_obj_add_event_cb(cancel,syncCancelClicked,LV_EVENT_CLICKED,this);
    auto *save=FormWidgets::button(root_,"SAVE",244,144,102,33);lv_obj_add_event_cb(save,syncSaveClicked,LV_EVENT_CLICKED,this);
    auto *back=FormWidgets::button(root_,"BACK",354,144,110,33);lv_obj_add_event_cb(back,syncBackClicked,LV_EVENT_CLICKED,this);
    FormFocus::enter(now);
    syncTimer_=lv_timer_create(syncTick,1000,this);refreshSync();
}
void SettingsApp::refreshSync() {
    if(!syncStatus_)return;
    const auto &s=gnss_.status();const auto &h=s.history();
    auto date=[](int64_t epoch,char *out,size_t n){if(!epoch){snprintf(out,n,"Unknown / never");return;}time_t t=epoch;tm local{};localtime_r(&t,&local);strftime(out,n,"%d %b %H:%M:%S",&local);};
    char attempt[32],success[32],drift[32];date(h.attempted,attempt,sizeof(attempt));date(h.successful,success,sizeof(success));
    if(h.driftKnown)snprintf(drift,sizeof(drift),"%+lld seconds",h.drift);else snprintf(drift,sizeof(drift),"Unknown");
    lv_label_set_text_fmt(syncStatus_,"GNSS: %s  %lus  samples: %u | RTC: %s\nLast attempt: %s\nLast success: %s\nDrift RTC-GNSS: %s\n%s | Next eligible: %lum %lus%s",
        s.active()?"SEARCHING":"OFF",static_cast<unsigned long>(s.elapsedSeconds()),s.samples(),rtc_.trusted()?"OK":"needs time",attempt,success,drift,
        GnssTime::resultName(h.result),static_cast<unsigned long>(s.waitSeconds()/60),static_cast<unsigned long>(s.waitSeconds()%60),
        store_.timeSync().automatic?"":" (auto OFF)");
}
void SettingsApp::syncTick(lv_timer_t *t){static_cast<SettingsApp*>(lv_timer_get_user_data(t))->refreshSync();}
void SettingsApp::syncNowClicked(lv_event_t *e){auto *s=static_cast<SettingsApp*>(lv_event_get_user_data(e));if(!s->gnss_.syncNow())s->shell_.notifications().show("SYNC NOT STARTED",s->gnss_.startError());s->refreshSync();}
void SettingsApp::syncCancelClicked(lv_event_t *e){auto *s=static_cast<SettingsApp*>(lv_event_get_user_data(e));s->gnss_.cancel();s->refreshSync();}
void SettingsApp::syncSaveClicked(lv_event_t *e){
    auto *s=static_cast<SettingsApp*>(lv_event_get_user_data(e));TimeSyncPreferences p=s->store_.timeSync();
    static const uint16_t hours[]={1,4,8,12,24};p.automatic=lv_obj_has_state(s->syncAuto_,LV_STATE_CHECKED);
    p.intervalHours=hours[lv_dropdown_get_selected(s->syncInterval_)];p.timezone=lv_dropdown_get_selected(s->syncZone_);
    s->gnss_.cancel();const bool zoneChanged=p.timezone!=s->store_.timeSync().timezone;
    if(!s->store_.saveTimeSync(p)){s->shell_.notifications().show("SAVE FAILED","Time sync settings could not be saved.");return;}
    if(zoneChanged)s->rtc_.timezoneChanged();s->gnss_.preferencesChanged();s->refreshSync();
}
void SettingsApp::syncBackClicked(lv_event_t *e){auto *s=static_cast<SettingsApp*>(lv_event_get_user_data(e));lv_obj_t *parent=lv_obj_get_parent(s->root_);s->destroy();s->create(parent);}
