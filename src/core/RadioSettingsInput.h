#pragma once
#include "data/MessagingSettingsStore.h"

namespace RadioSettingsInput {
inline bool number(const char*text,uint32_t min,uint32_t max,uint32_t&out){
    if(!text||!*text)return false;
    uint32_t value=0;
    for(const char*p=text;*p;++p){
        if(*p<'0'||*p>'9')return false;
        const uint32_t digit=static_cast<uint32_t>(*p-'0');
        if(value>max/10||(value==max/10&&digit>max%10))return false;
        value=value*10+digit;
    }
    if(value<min)return false;
    out=value;return true;
}
// Validate every field before mutating the candidate. No empty-field defaults,
// clipping, partial numbers, signs or overflowing numeric conversions.
inline const char*validate(const char*frequency,const char*sf,const char*power,const char*retry,MessagingSettings&candidate){
    uint32_t f,s,p,r;
    if(!number(frequency,863000,870000,f))return "Frequency must be 863000-870000 kHz.";
    if(!number(sf,7,12,s))return "SF must be a whole number from 7 to 12.";
    if(!number(power,2,14,p))return "Power must be a whole number from 2 to 14 dBm.";
    if(!number(retry,10,300,r))return "Retry must be a whole number from 10 to 300 seconds.";
    candidate.frequencyKhz=f;candidate.spreadingFactor=s;candidate.powerDbm=p;candidate.retrySeconds=r;
    return nullptr;
}
}
