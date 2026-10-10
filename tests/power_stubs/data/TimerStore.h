#pragma once
#include <time.h>
#include <stdint.h>
#include <vector>
struct TimerPreset {uint32_t id=0;uint8_t type=0,enabled=1,effect=47;uint16_t minutes=15;uint8_t hour=9,minute=0;char name[32]="TEST";};
struct TimerRuntime {time_t finishAt=0;uint8_t effect=47;char name[32]{};};
class TimerStore {
public:
    std::vector<TimerPreset> presets;TimerRuntime persisted{};
    size_t count()const{return presets.size();}
    const TimerPreset &at(size_t i)const{return presets[i];}
    bool loadRuntime(TimerRuntime &r){r=persisted;return true;}
    bool saveRuntime(const TimerRuntime &r){persisted=r;return true;}
    bool clearRuntime(){persisted={};return true;}
};
