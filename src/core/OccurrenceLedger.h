#pragma once
#include <stdint.h>
#include <time.h>

// Per persistent item ID: last delivered occurrence, not a global wall-clock
// watermark. One item's delivery must never suppress another simultaneous one.
class OccurrenceLedger {
public:
    void begin(const char *name){name_=name;}
    bool claim(char kind,uint32_t id,time_t occurrence);
    bool healthy() const { return healthy_; }
    void beginPass() { healthy_=true; }
private:const char *name_="pda-occurrences";bool healthy_=true;
};
