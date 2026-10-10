#pragma once
#include <stdint.h>
#include <time.h>
struct TestWire {
    uint8_t regs[11]{};unsigned at=0;bool readOk=true,writeOk=true;
    void beginTransmission(int){}
    void write(uint8_t address){at=address;}
    int endTransmission(bool){return readOk?0:1;}
    int requestFrom(uint8_t,uint8_t length){return readOk?length:0;}
    int read(){return regs[at++];}
    void set(const tm &t){
        auto bcd=[](unsigned n)->uint8_t{return ((n/10)<<4)|(n%10);};
        regs[0]=0;regs[4]=bcd(t.tm_sec);regs[5]=bcd(t.tm_min);regs[6]=bcd(t.tm_hour);
        regs[7]=bcd(t.tm_mday);regs[9]=bcd(t.tm_mon+1);regs[10]=bcd(t.tm_year-100);
    }
};
inline TestWire Wire;
