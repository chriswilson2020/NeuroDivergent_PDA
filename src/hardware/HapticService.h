#pragma once
#include <stdint.h>

class HapticService {
public:
    void begin(bool available) { available_ = available; }
    void play(uint8_t effect = 1);
    bool available() const { return available_; }
private:
    bool available_ = false;
};
