#include "core/SleepTransition.h"
#include "core/Deadline.h"
#include <cassert>
#include <cstdio>
#include <array>

struct FakeBackend {
    std::array<SleepTransition::InterruptState,49> config{};
    std::array<SleepTransition::InterruptState,49> original{};
    std::array<bool,49> wakeOn{},levels{},high{};
    bool gpio=false,timerOn=false;
    int calls=0,failAt=0;
    uint64_t timerUs=0;
    bool failRestore=false;
    FakeBackend() {
        levels.fill(true);levels[14]=false;
        config[14]={1,true,true};config[6]={3,true,false};config[1]={2,true,true};
        // Include a configured-but-masked interrupt: it must stay masked.
        config[7]={3,false,true};original=config;
    }
    int result(){return ++calls==failAt?-1:0;}
    int snapshot(int pin,SleepTransition::InterruptState &out){int e=result();if(!e)out=config[pin];return e;}
    int mask(int pin){int e=result();if(!e)config[pin].enabled=false;return e;}
    int unmask(int pin){int e=result();if(!e){assert(config[pin].type==original[pin].type);config[pin].enabled=true;}return e;}
    int level(int pin)const{return levels[pin];}
    int wake(int pin,bool h){
        // No normal ISR may run with the sleep-only level trigger installed.
        for(int p:SleepTransition::pins)assert(!config[p].enabled);
        int e=result();if(!e){wakeOn[pin]=true;high[pin]=h;config[pin].type=h?5:4;}return e;
    }
    int unwake(int pin){wakeOn[pin]=false;return 0;}
    int restoreType(int pin,const SleepTransition::InterruptState &s){if(failRestore&&pin==6)return -2;config[pin].type=s.type;config[pin].sleepSelected=s.sleepSelected;return 0;}
    int enableGpioWake(){int e=result();if(!e)gpio=true;return e;}
    int disableGpioWake(){gpio=false;return 0;}
    int enableTimerWake(uint64_t us){int e=result();if(!e){timerOn=true;timerUs=us;}return e;}
    int disableTimerWake(){timerOn=false;return 0;}
    void restored()const{
        assert(!gpio&&!timerOn);
        for(int p:SleepTransition::pins){assert(!wakeOn[p]);assert(config[p].type==original[p].type);assert(config[p].enabled==original[p].enabled);assert(config[p].sleepSelected==original[p].sleepSelected);}
    }
};
int main(){
    // Success / early veto / sleep rejection all unwind the same guard.
    for(int disposition=0;disposition<3;++disposition){
        FakeBackend b;
        {SleepTransition::WakeGuard<FakeBackend> g(b);assert(!g.arm());assert(!g.asserted());assert(!g.timer(12000));assert(b.timerUs==12000);
         if(disposition)assert(!g.restore());}
        b.restored();
    }
    // Fail every snapshot, mask, pin-wake, global-wake and timer setup step.
    for(int fail=1;fail<=23;++fail){
        FakeBackend b;b.failAt=fail;
        {SleepTransition::WakeGuard<FakeBackend> g(b);if(!g.arm())g.timer(10000);}
        b.restored();
    }
    {
        FakeBackend b;SleepTransition::WakeGuard<FakeBackend> g(b);assert(!g.arm());b.failRestore=true;
        assert(g.restore()==-2);
        for(int pin:SleepTransition::pins)assert(!b.config[pin].enabled); // no level-ISR storm on failed restore
    }
    // Packet or input arrives after normal IRQ delivery was masked: the
    // armed level remains asserted and the caller vetoes/returns from sleep.
    for(int pin:SleepTransition::pins){
        FakeBackend b;
        {SleepTransition::WakeGuard<FakeBackend> g(b);assert(!g.arm());b.levels[pin]=b.high[pin];assert(g.asserted()&(1ULL<<pin));}
        b.restored();
    }
    // Opposite-level wheel wake works for every resting quadrature state.
    for(int bits=0;bits<4;++bits){
        FakeBackend b;b.levels[40]=bits&1;b.levels[41]=bits&2;
        {SleepTransition::WakeGuard<FakeBackend> g(b);assert(!g.arm());assert(!g.asserted());b.levels[40]=!b.levels[40];assert(g.asserted()&(1ULL<<40));}
        b.restored();
    }
    // Deadline earlier than maintenance ceiling always wins, with elapsed
    // preflight/setup time deducted. Expired/too-close deadlines never sleep.
    assert(Deadline::sleepBudgetUs(10000,15000,1000000)==3000);
    assert(Deadline::sleepBudgetUs(14000,15000,1000000)==0);
    assert(Deadline::sleepBudgetUs(16000,15000,1000000)==0);
    assert(Deadline::sleepBudgetUs(10000,0,20000)==8000);
    const int64_t wallDue=Deadline::wallDeadlineUs(100,102,10000);
    assert(wallDue==1010000);
    assert(Deadline::sleepBudgetUs(900000,wallDue,2000000)==108000);
    puts("sleep transition tests passed");
}
