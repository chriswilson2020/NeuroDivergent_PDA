#include "CalendarStore.h"
#include "StoreIO.h"
#include <Arduino.h>
#include <SD.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_heap_caps.h>
#include <time.h>

namespace {
constexpr uint32_t kMagic = 0x43414C31; // CAL1
constexpr const char *kDataPath = "/PocketPDA/calendar/events.dat";

uint32_t checksumUpdate(uint32_t checksum, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i) { checksum ^= data[i]; checksum *= 16777619u; }
    return checksum;
}

bool validHeader(const StoreIO::Header &header) {
    return header.magic == kMagic && header.version == 1 && header.recordSize == sizeof(CalendarEvent);
}

CalendarEvent *allocateEvents(size_t count) {
    if (!count) count = 1;
    auto *result = static_cast<CalendarEvent *>(heap_caps_malloc(count * sizeof(CalendarEvent), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!result) result = static_cast<CalendarEvent *>(heap_caps_malloc(count * sizeof(CalendarEvent), MALLOC_CAP_8BIT));
    return result;
}

int compareEvents(const void *left, const void *right) {
    const auto &a = *static_cast<const CalendarEvent *>(left);
    const auto &b = *static_cast<const CalendarEvent *>(right);
    if (a.year != b.year) return a.year < b.year ? -1 : 1;
    if (a.month != b.month) return a.month < b.month ? -1 : 1;
    if (a.day != b.day) return a.day < b.day ? -1 : 1;
    if (a.startHour != b.startHour) return a.startHour < b.startHour ? -1 : 1;
    if (a.startMinute != b.startMinute) return a.startMinute < b.startMinute ? -1 : 1;
    return a.id < b.id ? -1 : a.id > b.id ? 1 : 0;
}
}

CalendarStore::~CalendarStore() { if (cache_) heap_caps_free(cache_); }

time_t CalendarStore::dateEpoch(int year, int month, int day) {
    struct tm value{}; value.tm_year = year - 1900; value.tm_mon = month - 1; value.tm_mday = day;
    value.tm_hour = 12; value.tm_isdst = -1; return mktime(&value);
}

bool CalendarStore::load() {
    cacheCount_ = 0; windowStart_ = windowEnd_ = 0; lastImportCount_ = 0;
    if (importCsv("/PocketPDA/calendar/import.csv")) return true;
    return scanMetadata();
}

bool CalendarStore::scanMetadata() {
    totalCount_ = 0; nextId_ = 1;
    if (!storage_.mounted()) return false;
    SPIBusManager::Guard guard(bus_); if (!guard) return false;
    File file = SD.open(kDataPath, FILE_READ); if (!file) return true;
    StoreIO::Header header{};
    if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) || !validHeader(header)) { file.close(); return false; }
    uint32_t checksum = 2166136261u;
    for (size_t i = 0; i < header.count; ++i) {
        CalendarEvent event{};
        if (file.read(reinterpret_cast<uint8_t *>(&event), sizeof(event)) != sizeof(event)) { file.close(); return false; }
        checksum = checksumUpdate(checksum, reinterpret_cast<const uint8_t *>(&event), sizeof(event));
        if (event.id >= nextId_) nextId_ = event.id + 1;
    }
    file.close();
    if (checksum != header.checksum) { nextId_ = 1; return false; }
    totalCount_ = header.count;
    return true;
}

bool CalendarStore::reserveCache(size_t required) {
    if (required <= cacheCapacity_) return true;
    size_t capacity = cacheCapacity_ ? cacheCapacity_ * 2 : 32;
    while (capacity < required) capacity *= 2;
    CalendarEvent *replacement = allocateEvents(capacity); if (!replacement) return false;
    if (cacheCount_) memcpy(replacement, cache_, cacheCount_ * sizeof(CalendarEvent));
    if (cache_) heap_caps_free(cache_);
    cache_ = replacement; cacheCapacity_ = capacity;
    return true;
}

bool CalendarStore::eventFallsInWindow(const CalendarEvent &event) const {
    if (!event.weekly) { const time_t date = dateEpoch(event.year, event.month, event.day); return date >= windowStart_ && date < windowEnd_; }
    for (int offset = 0; offset < 14; ++offset) {
        struct tm day{}; time_t epoch = windowStart_; day.tm_year = 0; localtime_r(&epoch, &day); day.tm_mday += offset; day.tm_hour = 12; day.tm_isdst = -1; mktime(&day);
        if (occursOn(event, day.tm_year + 1900, day.tm_mon + 1, day.tm_mday)) return true;
    }
    return false;
}

bool CalendarStore::loadWindow(int year, int month, int day) {
    struct tm monday{}; monday.tm_year = year - 1900; monday.tm_mon = month - 1; monday.tm_mday = day; monday.tm_hour = 12; monday.tm_isdst = -1; mktime(&monday);
    monday.tm_mday -= (monday.tm_wday + 6) % 7; mktime(&monday); windowStart_ = mktime(&monday);
    struct tm end = monday; end.tm_mday += 14; windowEnd_ = mktime(&end);
    cacheCount_ = 0;
    if (!storage_.mounted()) return false;
    SPIBusManager::Guard guard(bus_); if (!guard) return false;
    File file = SD.open(kDataPath, FILE_READ); if (!file) return true;
    StoreIO::Header header{};
    if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) || !validHeader(header)) { file.close(); return false; }
    uint32_t checksum = 2166136261u;
    for (size_t i = 0; i < header.count; ++i) {
        CalendarEvent event{};
        if (file.read(reinterpret_cast<uint8_t *>(&event), sizeof(event)) != sizeof(event)) { file.close(); cacheCount_ = 0; return false; }
        checksum = checksumUpdate(checksum, reinterpret_cast<const uint8_t *>(&event), sizeof(event));
        if (eventFallsInWindow(event)) { if (!reserveCache(cacheCount_ + 1)) { file.close(); cacheCount_ = 0; return false; } cache_[cacheCount_++] = event; }
    }
    file.close();
    if (checksum != header.checksum) { cacheCount_ = 0; return false; }
    if (cacheCount_ > 1) qsort(cache_, cacheCount_, sizeof(CalendarEvent), compareEvents);
    totalCount_ = header.count;
    return true;
}

bool CalendarStore::ensureWindowForDate(int year, int month, int day) {
    const time_t requested = dateEpoch(year, month, day);
    if (windowStart_ && requested >= windowStart_ && requested < windowEnd_) return true;
    return loadWindow(year, month, day);
}

CalendarEvent *CalendarStore::find(uint32_t id) {
    for (size_t i = 0; i < cacheCount_; ++i) if (cache_[i].id == id) return &cache_[i];
    return nullptr;
}

bool CalendarStore::loadAll(CalendarEvent *&records, size_t &count) {
    records = nullptr; count = 0;
    if (!storage_.mounted()) return false;
    SPIBusManager::Guard guard(bus_); if (!guard) return false;
    File file = SD.open(kDataPath, FILE_READ);
    if (!file) { records = allocateEvents(1); return records != nullptr; }
    StoreIO::Header header{};
    if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) || !validHeader(header)) { file.close(); return false; }
    records = allocateEvents(static_cast<size_t>(header.count) + 1); if (!records) { file.close(); return false; }
    const size_t bytes = static_cast<size_t>(header.count) * sizeof(CalendarEvent);
    if (bytes && file.read(reinterpret_cast<uint8_t *>(records), bytes) != bytes) { file.close(); heap_caps_free(records); records = nullptr; return false; }
    file.close();
    if (StoreIO::checksum(reinterpret_cast<const uint8_t *>(records), bytes) != header.checksum) { heap_caps_free(records); records = nullptr; return false; }
    count = header.count; return true;
}

bool CalendarStore::writeAll(CalendarEvent *records, size_t count) {
    if (count > UINT16_MAX) return false;
    if (count > 1) qsort(records, count, sizeof(CalendarEvent), compareEvents);
    return StoreIO::save(storage_, bus_, kDataPath, kMagic, records, count);
}

bool CalendarStore::reloadWindow() {
    if (!windowStart_) return true;
    struct tm anchor{}; localtime_r(&windowStart_, &anchor);
    return loadWindow(anchor.tm_year + 1900, anchor.tm_mon + 1, anchor.tm_mday);
}

bool CalendarStore::upsert(CalendarEvent &event) {
    CalendarEvent *all = nullptr; size_t count = 0;
    if (!loadAll(all, count)) return false;
    CalendarEvent *existing = nullptr;
    for (size_t i = 0; i < count; ++i) if (all[i].id == event.id) { existing = &all[i]; break; }
    if (existing) *existing = event;
    else { if (count >= UINT16_MAX) { heap_caps_free(all); return false; } event.id = nextId_++; all[count++] = event; }
    const bool saved = writeAll(all, count); heap_caps_free(all);
    if (!saved) return false;
    totalCount_ = count; reloadWindow(); return true;
}

bool CalendarStore::remove(uint32_t id) {
    CalendarEvent *all = nullptr; size_t count = 0;
    if (!loadAll(all, count)) return false;
    size_t index = count;
    for (size_t i = 0; i < count; ++i) if (all[i].id == id) { index = i; break; }
    if (index == count) { heap_caps_free(all); return false; }
    memmove(&all[index], &all[index + 1], (count - index - 1) * sizeof(CalendarEvent)); --count;
    const bool saved = writeAll(all, count); heap_caps_free(all);
    if (!saved) return false;
    totalCount_ = count; reloadWindow(); return true;
}

bool CalendarStore::occursOn(const CalendarEvent &event, int year, int month, int day) const {
    if (!event.weekly) return event.year == year && event.month == month && event.day == day;
    struct tm base{}; base.tm_year = event.year - 1900; base.tm_mon = event.month - 1; base.tm_mday = event.day; base.tm_hour = 12; base.tm_isdst = -1; mktime(&base);
    struct tm query{}; query.tm_year = year - 1900; query.tm_mon = month - 1; query.tm_mday = day; query.tm_hour = 12; query.tm_isdst = -1; mktime(&query);
    return query.tm_wday == base.tm_wday && difftime(mktime(&query), mktime(&base)) >= 0;
}

bool CalendarStore::nextReminderAfter(time_t now, time_t &trigger) {
    trigger = 0;
    if (!storage_.mounted()) return false;
    SPIBusManager::Guard guard(bus_); if (!guard) return false;
    File file = SD.open(kDataPath, FILE_READ); if (!file) return true;
    StoreIO::Header header{};
    if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) || !validHeader(header)) { file.close(); return false; }
    uint32_t checksum = 2166136261u;
    for (size_t i = 0; i < header.count; ++i) {
        CalendarEvent event{};
        if (file.read(reinterpret_cast<uint8_t *>(&event), sizeof(event)) != sizeof(event)) { file.close(); return false; }
        checksum = checksumUpdate(checksum, reinterpret_cast<const uint8_t *>(&event), sizeof(event));
        struct tm occurrence{}; occurrence.tm_year = event.year - 1900; occurrence.tm_mon = event.month - 1; occurrence.tm_mday = event.day;
        occurrence.tm_hour = event.startHour; occurrence.tm_min = event.startMinute; occurrence.tm_isdst = -1;
        time_t candidate = mktime(&occurrence) - static_cast<time_t>(event.reminderMinutes) * 60;
        if (event.weekly && candidate <= now) {
            const time_t delta = now - candidate;
            const int weeks = static_cast<int>(delta / (7 * 86400)) + 1;
            occurrence.tm_mday += weeks * 7; occurrence.tm_isdst = -1;
            candidate = mktime(&occurrence) - static_cast<time_t>(event.reminderMinutes) * 60;
            while (candidate <= now) { occurrence.tm_mday += 7; occurrence.tm_isdst = -1; candidate = mktime(&occurrence) - static_cast<time_t>(event.reminderMinutes) * 60; }
        }
        if (candidate > now && (!trigger || candidate < trigger)) trigger = candidate;
    }
    file.close();
    if (checksum != header.checksum) { trigger = 0; return false; }
    return true;
}

bool CalendarStore::importCsv(const char *path) {
    if (!storage_.mounted()) return false;
    size_t imported = 0; uint32_t checksum = 2166136261u; bool valid = true;
    SPIBusManager::Guard guard(bus_); if (!guard) return false;
    File input = SD.open(path, FILE_READ); if (!input) return false;
    const char *temporary = "/PocketPDA/calendar/events.import.tmp"; SD.remove(temporary);
    File output = SD.open(temporary, FILE_WRITE); if (!output) { input.close(); return false; }
    StoreIO::Header header{kMagic, 1, static_cast<uint16_t>(sizeof(CalendarEvent)), 0, 0, checksum};
    if (output.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) != sizeof(header)) valid = false;
    char line[160]; bool firstLine = true;
    while (valid && input.available()) {
        const size_t length = input.readBytesUntil('\n', line, sizeof(line) - 1); line[length] = 0;
        if (length && line[length - 1] == '\r') line[length - 1] = 0;
        if (firstLine) { firstLine = false; if (!strncmp(line, "date,", 5)) continue; }
        if (!line[0] || line[0] == '#') continue;
        if (imported >= UINT16_MAX) { valid = false; break; }
        char *fields[6]{}; size_t fieldCount = 0; char *cursor = line;
        while (fieldCount < 6) { fields[fieldCount++] = cursor; char *comma = strchr(cursor, ','); if (!comma) break; *comma = 0; cursor = comma + 1; }
        if (fieldCount != 6) { valid = false; break; }
        CalendarEvent event{}; int year, month, day, sh, sm, eh, em, reminder;
        if (sscanf(fields[0], "%d-%d-%d", &year, &month, &day) != 3 || sscanf(fields[1], "%d:%d", &sh, &sm) != 2 ||
            sscanf(fields[2], "%d:%d", &eh, &em) != 2 || sscanf(fields[5], "%d", &reminder) != 1 ||
            year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
            sh < 0 || sh > 23 || sm < 0 || sm > 59 || eh < 0 || eh > 23 || em < 0 || em > 59) { valid = false; break; }
        event.id = imported + 1; event.year = year; event.month = month; event.day = day;
        event.startHour = sh; event.startMinute = sm; event.endHour = eh; event.endMinute = em;
        event.reminderMinutes = constrain(reminder, 0, 1440); strlcpy(event.title, fields[3], sizeof(event.title)); strlcpy(event.location, fields[4], sizeof(event.location));
        if (!event.title[0] || output.write(reinterpret_cast<const uint8_t *>(&event), sizeof(event)) != sizeof(event)) { valid = false; break; }
        checksum = checksumUpdate(checksum, reinterpret_cast<const uint8_t *>(&event), sizeof(event)); ++imported;
    }
    input.close();
    if (valid && imported) {
        header.count = static_cast<uint16_t>(imported); header.checksum = checksum;
        valid = output.seek(0) && output.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header);
    }
    output.flush(); output.close();
    if (!valid || !imported) { SD.remove(temporary); return false; }
    SD.remove(kDataPath);
    if (!SD.rename(temporary, kDataPath)) return false;
    SD.remove("/PocketPDA/calendar/last-import.csv"); SD.rename(path, "/PocketPDA/calendar/last-import.csv");
    totalCount_ = imported; nextId_ = imported + 1; lastImportCount_ = imported;
    Serial.printf("[PocketPDA] imported %u calendar events\n", static_cast<unsigned>(imported));
    return true;
}
