#pragma once
#include <stdint.h>

namespace GnssPowerReadback {
enum class Result { Ok, IoError, NotOutput, LatchMismatch, LevelMismatch };

// XL9555 digitalRead() in SensorLib only accepts INPUT pins. GNSS_EN is
// OUTPUT: read the chip registers directly without changing its pin mode.
// This verifies the enable signal, not the receiver's supply voltage.
template<class Bus> bool read(Bus &bus, uint8_t reg, uint8_t &value) {
    bus.beginTransmission(0x20);
    if(bus.write(reg)!=1 || bus.endTransmission(false)!=0) return false;
    if(bus.requestFrom(uint8_t(0x20),uint8_t(1))!=1 || bus.available()<1) return false;
    const int byte=bus.read();
    if(byte<0) return false;
    value=uint8_t(byte);return true;
}
template<class Bus> Result verify(Bus &bus, uint8_t pin, bool enabled) {
    if(pin>=16) return Result::NotOutput;
    const uint8_t port=pin/8, mask=uint8_t(1U<<(pin%8));
    uint8_t config=0,latch=0,level=0;
    if(!read(bus,uint8_t(6+port),config)) return Result::IoError;
    if(config&mask) return Result::NotOutput;
    if(!read(bus,uint8_t(2+port),latch)) return Result::IoError;
    if(bool(latch&mask)!=enabled) return Result::LatchMismatch;
    if(!read(bus,port,level)) return Result::IoError;
    return bool(level&mask)==enabled ? Result::Ok : Result::LevelMismatch;
}
inline const char *error(Result result) {
    switch(result) {
    case Result::Ok:return "";
    case Result::IoError:return "GNSS enable readback: I2C communication failed.";
    case Result::NotOutput:return "GNSS enable pin is not configured as an output.";
    case Result::LatchMismatch:return "GNSS enable output latch did not change.";
    case Result::LevelMismatch:return "GNSS enable signal does not match its output latch.";
    }
    return "GNSS power verification failed.";
}
}
