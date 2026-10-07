#pragma once

#include "data/SettingsStore.h"
#include <stdint.h>

class SPIBusManager;
class StorageService;
namespace fs { class File; }

class BackupService {
public:
    using RestoredCallback = void (*)(void *context);
    static constexpr const char *kBackupPath = "/PocketPDA/backups/PocketPDA-Backup.ppb";

    BackupService(StorageService &storage, SPIBusManager &bus, SettingsStore &settings)
        : storage_(storage), bus_(bus), settings_(settings) {}
    bool create();
    bool restore();
    const char *lastError() const { return error_; }
    void setRestoredCallback(RestoredCallback callback, void *context) { restoredCallback_ = callback; restoredContext_ = context; }

private:
    bool appendDirectory(fs::File &backup, const char *directory, const char *relative, uint16_t &count);
    bool appendFile(fs::File &backup, fs::File &source, const char *relative, uint16_t &count);
    bool appendSettings(fs::File &backup, uint16_t &count);
    bool validate(fs::File &backup);
    bool extract(fs::File &backup);
    bool swapStaging();
    bool removeTree(const char *path);
    bool makeParentDirectories(const char *path);
    bool allowedPath(const char *path) const;
    void fail(const char *message);

    StorageService &storage_;
    SPIBusManager &bus_;
    SettingsStore &settings_;
    RestoredCallback restoredCallback_ = nullptr;
    void *restoredContext_ = nullptr;
    char error_[112]{};
    DeviceSettings restoredSettings_{};
    bool hasRestoredSettings_ = false;
};
