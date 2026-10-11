#pragma once
#include <cstddef>
#include <cstdint>
inline void esp_fill_random(void *data,size_t size) {
    static uint8_t seed=0;auto *p=static_cast<uint8_t*>(data);
    for(size_t i=0;i<size;++i)p[i]=++seed;
}
