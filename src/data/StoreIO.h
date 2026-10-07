#pragma once
#include "hardware/SPIBusManager.h"
#include "hardware/StorageService.h"
#include <SD.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>

namespace StoreIO {
struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t recordSize;
    uint16_t count;
    uint16_t reserved;
    uint32_t checksum;
};

inline uint32_t checksum(const uint8_t *data, size_t size) {
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < size; ++i) { value ^= data[i]; value *= 16777619u; }
    return value;
}

template <typename T>
bool load(StorageService &storage, SPIBusManager &bus, const char *path,
          uint32_t magic, T *records, size_t capacity, size_t &count) {
    count = 0;
    if (!storage.mounted()) return false;
    SPIBusManager::Guard guard(bus);
    if (!guard) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return true;
    Header header{};
    if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
        header.magic != magic || header.version != 1 || header.recordSize != sizeof(T) || header.count > capacity) {
        file.close(); return false;
    }
    const size_t bytes = static_cast<size_t>(header.count) * sizeof(T);
    if (bytes && file.read(reinterpret_cast<uint8_t *>(records), bytes) != bytes) { file.close(); return false; }
    file.close();
    if (header.checksum != checksum(reinterpret_cast<const uint8_t *>(records), bytes)) return false;
    count = header.count;
    return true;
}

template <typename T>
bool save(StorageService &storage, SPIBusManager &bus, const char *path,
          uint32_t magic, const T *records, size_t count) {
    if (!storage.mounted()) return false;
    SPIBusManager::Guard guard(bus);
    if (!guard) return false;
    char temporary[96]; snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    SD.remove(temporary);
    File file = SD.open(temporary, FILE_WRITE);
    if (!file) return false;
    const size_t bytes = count * sizeof(T);
    Header header{magic, 1, static_cast<uint16_t>(sizeof(T)), static_cast<uint16_t>(count), 0,
                  checksum(reinterpret_cast<const uint8_t *>(records), bytes)};
    bool ok = file.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header);
    if (ok && bytes) ok = file.write(reinterpret_cast<const uint8_t *>(records), bytes) == bytes;
    file.flush(); file.close();
    if (!ok) { SD.remove(temporary); return false; }
    SD.remove(path);
    return SD.rename(temporary, path);
}
}
