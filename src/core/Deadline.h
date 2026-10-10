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
// External RTC reads have one-second resolution. Wake conservatively before
// the earliest possible boundary, rather than rounding a deadline up.
inline int64_t wallDeadlineUs(time_t now,time_t due,int64_t sampledUs) {
    return due ? sampledUs+(static_cast<int64_t>(due)-now)*1000000-1000000 : 0;
}
inline int64_t earliestUs(int64_t a,int64_t b) { return !a?b:!b?a:a<b?a:b; }
inline uint64_t sleepBudgetUs(int64_t nowUs,int64_t dueUs,int64_t housekeepingUs,uint32_t marginUs=2000) {
    const int64_t end=earliestUs(dueUs,housekeepingUs);
    return end>nowUs+marginUs?static_cast<uint64_t>(end-nowUs-marginUs):0;
}
}
