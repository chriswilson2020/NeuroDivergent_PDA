#pragma once
#include <stdint.h>
#include <cstring>
extern uint32_t fakeMs;
inline uint32_t millis(){return fakeMs;}
struct LockedTestSerial {
    template<typename... Args>int printf(const char *,Args...){return 0;}
    void println(const char *){}
};
inline LockedTestSerial Serial;
