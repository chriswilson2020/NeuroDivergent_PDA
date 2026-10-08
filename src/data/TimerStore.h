#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>

class StorageService;
class SPIBusManager;

struct TimerPreset {
    uint32_t id = 0;
    uint8_t type = 0;       // 0=countdown, 1=daily alarm
    uint8_t enabled = 1;
    uint8_t effect = 47;    // DRV2605 waveform
    uint8_t reserved = 0;
    uint16_t minutes = 15;  // countdown duration
    uint8_t hour = 7;
    uint8_t minute = 0;
    char name[32]{};
};

struct TimerRuntime {
    time_t finishAt = 0;
    uint8_t effect = 47;
    char name[32]{};
};

class TimerStore {
public:
    static constexpr size_t kCapacity = 12;
    TimerStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    bool load();
    bool save();
    size_t count() const { return count_; }
    const TimerPreset &at(size_t index) const { return records_[index]; }
    TimerPreset *find(uint32_t id);
    bool upsert(TimerPreset &record);
    bool remove(uint32_t id);
    bool loadRuntime(TimerRuntime &runtime);
    bool saveRuntime(const TimerRuntime &runtime);
    bool clearRuntime();

private:
    bool createDefaults();
    StorageService &storage_;
    SPIBusManager &bus_;
    TimerPreset records_[kCapacity]{};
    size_t count_ = 0;
    uint32_t nextId_ = 1;
};
