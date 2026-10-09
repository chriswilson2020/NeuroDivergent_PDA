#pragma once
#include <stddef.h>
#include <stdint.h>

struct MessagingContact { uint64_t deviceId=0; char name[24]{}; uint8_t key[32]{}; };
struct MessagingSettings {
    bool enabled=false; bool notifications=true; char deviceName[24]="PocketPDA";
    uint32_t frequencyKhz=868300; uint8_t spreadingFactor=8; int8_t powerDbm=14;
    uint16_t retrySeconds=30;
    uint64_t deviceId=0; uint32_t messageSequence=1; uint32_t counterHighWater=1;
    MessagingContact contacts[4]{};
};
class MessagingSettingsStore {
public:
    bool load(); bool save();
    const MessagingSettings &value() const{return settings_;} MessagingSettings &value(){return settings_;}
    const MessagingContact *findContact(uint64_t id)const; MessagingContact *findContact(uint64_t id);
    bool upsertContact(uint64_t id,const char*name,const uint8_t key[32]); bool removeContact(uint64_t id);
    uint32_t nextMessageId(); uint32_t nextPacketCounter();
private:
    MessagingSettings settings_{}; uint32_t counterNext_=1,counterLimit_=1;
};
