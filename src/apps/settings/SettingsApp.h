#pragma once
#include "core/App.h"
#include "data/SettingsStore.h"
class HardwareManager;
class PowerManager;
class RTCService;
class Shell;
class UsbDiskService;
class BackupService;

class SettingsApp : public App {
public:
    SettingsApp(SettingsStore &store, HardwareManager &hardware, PowerManager &power, RTCService &rtc, Shell &shell, UsbDiskService &usbDisk, BackupService &backup)
        : store_(store), hardware_(hardware), power_(power), rtc_(rtc), shell_(shell), usbDisk_(usbDisk), backup_(backup) {}
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
    SettingsStore &store_;
    HardwareManager &hardware_;
    PowerManager &power_;
    RTCService &rtc_;
    Shell &shell_;
    UsbDiskService &usbDisk_;
    BackupService &backup_;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *date_ = nullptr;
    lv_obj_t *time_ = nullptr;
    lv_obj_t *brightness_ = nullptr;
    lv_obj_t *dim_ = nullptr;
    lv_obj_t *sleep_ = nullptr;
};
