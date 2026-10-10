#pragma once
#include "Wire.h"
#include <time.h>
struct TestRtc {
    unsigned writes=0;
    void setDateTime(const tm &t){++writes;if(Wire.writeOk)Wire.set(t);}
    void disableAlarm(){}void resetAlarm(){}void enableAlarm(){}
    int hour=-1,minute=-1;
    void setAlarm(int h,int m,int,int,int){hour=h;minute=m;}
};
struct TestInstance { TestRtc rtc; };
inline TestInstance instance;
struct TestSerial {template<class...Args>void printf(const char*,Args...){}void println(const char*){}};
inline TestSerial Serial;
