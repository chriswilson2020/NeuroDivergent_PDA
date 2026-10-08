#pragma once
#include <stddef.h>
#include <stdint.h>

class StorageService;
class SPIBusManager;

struct AssignmentRecord {
    uint32_t id = 0;
    uint8_t completed = 0;
    uint8_t priority = 1;
    uint8_t stepCount = 0;
    uint8_t currentStep = 0;
    uint16_t effortMinutes = 30;
    int16_t dueYear = 0;
    uint8_t dueMonth = 0;
    uint8_t dueDay = 0;
    char title[48]{};
    char steps[6][48]{};
};

class AssignmentStore {
public:
    static constexpr size_t kCapacity = 24;
    AssignmentStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    bool load();
    bool save();
    size_t count() const { return count_; }
    const AssignmentRecord &at(size_t index) const { return records_[index]; }
    AssignmentRecord *find(uint32_t id);
    bool upsert(AssignmentRecord &record);
    bool remove(uint32_t id);
    bool completeNextStep(uint32_t id);

private:
    StorageService &storage_;
    SPIBusManager &bus_;
    AssignmentRecord records_[kCapacity]{};
    size_t count_ = 0;
    uint32_t nextId_ = 1;
};
