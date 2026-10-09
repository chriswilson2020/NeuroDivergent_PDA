#include "MessageStore.h"
#include "StoreIO.h"
#include <Arduino.h>
#include <cstring>
namespace { constexpr uint32_t kMagic = 0x4D534731; }

bool MessageStore::load() {
    const bool ok = StoreIO::load(storage_, bus_, "/PocketPDA/messages/messages.dat", kMagic, records_, kCapacity, count_);
    nextLocalId_ = 1;
    bool changed = false;
    for (size_t i = 0; i < count_; ++i) {
        if (records_[i].localId >= nextLocalId_) nextLocalId_ = records_[i].localId + 1;
        if (records_[i].direction == MessageDirection::Outgoing && records_[i].delivery == MessageDelivery::Sending) {
            records_[i].delivery = MessageDelivery::Queued; changed = true;
        }
        records_[i].text[sizeof(records_[i].text) - 1] = 0;
    }
    if (changed) save();
    return ok;
}
bool MessageStore::save() { return StoreIO::save(storage_, bus_, "/PocketPDA/messages/messages.dat", kMagic, records_, count_); }
MessageRecord *MessageStore::findLocal(uint32_t id) { for (size_t i=0;i<count_;++i) if(records_[i].localId==id)return &records_[i]; return nullptr; }
MessageRecord *MessageStore::findRemote(uint64_t contact, uint32_t id) { for(size_t i=0;i<count_;++i)if(records_[i].contactId==contact&&records_[i].messageId==id&&records_[i].direction==MessageDirection::Incoming)return &records_[i];return nullptr; }
MessageRecord *MessageStore::nextPending() { for(size_t i=0;i<count_;++i)if(records_[i].direction==MessageDirection::Outgoing&&(records_[i].delivery==MessageDelivery::Queued||records_[i].delivery==MessageDelivery::Sending))return &records_[i];return nullptr; }
bool MessageStore::makeRoom(){if(count_<kCapacity)return true;size_t victim=kCapacity;for(size_t i=0;i<count_;++i)if(records_[i].delivery==MessageDelivery::Delivered){victim=i;break;}if(victim==kCapacity)return false;memmove(&records_[victim],&records_[victim+1],(count_-victim-1)*sizeof(MessageRecord));--count_;return true;}
bool MessageStore::addOutgoing(uint64_t contact,uint32_t messageId,uint32_t timestamp,const char*text,uint32_t&localId){if(!makeRoom())return false;MessageRecord&r=records_[count_++];r={};r.localId=nextLocalId_++;r.messageId=messageId;r.contactId=contact;r.timestamp=timestamp;r.direction=MessageDirection::Outgoing;r.delivery=MessageDelivery::Queued;strlcpy(r.text,text?text:"",sizeof(r.text));localId=r.localId;return save();}
bool MessageStore::addIncoming(uint64_t contact,uint32_t messageId,uint32_t timestamp,const char*text,bool&duplicate){duplicate=findRemote(contact,messageId)!=nullptr;if(duplicate)return true;if(!makeRoom())return false;MessageRecord&r=records_[count_++];r={};r.localId=nextLocalId_++;r.messageId=messageId;r.contactId=contact;r.timestamp=timestamp;r.direction=MessageDirection::Incoming;r.delivery=MessageDelivery::Delivered;r.unread=1;strlcpy(r.text,text?text:"",sizeof(r.text));return save();}
bool MessageStore::updateDelivery(uint32_t id,MessageDelivery state,uint8_t attempts){auto*r=findLocal(id);if(!r)return false;r->delivery=state;r->attempts=attempts;return save();}
bool MessageStore::remove(uint32_t id){for(size_t i=0;i<count_;++i)if(records_[i].localId==id){memmove(&records_[i],&records_[i+1],(count_-i-1)*sizeof(MessageRecord));--count_;return save();}return false;}
void MessageStore::markRead(uint64_t contact){bool changed=false;for(size_t i=0;i<count_;++i)if(records_[i].contactId==contact&&records_[i].unread){records_[i].unread=0;changed=true;}if(changed)save();}
size_t MessageStore::unreadCount()const{size_t value=0;for(size_t i=0;i<count_;++i)value+=records_[i].unread?1:0;return value;}
