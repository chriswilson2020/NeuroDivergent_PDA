#include "RoutineStore.h"
#include "StoreIO.h"
#include "hardware/SPIBusManager.h"
#include "hardware/StorageService.h"
#include <Arduino.h>
#include <SD.h>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x52544E31; // RTN1
constexpr const char *kPath = "/PocketPDA/routines/routines.dat";
constexpr const char *kImportPath = "/PocketPDA/routines/import.csv";
}

bool RoutineStore::load() {
    count_ = 0;
    lastImportCount_ = 0;
    if (importCsv()) return true;
    const bool ok = StoreIO::load(storage_, bus_, kPath, kMagic, records_, kCapacity, count_);
    nextId_ = 1;
    for (size_t i = 0; i < count_; ++i) if (records_[i].id >= nextId_) nextId_ = records_[i].id + 1;
    return ok;
}

bool RoutineStore::save() { return StoreIO::save(storage_, bus_, kPath, kMagic, records_, count_); }

RoutineRecord *RoutineStore::find(uint32_t id) {
    for (size_t i = 0; i < count_; ++i) if (records_[i].id == id) return &records_[i];
    return nullptr;
}

bool RoutineStore::upsert(RoutineRecord &record) {
    RoutineRecord *existing = find(record.id);
    if (existing) *existing = record;
    else {
        if (count_ >= kCapacity) return false;
        record.id = nextId_++;
        records_[count_++] = record;
    }
    return save();
}

bool RoutineStore::remove(uint32_t id) {
    for (size_t i = 0; i < count_; ++i) {
        if (records_[i].id != id) continue;
        memmove(&records_[i], &records_[i + 1], (count_ - i - 1) * sizeof(RoutineRecord));
        --count_;
        return save();
    }
    return false;
}

bool RoutineStore::importCsv() {
    if (!storage_.mounted()) return false;
    RoutineRecord imported[kCapacity]{};
    size_t importedCount = 0;
    {
        SPIBusManager::Guard guard(bus_);
        if (!guard) return false;
        File input = SD.open(kImportPath, FILE_READ);
        if (!input) return false;
        char line[128];
        bool first = true;
        while (input.available()) {
            const size_t length = input.readBytesUntil('\n', line, sizeof(line) - 1);
            line[length] = 0;
            if (length && line[length - 1] == '\r') line[length - 1] = 0;
            if (first) { first = false; if (!strncmp(line, "routine,", 8)) continue; }
            if (!line[0] || line[0] == '#') continue;
            char *comma = strchr(line, ',');
            if (!comma) { input.close(); return false; }
            *comma = 0;
            const char *title = line;
            const char *step = comma + 1;
            if (!title[0] || !step[0]) { input.close(); return false; }
            RoutineRecord *routine = nullptr;
            for (size_t i = 0; i < importedCount; ++i) if (!strcmp(imported[i].title, title)) routine = &imported[i];
            if (!routine) {
                if (importedCount >= kCapacity) { input.close(); return false; }
                routine = &imported[importedCount++];
                routine->id = importedCount;
                strlcpy(routine->title, title, sizeof(routine->title));
            }
            if (routine->stepCount >= 8) { input.close(); return false; }
            strlcpy(routine->steps[routine->stepCount++], step, sizeof(routine->steps[0]));
        }
        input.close();
        if (!importedCount) return false;
    }
    memcpy(records_, imported, importedCount * sizeof(RoutineRecord));
    count_ = importedCount;
    nextId_ = importedCount + 1;
    if (!save()) { count_ = 0; return false; }
    {
        SPIBusManager::Guard guard(bus_);
        if (guard) {
            SD.remove("/PocketPDA/routines/last-import.csv");
            SD.rename(kImportPath, "/PocketPDA/routines/last-import.csv");
        }
    }
    lastImportCount_ = importedCount;
    return true;
}
