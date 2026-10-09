#pragma once
#include <stdint.h>
#include <lvgl.h>

class StorageService;
class SPIBusManager;

class UsbDiskService {
public:
    using FinishedCallback = void (*)(void *context, bool storageReady);
    using StartedCallback = void (*)(void *context);

    UsbDiskService(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    void setFinishedCallback(FinishedCallback callback, void *context) { callback_ = callback; callbackContext_ = context; }
    void setStartedCallback(StartedCallback callback, void *context) { startedCallback_ = callback; startedContext_ = context; }
    bool begin(FinishedCallback callback, void *context);
    void update();
    bool active() const { return active_; }
    const char *lastError() const { return lastError_; }

private:
    void createOverlay();
    void finish();
    void setInputsEnabled(bool enabled);

    StorageService &storage_;
    SPIBusManager &bus_;
    FinishedCallback callback_ = nullptr;
    StartedCallback startedCallback_ = nullptr;
    void *startedContext_ = nullptr;
    void *callbackContext_ = nullptr;
    lv_obj_t *overlay_ = nullptr;
    lv_obj_t *status_ = nullptr;
    lv_timer_t *refreshTimer_ = nullptr;
    uint32_t lastStatusUpdate_ = 0;
    bool active_ = false;
    const char *lastError_ = "USB Disk Mode is unavailable.";
};
