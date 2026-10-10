#pragma once
#include <time.h>
#include <stdint.h>
class RTCService {
public:
    time_t wall=0;uint32_t rev=0;
    bool now(tm &t)const{localtime_r(&wall,&t);return true;}
    uint32_t revision()const{return rev;}
};
