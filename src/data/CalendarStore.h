#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>
class StorageService;
class SPIBusManager;

struct CalendarEvent {
    uint32_t id = 0;
    int16_t year = 2026;
    uint8_t month = 1, day = 1;
    uint8_t startHour = 9, startMinute = 0, endHour = 10, endMinute = 0;
    uint16_t reminderMinutes = 5;
    uint8_t weekly = 0;
    char title[40]{};
    char location[20]{};
};

class CalendarStore {
public:
    CalendarStore(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    ~CalendarStore();
    bool load();
    bool ensureWindowForDate(int year, int month, int day);
    size_t count() const { return cacheCount_; }
    size_t totalCount() const { return totalCount_; }
    size_t lastImportCount() const { return lastImportCount_; }
    const CalendarEvent &at(size_t index) const { return cache_[index]; }
    CalendarEvent *find(uint32_t id);
    bool upsert(CalendarEvent &event);
    bool remove(uint32_t id);
    bool occursOn(const CalendarEvent &event, int year, int month, int day) const;
    bool nextReminderAfter(time_t now, time_t &trigger);

private:
    bool importCsv(const char *path);
    bool loadWindow(int year, int month, int day);
    bool reserveCache(size_t required);
    bool scanMetadata();
    bool loadAll(CalendarEvent *&records, size_t &count);
    bool writeAll(CalendarEvent *records, size_t count);
    bool reloadWindow();
    bool eventFallsInWindow(const CalendarEvent &event) const;
    static time_t dateEpoch(int year, int month, int day);

    StorageService &storage_;
    SPIBusManager &bus_;
    CalendarEvent *cache_ = nullptr;
    size_t cacheCount_ = 0, cacheCapacity_ = 0, totalCount_ = 0, lastImportCount_ = 0;
    uint32_t nextId_ = 1;
    time_t windowStart_ = 0, windowEnd_ = 0;
};
