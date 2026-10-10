#pragma once
#include <time.h>
#include <stdint.h>
namespace Deadline {
inline time_t earliest(time_t a, time_t b) { return !a ? b : !b ? a : a < b ? a : b; }
inline bool crossed(time_t due, time_t previous, time_t now) { return due > previous && due <= now; }
inline time_t daily(time_t now, int hour, int minute) {
    struct tm day{}; localtime_r(&now, &day);
    day.tm_hour=hour; day.tm_min=minute; day.tm_sec=0; day.tm_isdst=-1;
    time_t due=mktime(&day);
    if(due<=now) { ++day.tm_mday; day.tm_isdst=-1; due=mktime(&day); }
    return due;
}
inline uint32_t sleepBudgetMs(time_t now, time_t due, uint32_t cap) {
    if(!due) return cap;
    if(due<=now) return 0;
    const uint64_t ms=static_cast<uint64_t>(due-now)*1000;
    return ms<cap?static_cast<uint32_t>(ms):cap;
}
}
