#pragma once
#include <stddef.h>
#include <stdint.h>
class StorageService; class SPIBusManager;

struct TaskRecord {
    // recurring: 0=once, 1=weekly, 2=daily, 3=weekdays, 4=monthly.
    uint32_t id = 0;
    uint8_t completed = 0, priority = 1, recurring = 0, reminder = 0;
    int16_t dueYear = 0; uint8_t dueMonth = 0, dueDay = 0;
    char title[48]{};
    char notes[96]{};
};

class TaskStore {
public:
    static constexpr size_t kCapacity = 64;
    TaskStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    bool load(); bool save(); size_t count() const { return count_; }
    size_t lastImportCount() const { return lastImportCount_; }
    const TaskRecord &at(size_t i) const { return records_[i]; }
    TaskRecord *find(uint32_t id); bool upsert(TaskRecord &record); bool remove(uint32_t id); bool setCompleted(uint32_t id, bool completed);
private:
    bool importCsv();
    StorageService &storage_; SPIBusManager &bus_; TaskRecord records_[kCapacity]{}; size_t count_ = 0; uint32_t nextId_ = 1;
    size_t lastImportCount_ = 0;
};
