#pragma once
#include <freertos/FreeRTOS.h>

class SPIBusManager {
public:
    bool lock(TickType_t wait = portMAX_DELAY);
    void unlock();
    class Guard {
    public:
        explicit Guard(SPIBusManager &bus,TickType_t wait=portMAX_DELAY) : bus_(bus), locked_(bus_.lock(wait)) {}
        ~Guard() { if (locked_) bus_.unlock(); }
        explicit operator bool() const { return locked_; }
    private: SPIBusManager &bus_; bool locked_;
    };
};
