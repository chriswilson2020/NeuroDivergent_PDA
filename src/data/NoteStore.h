#pragma once
#include <stddef.h>
#include <stdint.h>
class StorageService; class SPIBusManager;

struct NoteRecord { uint32_t id = 0; uint32_t modified = 0; char title[48]{}; };
class NoteStore {
public:
    static constexpr size_t kCapacity = 32;
    NoteStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    bool load(); bool save(); size_t count() const { return count_; }
    const NoteRecord &at(size_t i) const { return records_[i]; }
    NoteRecord *find(uint32_t id); bool upsert(NoteRecord &record, const char *body); bool remove(uint32_t id); bool readBody(uint32_t id, char *buffer, size_t capacity);
private:
    StorageService &storage_; SPIBusManager &bus_; NoteRecord records_[kCapacity]{}; size_t count_ = 0; uint32_t nextId_ = 1;
};
