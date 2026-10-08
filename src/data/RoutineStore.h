#pragma once
#include <stddef.h>
#include <stdint.h>

class StorageService;
class SPIBusManager;

struct RoutineRecord {
    uint32_t id = 0;
    uint8_t stepCount = 0;
    char title[36]{};
    char steps[8][48]{};
};

class RoutineStore {
public:
    static constexpr size_t kCapacity = 12;
    RoutineStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    bool load();
    bool save();
    size_t count() const { return count_; }
    size_t lastImportCount() const { return lastImportCount_; }
    const RoutineRecord &at(size_t index) const { return records_[index]; }
    RoutineRecord *find(uint32_t id);
    bool upsert(RoutineRecord &record);
    bool remove(uint32_t id);

private:
    bool importCsv();
    StorageService &storage_;
    SPIBusManager &bus_;
    RoutineRecord records_[kCapacity]{};
    size_t count_ = 0;
    size_t lastImportCount_ = 0;
    uint32_t nextId_ = 1;
};
