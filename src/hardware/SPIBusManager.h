#pragma once
#include <freertos/FreeRTOS.h>

class SPIBusManager {
public:
    bool lock(TickType_t wait = portMAX_DELAY);
    void unlock();
    class Guard {
    public:
        explicit Guard(SPIBusManager &bus) : bus_(bus), locked_(bus_.lock()) {}
        ~Guard() { if (locked_) bus_.unlock(); }
        explicit operator bool() const { return locked_; }
    private: SPIBusManager &bus_; bool locked_;
    };
};
