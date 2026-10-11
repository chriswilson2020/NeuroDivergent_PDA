#pragma once
#include <cstddef>
#include <cstdint>
class StorageService;
class SPIBusManager;

struct ContactRecord {
    uint32_t id=0;
    uint8_t favourite=0;
    char name[64]{},company[64]{},job[48]{},mobile[32]{},phone[32]{};
    char email[96]{},address[160]{},website[96]{},notes[256]{};
};

class ContactStore {
public:
    static constexpr size_t kCapacity=512;
    ContactStore(StorageService &storage,SPIBusManager &bus):storage_(storage),bus_(bus){}
    ~ContactStore();
    ContactStore(const ContactStore&)=delete;
    ContactStore &operator=(const ContactStore&)=delete;
    bool load();
    bool upsert(ContactRecord &record);
    bool remove(uint32_t id);
    size_t count() const { return count_; }
    const ContactRecord &at(size_t i) const { return records_[i]; }
    const ContactRecord *find(uint32_t id) const;
    bool ready() const { return ready_; }
    const char *error() const { return error_; }
    static bool matches(const ContactRecord &,const char *query);
    static bool valid(const ContactRecord &);
private:
    bool save();
    void sort();
    StorageService &storage_;SPIBusManager &bus_;
    ContactRecord *records_=nullptr;
    size_t count_=0;
    bool ready_=false;
    const char *error_="microSD unavailable";
};
