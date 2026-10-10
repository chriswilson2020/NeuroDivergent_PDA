#include "core/Deadline.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
static time_t at(int y,int m,int d,int h,int min,int sec=0){
    tm t{};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=min;t.tm_sec=sec;t.tm_isdst=-1;return mktime(&t);
}
int main(){
    setenv("TZ","Europe/Amsterdam",1);tzset();
    const time_t midnight=at(2026,10,10,0,0);
    assert(Deadline::daily(midnight-1,0,0)==midnight);
    assert(Deadline::daily(midnight,0,0)==at(2026,10,11,0,0));
    assert(Deadline::crossed(midnight,midnight-10,midnight+65)); // late wake, beyond old 60s window
    assert(!Deadline::crossed(midnight,midnight+65,midnight+66)); // no repeated notification
    int delivered=0;for(int i=0;i<3;++i)if(Deadline::crossed(midnight,midnight-1,midnight))++delivered;
    assert(delivered==3); // simultaneous events are all due
    const time_t spring=at(2026,3,28,9,0);
    assert(Deadline::daily(spring,9,0)-spring==23*3600);
    const time_t autumn=at(2026,10,24,9,0);
    assert(Deadline::daily(autumn,9,0)-autumn==25*3600);
    assert(Deadline::earliest(0,midnight)==midnight);
    assert(Deadline::earliest(midnight+5,midnight)==midnight);
    assert(Deadline::sleepBudgetMs(midnight,midnight,100)==0);
    assert(Deadline::sleepBudgetMs(midnight,midnight+1,100)==100);
    assert(Deadline::sleepBudgetMs(midnight,0,100)==100);
    puts("deadline tests passed");
}
