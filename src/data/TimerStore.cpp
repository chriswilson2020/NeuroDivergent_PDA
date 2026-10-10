#include "TimerStore.h"
#include "StoreIO.h"
#include "core/TimeBasis.h"
#include "hardware/SPIBusManager.h"
#include "hardware/StorageService.h"
#include <Arduino.h>
#include <SD.h>
#include <cstring>

namespace {
constexpr uint32_t kPresetMagic = 0x54494D31; // TIM1
constexpr uint32_t kRuntimeMagic = 0x54495232; // TIR2: UTC epoch
constexpr const char *kPresetPath = "/PocketPDA/timers/presets.dat";
constexpr const char *kRuntimePath = "/PocketPDA/timers/active.dat";
}

bool TimerStore::load() {
    bool hadPresetFile = false;
    if (storage_.mounted()) { SPIBusManager::Guard guard(bus_); if (guard) hadPresetFile = SD.exists(kPresetPath); }
    const bool ok = StoreIO::load(storage_, bus_, kPresetPath, kPresetMagic, records_, kCapacity, count_);
    nextId_ = 1;
    for (size_t i = 0; i < count_; ++i) if (records_[i].id >= nextId_) nextId_ = records_[i].id + 1;
    if (ok && !hadPresetFile) return createDefaults();
    return ok;
}

bool TimerStore::createDefaults() {
    static const struct { const char *name; uint16_t minutes; uint8_t effect; } defaults[] = {
        {"Focus", 15, 47}, {"Break", 5, 1}, {"Leave in", 10, 15}
    };
    for (const auto &item : defaults) {
        TimerPreset &record = records_[count_++]; record.id = nextId_++; record.minutes = item.minutes;
        record.effect = item.effect; strlcpy(record.name, item.name, sizeof(record.name));
    }
    return save();
}

bool TimerStore::save() { return StoreIO::save(storage_, bus_, kPresetPath, kPresetMagic, records_, count_); }
TimerPreset *TimerStore::find(uint32_t id) { for (size_t i=0;i<count_;++i) if(records_[i].id==id) return &records_[i]; return nullptr; }
bool TimerStore::upsert(TimerPreset &record) { TimerPreset *old=find(record.id); if(old)*old=record; else { if(count_>=kCapacity)return false; record.id=nextId_++;records_[count_++]=record; } return save(); }
bool TimerStore::remove(uint32_t id) { for(size_t i=0;i<count_;++i)if(records_[i].id==id){memmove(&records_[i],&records_[i+1],(count_-i-1)*sizeof(TimerPreset));--count_;return save();}return false; }
bool TimerStore::loadRuntime(TimerRuntime &runtime) {
    size_t count=0;
    if(StoreIO::load(storage_,bus_,kRuntimePath,kRuntimeMagic,&runtime,1,count)&&count==1)return true;
    count=0;
    if(!StoreIO::load(storage_,bus_,kRuntimePath,0x54495231,&runtime,1,count)||count!=1)return false;
    if(runtime.finishAt){tm civil{};gmtime_r(&runtime.finishAt,&civil);time_t epoch=0;
        if(!TimeBasis::local(civil,epoch))return false;runtime.finishAt=epoch;}
    return saveRuntime(runtime);
}
bool TimerStore::saveRuntime(const TimerRuntime &runtime) { return StoreIO::save(storage_,bus_,kRuntimePath,kRuntimeMagic,&runtime,1); }
bool TimerStore::clearRuntime() { if(!storage_.mounted())return false;SPIBusManager::Guard guard(bus_);return guard&&(!SD.exists(kRuntimePath)||SD.remove(kRuntimePath)); }
