#include "UsbDiskService.h"
#include "SPIBusManager.h"
#include "StorageService.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <LilyGoLib.h>
#include <usb/USB_Service.h>

void UsbDiskService::setInputsEnabled(bool enabled) {
    lv_indev_t *input = nullptr;
    while ((input = lv_indev_get_next(input)) != nullptr) lv_indev_enable(input, enabled);
}

void UsbDiskService::createOverlay() {
    overlay_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(overlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay_, Theme::color(0xF2EFE7), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_, 0, 0);
    lv_obj_set_style_radius(overlay_, 0, 0);
    lv_obj_set_style_pad_all(overlay_, 16, 0);
    lv_obj_set_scrollable(overlay_, false);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(overlay_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon = lv_label_create(overlay_);
    lv_label_set_text(icon, LV_SYMBOL_USB);
    lv_obj_set_style_text_color(icon, Theme::color(0x2E6E62), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);

    lv_obj_t *title = lv_label_create(overlay_);
    lv_label_set_text(title, "USB Disk Mode");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, Theme::color(0x222725), 0);

    status_ = lv_label_create(overlay_);
    lv_label_set_text(status_, "Connecting SD card to computer...");
    lv_obj_set_style_text_font(status_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_, Theme::color(0x505754), 0);

    lv_obj_t *hint = lv_label_create(overlay_);
    lv_label_set_text(hint, "Copy files, then EJECT POCKETPDA on the computer.\nThe pager will reload and import the agenda automatically.");
    lv_obj_set_width(hint, LV_PCT(92));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, Theme::color(0x9A5D00), 0);
}

bool UsbDiskService::begin(FinishedCallback callback, void *context) {
    if (active_) return true;
    if (callback) setFinishedCallback(callback, context);
    if (!storage_.mounted()) { lastError_ = "Insert and mount a microSD card first."; return false; }
    if (!lilygo_usb_service_begin() || !lilygo_usb_service_available()) { lastError_ = "TinyUSB could not be started."; return false; }
    if (!lilygo_usb_msc_select_backend(LILYGO_USB_MSC_BACKEND_SD) || !lilygo_usb_msc_available()) { lastError_ = "The SD card cannot be exposed over USB."; return false; }

    createOverlay();
    lv_refr_now(nullptr);
    setInputsEnabled(false);
    refreshTimer_ = lv_display_get_refr_timer(nullptr);
    if (refreshTimer_) lv_timer_pause(refreshTimer_);
    if (!storage_.beginHostAccess()) {
        if (refreshTimer_) lv_timer_resume(refreshTimer_);
        refreshTimer_ = nullptr;
        setInputsEnabled(true);
        lv_obj_delete(overlay_); overlay_ = nullptr; status_ = nullptr;
        lastError_ = "The SD card is busy.";
        return false;
    }

    active_ = true;
    if (!lilygo_usb_msc_set_active(true)) {
        lastError_ = lilygo_usb_msc_status();
        finish();
        return false;
    }
    lastStatusUpdate_ = 0;
    return true;
}

void UsbDiskService::update() {
    if (!active_) return;
    lilygo_usb_msc_poll();
    if (!lilygo_usb_msc_is_active()) { finish(); return; }
    if (millis() - lastStatusUpdate_ >= 500) {
        lastStatusUpdate_ = millis();
        if (status_) lv_label_set_text(status_, lilygo_usb_msc_status());
    }
}

void UsbDiskService::finish() {
    if (!active_) return;
    active_ = false;
    storage_.endHostAccess();
    storage_.unmount();
    const bool storageReady = storage_.mount(bus_);

    if (refreshTimer_) lv_timer_resume(refreshTimer_);
    refreshTimer_ = nullptr;
    if (overlay_) lv_obj_delete(overlay_);
    overlay_ = nullptr;
    status_ = nullptr;
    setInputsEnabled(true);
    lv_refr_now(nullptr);
    if (callback_) callback_(callbackContext_, storageReady);
}
