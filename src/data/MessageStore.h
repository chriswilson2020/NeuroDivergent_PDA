#pragma once
#include <stddef.h>
#include <stdint.h>
class StorageService; class SPIBusManager;

enum class MessageDirection : uint8_t { Incoming = 0, Outgoing = 1 };
enum class MessageDelivery : uint8_t { Queued = 0, Sending = 1, Delivered = 2, Failed = 3 };

struct MessageRecord {
    uint32_t localId = 0;
    uint32_t messageId = 0;
    uint64_t contactId = 0;
    uint32_t timestamp = 0;
    MessageDirection direction = MessageDirection::Incoming;
    MessageDelivery delivery = MessageDelivery::Delivered;
    uint8_t unread = 0;
    uint8_t attempts = 0;
    char text[161]{};
};

class MessageStore {
public:
    static constexpr size_t kCapacity = 128;
    MessageStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    bool load(); bool save();
    size_t count() const { return count_; }
    const MessageRecord &at(size_t index) const { return records_[index]; }
    MessageRecord *findLocal(uint32_t localId);
    MessageRecord *findRemote(uint64_t contactId, uint32_t messageId);
    MessageRecord *nextPending();
    bool addOutgoing(uint64_t contactId, uint32_t messageId, uint32_t timestamp, const char *text, uint32_t &localId);
    bool addIncoming(uint64_t contactId, uint32_t messageId, uint32_t timestamp, const char *text, bool &duplicate);
    bool updateDelivery(uint32_t localId, MessageDelivery delivery, uint8_t attempts);
    bool remove(uint32_t localId);
    void markRead(uint64_t contactId);
    size_t unreadCount() const;
private:
    bool makeRoom();
    StorageService &storage_; SPIBusManager &bus_;
    MessageRecord records_[kCapacity]{}; size_t count_ = 0; uint32_t nextLocalId_ = 1;
};
