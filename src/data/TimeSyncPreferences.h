#pragma once
#include <stdint.h>
struct TimeSyncPreferences {
    uint8_t version=1;
    uint8_t automatic=1;
    uint8_t timezone=0; // 0 Europe/Amsterdam; 1 UTC
    uint8_t reserved=0;
    uint16_t intervalHours=4;
    uint16_t reserved2=0;
};
inline bool validTimeSyncPreferences(const TimeSyncPreferences &p) {
    return p.version==1&&p.automatic<=1&&p.timezone<=1&&p.intervalHours>=1&&p.intervalHours<=24&&p.reserved==0&&p.reserved2==0;
}
