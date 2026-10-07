#pragma once

#include <stddef.h>
#include <stdint.h>

class StorageService;
class SPIBusManager;

struct HabitRecord {
    uint32_t id = 0;
    uint32_t lastCompletedDate = 0;
    uint32_t previousCompletedDate = 0;
    uint16_t streak = 0;
    uint16_t previousStreak = 0;
    char name[32]{};
};

class HabitStore {
public:
    static constexpr size_t kCapacity = 8;

    HabitStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    bool load();
    bool save();
    size_t count() const { return count_; }
    const HabitRecord &at(size_t index) const { return records_[index]; }
    HabitRecord *find(uint32_t id);
    bool upsert(HabitRecord &record);
    bool remove(uint32_t id);
    bool toggleToday(uint32_t id, uint32_t today, uint32_t yesterday);
    bool completedToday(const HabitRecord &record, uint32_t today) const { return record.lastCompletedDate == today; }

private:
    void addDefaults();

    StorageService &storage_;
    SPIBusManager &bus_;
    HabitRecord records_[kCapacity]{};
    size_t count_ = 0;
    uint32_t nextId_ = 1;
};
