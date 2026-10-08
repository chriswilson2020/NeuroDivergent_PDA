#include "BackupService.h"
#include "data/SettingsStore.h"
#include "hardware/SPIBusManager.h"
#include "hardware/StorageService.h"
#include <Arduino.h>
#include <SD.h>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x50444231; // PDB1
constexpr uint16_t kVersion = 1;
constexpr uint16_t kMaxEntries = 128;
constexpr const char *kTemporaryPath = "/PocketPDA/backups/PocketPDA-Backup.tmp";
constexpr const char *kStageRoot = "/PocketPDA/restore-staging";
constexpr const char *kSettingsName = "@settings";
constexpr size_t kChunkSize = 512;

struct ArchiveHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t entries;
};

struct EntryHeader {
    char path[88];
    uint32_t size;
    uint32_t checksum;
};

uint32_t checksumUpdate(uint32_t value, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i) { value ^= data[i]; value *= 16777619u; }
    return value;
}

bool readExact(File &file, void *destination, size_t size) {
    return file.read(static_cast<uint8_t *>(destination), size) == size;
}
}

void BackupService::fail(const char *message) { strlcpy(error_, message, sizeof(error_)); }

bool BackupService::create() {
    error_[0] = 0;
    if (!storage_.mounted()) { fail("The microSD card is not available."); return false; }
    bool ok = false;
    {
        SPIBusManager::Guard guard(bus_);
        if (!guard) { fail("The microSD card is busy."); return false; }
        SD.remove(kTemporaryPath);
        File backup = SD.open(kTemporaryPath, FILE_WRITE);
        if (!backup) { fail("Could not create the backup file."); return false; }
        ArchiveHeader header{kMagic, kVersion, 0};
        ok = backup.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header);
        uint16_t count = 0;
        if (ok) ok = appendSettings(backup, count);
        static const char *directories[] = {"calendar", "tasks", "assignments", "habits", "routines", "timers", "notes", "files"};
        for (const char *directory : directories) {
            if (!ok) break;
            char full[64]; snprintf(full, sizeof(full), "/PocketPDA/%s", directory);
            ok = appendDirectory(backup, full, directory, count);
        }
        if (ok) {
            header.entries = count;
            ok = backup.seek(0) && backup.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header);
        }
        backup.flush();
        backup.close();
        if (ok) {
            SD.remove(kBackupPath);
            ok = SD.rename(kTemporaryPath, kBackupPath);
            if (!ok) fail("Could not finalize the backup file.");
        } else {
            SD.remove(kTemporaryPath);
            if (!error_[0]) fail("Backup failed while reading data.");
        }
    }
    return ok;
}

bool BackupService::appendSettings(File &backup, uint16_t &count) {
    if (count >= kMaxEntries) { fail("Too many files to back up."); return false; }
    const DeviceSettings &settings = settings_.value();
    EntryHeader entry{};
    strlcpy(entry.path, kSettingsName, sizeof(entry.path));
    entry.size = sizeof(settings);
    entry.checksum = checksumUpdate(2166136261u, reinterpret_cast<const uint8_t *>(&settings), sizeof(settings));
    if (backup.write(reinterpret_cast<const uint8_t *>(&entry), sizeof(entry)) != sizeof(entry) ||
        backup.write(reinterpret_cast<const uint8_t *>(&settings), sizeof(settings)) != sizeof(settings)) return false;
    ++count;
    return true;
}

bool BackupService::appendDirectory(File &backup, const char *directory, const char *relative, uint16_t &count) {
    File folder = SD.open(directory, FILE_READ);
    if (!folder) return true;
    if (!folder.isDirectory()) { folder.close(); return false; }
    for (;;) {
        File item = folder.openNextFile();
        if (!item) break;
        const char *fullName = item.name();
        const char *base = strrchr(fullName, '/');
        base = base ? base + 1 : fullName;
        char childRelative[88];
        char childFull[128];
        snprintf(childRelative, sizeof(childRelative), "%s/%s", relative, base);
        strlcpy(childFull, item.path(), sizeof(childFull));
        if (strlen(childRelative) >= sizeof(childRelative) - 1 || strlen(childFull) >= sizeof(childFull) - 1) {
            item.close(); folder.close(); fail("A file path is too long to back up."); return false;
        }
        bool ok = item.isDirectory() ? true : appendFile(backup, item, childRelative, count);
        const bool directoryItem = item.isDirectory();
        item.close();
        if (ok && directoryItem) ok = appendDirectory(backup, childFull, childRelative, count);
        if (!ok) { folder.close(); return false; }
    }
    folder.close();
    return true;
}

bool BackupService::appendFile(File &backup, File &source, const char *relative, uint16_t &count) {
    if (count >= kMaxEntries) { fail("Too many files to back up."); return false; }
    EntryHeader entry{};
    strlcpy(entry.path, relative, sizeof(entry.path));
    entry.size = source.size();
    uint8_t buffer[kChunkSize];
    uint32_t checksum = 2166136261u;
    source.seek(0);
    uint32_t remaining = entry.size;
    while (remaining) {
        const size_t wanted = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        const size_t read = source.read(buffer, wanted);
        if (read != wanted) return false;
        checksum = checksumUpdate(checksum, buffer, read);
        remaining -= read;
    }
    entry.checksum = checksum;
    source.seek(0);
    if (backup.write(reinterpret_cast<const uint8_t *>(&entry), sizeof(entry)) != sizeof(entry)) return false;
    remaining = entry.size;
    while (remaining) {
        const size_t wanted = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        const size_t read = source.read(buffer, wanted);
        if (read != wanted || backup.write(buffer, read) != read) return false;
        remaining -= read;
    }
    ++count;
    return true;
}

bool BackupService::restore() {
    error_[0] = 0;
    hasRestoredSettings_ = false;
    if (!storage_.mounted()) { fail("The microSD card is not available."); return false; }
    bool ok = false;
    {
        SPIBusManager::Guard guard(bus_);
        if (!guard) { fail("The microSD card is busy."); return false; }
        File backup = SD.open(kBackupPath, FILE_READ);
        if (!backup) { fail("PocketPDA-Backup.ppb was not found."); return false; }
        ok = validate(backup);
        if (ok) { backup.seek(0); ok = extract(backup); }
        backup.close();
        if (ok) ok = swapStaging();
        if (!ok) removeTree(kStageRoot);
    }
    if (ok && hasRestoredSettings_ && !settings_.save(restoredSettings_)) {
        fail("Data restored, but device settings could not be saved.");
        ok = false;
    }
    if (ok) {
        if (restoredCallback_) restoredCallback_(restoredContext_);
    }
    return ok;
}

bool BackupService::allowedPath(const char *path) const {
    if (!path || !path[0] || path[0] == '/' || strstr(path, "..")) return false;
    if (!strcmp(path, kSettingsName)) return true;
    static const char *prefixes[] = {"calendar/", "tasks/", "assignments/", "habits/", "routines/", "timers/", "notes/", "files/"};
    for (const char *prefix : prefixes) if (!strncmp(path, prefix, strlen(prefix))) return true;
    return false;
}

bool BackupService::validate(File &backup) {
    ArchiveHeader header{};
    if (!readExact(backup, &header, sizeof(header)) || header.magic != kMagic || header.version != kVersion ||
        !header.entries || header.entries > kMaxEntries) { fail("The backup header is invalid."); return false; }
    uint8_t buffer[kChunkSize];
    for (uint16_t i = 0; i < header.entries; ++i) {
        EntryHeader entry{};
        if (!readExact(backup, &entry, sizeof(entry)) || !memchr(entry.path, 0, sizeof(entry.path)) || !allowedPath(entry.path)) {
            fail("The backup contains an invalid file path."); return false;
        }
        if (!strcmp(entry.path, kSettingsName) && entry.size != sizeof(DeviceSettings)) { fail("The saved settings are invalid."); return false; }
        uint32_t checksum = 2166136261u;
        uint32_t remaining = entry.size;
        while (remaining) {
            const size_t wanted = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
            if (backup.read(buffer, wanted) != wanted) { fail("The backup is incomplete."); return false; }
            checksum = checksumUpdate(checksum, buffer, wanted);
            remaining -= wanted;
        }
        if (checksum != entry.checksum) { fail("The backup checksum does not match."); return false; }
    }
    return true;
}

bool BackupService::extract(File &backup) {
    ArchiveHeader header{};
    if (!readExact(backup, &header, sizeof(header))) return false;
    removeTree(kStageRoot);
    if (!SD.mkdir(kStageRoot)) { fail("Could not create restore staging."); return false; }
    static const char *directories[] = {"calendar", "tasks", "assignments", "habits", "routines", "timers", "notes", "files"};
    for (const char *directory : directories) {
        char path[80]; snprintf(path, sizeof(path), "%s/%s", kStageRoot, directory);
        if (!SD.mkdir(path)) { fail("Could not prepare restore folders."); return false; }
    }
    restoredSettings_ = settings_.value();
    uint8_t buffer[kChunkSize];
    for (uint16_t i = 0; i < header.entries; ++i) {
        EntryHeader entry{};
        if (!readExact(backup, &entry, sizeof(entry))) return false;
        if (!strcmp(entry.path, kSettingsName)) {
            if (!readExact(backup, &restoredSettings_, sizeof(restoredSettings_))) return false;
            hasRestoredSettings_ = true;
            continue;
        }
        char destination[160]; snprintf(destination, sizeof(destination), "%s/%s", kStageRoot, entry.path);
        if (!makeParentDirectories(destination)) return false;
        SD.remove(destination);
        File output = SD.open(destination, FILE_WRITE);
        if (!output) { fail("Could not create a restored file."); return false; }
        uint32_t remaining = entry.size;
        bool ok = true;
        while (remaining) {
            const size_t wanted = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
            const size_t read = backup.read(buffer, wanted);
            if (read != wanted || output.write(buffer, read) != read) { ok = false; break; }
            remaining -= read;
        }
        output.flush(); output.close();
        if (!ok) { fail("Could not extract all restored data."); return false; }
    }
    return true;
}

bool BackupService::makeParentDirectories(const char *path) {
    char working[160]; strlcpy(working, path, sizeof(working));
    for (char *cursor = working + 1; *cursor; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = 0;
        if (!SD.exists(working) && !SD.mkdir(working)) return false;
        *cursor = '/';
    }
    return true;
}

bool BackupService::removeTree(const char *path) {
    if (!SD.exists(path)) return true;
    File item = SD.open(path, FILE_READ);
    if (!item) return false;
    if (!item.isDirectory()) { item.close(); return SD.remove(path); }
    for (;;) {
        File child = item.openNextFile();
        if (!child) break;
        char childPath[160]; strlcpy(childPath, child.path(), sizeof(childPath));
        child.close();
        if (!removeTree(childPath)) { item.close(); return false; }
    }
    item.close();
    return SD.rmdir(path);
}

bool BackupService::swapStaging() {
    static const char *directories[] = {"calendar", "tasks", "assignments", "habits", "routines", "timers", "notes", "files"};
    static constexpr size_t kDirectoryCount = sizeof(directories) / sizeof(directories[0]);
    bool oldMoved[kDirectoryCount]{};
    bool newMoved[kDirectoryCount]{};
    for (size_t i = 0; i < kDirectoryCount; ++i) {
        char current[64], old[80];
        snprintf(current, sizeof(current), "/PocketPDA/%s", directories[i]);
        snprintf(old, sizeof(old), "/PocketPDA/backups/pre-restore-%s", directories[i]);
        removeTree(old);
        if (SD.exists(current)) {
            if (!SD.rename(current, old)) { fail("Could not stage the existing data."); goto rollback; }
            oldMoved[i] = true;
        }
    }
    for (size_t i = 0; i < kDirectoryCount; ++i) {
        char staged[96], current[64];
        snprintf(staged, sizeof(staged), "%s/%s", kStageRoot, directories[i]);
        snprintf(current, sizeof(current), "/PocketPDA/%s", directories[i]);
        if (!SD.rename(staged, current)) { fail("Could not activate the restored data."); goto rollback; }
        newMoved[i] = true;
    }
    SD.rmdir(kStageRoot);
    for (size_t i = 0; i < kDirectoryCount; ++i) {
        char old[80]; snprintf(old, sizeof(old), "/PocketPDA/backups/pre-restore-%s", directories[i]);
        removeTree(old);
    }
    return true;

rollback:
    for (size_t i = 0; i < kDirectoryCount; ++i) {
        char staged[96], current[64], old[80];
        snprintf(staged, sizeof(staged), "%s/%s", kStageRoot, directories[i]);
        snprintf(current, sizeof(current), "/PocketPDA/%s", directories[i]);
        snprintf(old, sizeof(old), "/PocketPDA/backups/pre-restore-%s", directories[i]);
        if (newMoved[i]) SD.rename(current, staged);
        if (oldMoved[i]) SD.rename(old, current);
    }
    return false;
}
