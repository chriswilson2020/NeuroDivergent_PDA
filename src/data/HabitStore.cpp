#include "HabitStore.h"
#include "StoreIO.h"
#include "hardware/SPIBusManager.h"
#include "hardware/StorageService.h"
#include <Arduino.h>
#include <SD.h>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x48414231; // HAB1
constexpr const char *kPath = "/PocketPDA/habits/habits.dat";
}

bool HabitStore::load() {
    bool existed = false;
    if (storage_.mounted()) {
        SPIBusManager::Guard guard(bus_);
        if (guard) existed = SD.exists(kPath);
    }
    const bool ok = StoreIO::load(storage_, bus_, kPath, kMagic, records_, kCapacity, count_);
    nextId_ = 1;
    for (size_t i = 0; i < count_; ++i) if (records_[i].id >= nextId_) nextId_ = records_[i].id + 1;
    if (ok && !existed) {
        addDefaults();
        return save();
    }
    return ok;
}

bool HabitStore::save() { return StoreIO::save(storage_, bus_, kPath, kMagic, records_, count_); }

HabitRecord *HabitStore::find(uint32_t id) {
    for (size_t i = 0; i < count_; ++i) if (records_[i].id == id) return &records_[i];
    return nullptr;
}

bool HabitStore::upsert(HabitRecord &record) {
    HabitRecord *existing = find(record.id);
    if (existing) {
        const uint32_t id = existing->id;
        const uint32_t last = existing->lastCompletedDate;
        const uint32_t previous = existing->previousCompletedDate;
        const uint16_t streak = existing->streak;
        const uint16_t previousStreak = existing->previousStreak;
        *existing = record;
        existing->id = id;
        existing->lastCompletedDate = last;
        existing->previousCompletedDate = previous;
        existing->streak = streak;
        existing->previousStreak = previousStreak;
    } else {
        if (count_ >= kCapacity) return false;
        record.id = nextId_++;
        records_[count_++] = record;
    }
    return save();
}

bool HabitStore::remove(uint32_t id) {
    for (size_t i = 0; i < count_; ++i) {
        if (records_[i].id != id) continue;
        memmove(&records_[i], &records_[i + 1], (count_ - i - 1) * sizeof(HabitRecord));
        --count_;
        return save();
    }
    return false;
}

bool HabitStore::toggleToday(uint32_t id, uint32_t today, uint32_t yesterday) {
    HabitRecord *record = find(id);
    if (!record) return false;
    if (record->lastCompletedDate == today) {
        record->lastCompletedDate = record->previousCompletedDate;
        record->streak = record->previousStreak;
        record->previousCompletedDate = 0;
        record->previousStreak = 0;
    } else {
        record->previousCompletedDate = record->lastCompletedDate;
        record->previousStreak = record->streak;
        record->lastCompletedDate = today;
        record->streak = record->previousCompletedDate == yesterday ? record->previousStreak + 1 : 1;
    }
    return save();
}

void HabitStore::addDefaults() {
    static const char *names[] = {"Morning check-in", "Drink water", "Move for 5 minutes", "Plan tomorrow"};
    for (const char *name : names) {
        HabitRecord record{};
        record.id = nextId_++;
        strlcpy(record.name, name, sizeof(record.name));
        records_[count_++] = record;
    }
}
