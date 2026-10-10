#pragma once
#include <time.h>
#include <stdint.h>
#include <stdlib.h>

// The RTC stores UTC. Application records remain local civil dates/times.
namespace TimeBasis {
inline void configure(uint8_t zone = 0) {
    setenv("TZ", zone == 1 ? "UTC0" : "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
}
inline bool leap(int y) { return y%4==0 && (y%100!=0 || y%400==0); }
inline bool valid(const tm &t) {
    static constexpr int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    const int y=t.tm_year+1900;
    return y>=2024 && y<=2099 && t.tm_mon>=0 && t.tm_mon<12 && t.tm_mday>=1 &&
        t.tm_mday<=days[t.tm_mon]+(t.tm_mon==1&&leap(y)) &&
        t.tm_hour>=0 && t.tm_hour<24 && t.tm_min>=0 && t.tm_min<60 && t.tm_sec>=0 && t.tm_sec<60;
}
inline time_t utc(const tm &t) {
    // Gregorian civil-to-days; independent of libc TZ and nonstandard timegm.
    int y=t.tm_year+1900; const unsigned m=t.tm_mon+1; y-=m<=2;
    const int era=(y>=0?y:y-399)/400;
    const unsigned yo=static_cast<unsigned>(y-era*400);
    const unsigned doy=(153*(m>2?m-3:m+9)+2)/5+t.tm_mday-1;
    const unsigned doe=yo*365+yo/4-yo/100+doy;
    return static_cast<time_t>(era*146097+static_cast<int>(doe)-719468)*86400+
        t.tm_hour*3600+t.tm_min*60+t.tm_sec;
}
inline bool sameCivil(const tm &a,const tm &b) {
    return a.tm_year==b.tm_year&&a.tm_mon==b.tm_mon&&a.tm_mday==b.tm_mday&&
        a.tm_hour==b.tm_hour&&a.tm_min==b.tm_min&&a.tm_sec==b.tm_sec;
}
inline bool local(const tm &civil,time_t &out) {
    // Fold: first occurrence. Gap: reject for manual entry; scheduled events
    // use scheduled() below to move to the first valid following minute.
    bool found=false;
    for(int dst=0;dst<=1;++dst) {
        tm trial=civil;trial.tm_isdst=dst;
        const time_t epoch=mktime(&trial);tm check{};localtime_r(&epoch,&check);
        if(sameCivil(civil,check)&&(!found||epoch<out)){out=epoch;found=true;}
    }
    return found;
}
inline time_t scheduled(tm civil) {
    // Normalize date arithmetic without normalizing a DST-gap time first.
    tm noon=civil;noon.tm_hour=12;noon.tm_min=0;noon.tm_sec=0;noon.tm_isdst=-1;mktime(&noon);
    civil.tm_year=noon.tm_year;civil.tm_mon=noon.tm_mon;civil.tm_mday=noon.tm_mday;
    time_t result=0;if(local(civil,result))return result;
    const time_t naive=utc(civil);
    for(int minutes=1;minutes<=180;++minutes) {
        const time_t next=naive+minutes*60;tm candidate{};gmtime_r(&next,&candidate);
        if(local(candidate,result))return result;
    }
    return 0;
}
}
