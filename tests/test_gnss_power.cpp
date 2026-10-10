#include "hardware/GnssPowerReadback.h"
#include <assert.h>
#include <stdio.h>
struct Bus {
    uint8_t regs[8]{},reg=0;bool nack=false,shortRead=false;unsigned writes=0;
    void beginTransmission(uint8_t address){assert(address==0x20);}
    unsigned write(uint8_t value){reg=value;++writes;return 1;}
    unsigned endTransmission(bool stop){assert(!stop);return nack?2:0;}
    unsigned requestFrom(uint8_t address,uint8_t count){assert(address==0x20&&count==1);return shortRead?0:1;}
    int available(){return shortRead?0:1;}
    int read(){assert(reg<8);return regs[reg];}
    // Deliberately no digitalRead(): that API rejects OUTPUT pins.
};
int main(){
    using namespace GnssPowerReadback;
    for(uint8_t pin=0;pin<16;++pin){
        Bus b;const auto port=pin/8,mask=1<<(pin%8);
        b.regs[6+port]=uint8_t(~mask); // Only this pin is an output.
        assert(verify(b,pin,false)==Result::Ok);
        b.regs[2+port]=mask;b.regs[port]=mask;
        assert(verify(b,pin,true)==Result::Ok);
        b.regs[port]=0;assert(verify(b,pin,true)==Result::LevelMismatch);
        b.regs[2+port]=0;assert(verify(b,pin,true)==Result::LatchMismatch);
        b.regs[6+port]=0xff;assert(verify(b,pin,true)==Result::NotOutput);
        b.nack=true;assert(verify(b,pin,false)==Result::IoError);
        b.nack=false;b.shortRead=true;assert(verify(b,pin,false)==Result::IoError);
    }
    Bus b;assert(verify(b,16,true)==Result::NotOutput);assert(!b.writes);
    puts("GNSS output-pin readback tests passed");
}
