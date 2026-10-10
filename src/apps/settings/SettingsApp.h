#pragma once
#include "core/App.h"
#include "data/SettingsStore.h"
class HardwareManager;
class PowerManager;
class RTCService;
class Shell;
class UsbDiskService;
class BackupService;
class GnssTimeService;

class SettingsApp : public App {
public:
    SettingsApp(SettingsStore &store, HardwareManager &hardware, PowerManager &power, RTCService &rtc, Shell &shell, UsbDiskService &usbDisk, BackupService &backup, GnssTimeService &gnss)
        : store_(store), hardware_(hardware), power_(power), rtc_(rtc), shell_(shell), usbDisk_(usbDisk), backup_(backup), gnss_(gnss) {}
    const char *id() const override { return "settings"; }
    const char *title() const override { return "Settings"; }
    void create(lv_obj_t *parent) override;
    void resume() override {}
    void suspend() override {}
    void destroy() override;
    lv_obj_t *root() const override { return root_; }
private:
    static void saveClicked(lv_event_t *event);
    static void backupClicked(lv_event_t *event);
    static void restoreClicked(lv_event_t *event);
    static void confirmRestore(void *context);
    static void shutdownClicked(lv_event_t *event);
    static void usbDiskClicked(lv_event_t *event);
    static void confirmShutdown(void *context);
    void save();
    void showSync();
    void refreshSync();
    static void syncClicked(lv_event_t *);
    static void syncNowClicked(lv_event_t *);
    static void syncCancelClicked(lv_event_t *);
    static void syncSaveClicked(lv_event_t *);
    static void syncBackClicked(lv_event_t *);
    static void syncTick(lv_timer_t *);
    SettingsStore &store_;
    HardwareManager &hardware_;
    PowerManager &power_;
    RTCService &rtc_;
    Shell &shell_;
    UsbDiskService &usbDisk_;
    BackupService &backup_;
    GnssTimeService &gnss_;
    lv_obj_t *syncAuto_=nullptr,*syncInterval_=nullptr,*syncZone_=nullptr,*syncStatus_=nullptr;
    lv_timer_t *syncTimer_=nullptr;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *date_ = nullptr;
    lv_obj_t *time_ = nullptr;
    lv_obj_t *brightness_ = nullptr;
    lv_obj_t *dim_ = nullptr;
    lv_obj_t *sleep_ = nullptr;
};
