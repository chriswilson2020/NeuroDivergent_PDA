#include "StorageService.h"
#include "SPIBusManager.h"
#include <LilyGoLib.h>
#include <SD.h>

bool StorageService::mount(SPIBusManager &bus) {
    hostOwned_ = false;
    SPIBusManager::Guard guard(bus);
    if (!guard) return false;
    mounted_ = instance.installSD();
    if (!mounted_) return false;
    const char *paths[] = {"/PocketPDA", "/PocketPDA/calendar", "/PocketPDA/tasks", "/PocketPDA/notes", "/PocketPDA/files", "/PocketPDA/backups"};
    for (const char *path : paths) if (!SD.exists(path)) SD.mkdir(path);
    return true;
}
void StorageService::unmount() { if (mounted_) instance.uninstallSD(); mounted_ = false; hostOwned_ = false; }

bool StorageService::beginHostAccess() {
    if (!mounted_ || hostOwned_) return false;
    hostOwned_ = true;
    return true;
}
