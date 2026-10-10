#pragma once
#include "TimeBasis.h"
#include <stddef.h>

namespace GnssTime {
struct Sample { time_t utc=0; uint32_t tow=0; int32_t nano=0; uint32_t accuracyNs=0; };
// UBX NAV-TIMEUTC, or NAV-PVT with explicit confirmation. No NMEA timestamp
// or position is retained. M10 firmware need not support PVT confirmedAvai.
class Parser {
public:
    void reset() { resetFrame();frames_=0; }
    unsigned frames()const{return frames_;}
    bool feed(uint8_t b,Sample &out) {
        switch(state_) {
        case 0: if(b==0xb5)state_=1;break;
        case 1: state_=b==0x62?2:(b==0xb5?1:0);a_=c_=0;break;
        case 2: cls_=b;sum(b);state_=3;break;
        case 3: id_=b;sum(b);state_=4;break;
        case 4: length_=b;sum(b);state_=5;break;
        case 5: length_|=uint16_t(b)<<8;sum(b);position_=0;
            if(length_>sizeof(payload_)){resetFrame();break;}state_=length_?6:7;break;
        case 6: payload_[position_++]=b;sum(b);if(position_==length_)state_=7;break;
        case 7: if(b!=a_){resetFrame();break;}state_=8;break;
        case 8: {const bool checksum=b==c_;if(checksum)++frames_;const bool ok=checksum&&decode(out);resetFrame();return ok;}
        }
        return false;
    }
private:
    void resetFrame(){state_=0;position_=length_=0;}
    void sum(uint8_t b){a_+=b;c_+=a_;}
    uint32_t u32(unsigned n)const{return uint32_t(payload_[n])|(uint32_t(payload_[n+1])<<8)|(uint32_t(payload_[n+2])<<16)|(uint32_t(payload_[n+3])<<24);}
    bool decode(Sample &out)const {
        const auto *p=payload_;
        if(cls_==1&&id_==0x21&&length_==20) {
            const unsigned standard=p[19]>>4;
            if((p[19]&7)!=7||standard==0||standard==15||u32(4)>100000000)return false;
            tm t{};t.tm_year=(p[12]|(uint16_t(p[13])<<8))-1900;t.tm_mon=p[14]-1;t.tm_mday=p[15];
            t.tm_hour=p[16];t.tm_min=p[17];t.tm_sec=p[18];
            const int32_t nano=static_cast<int32_t>(u32(8));
            if(!TimeBasis::valid(t)||nano<=-1000000000||nano>=1000000000||u32(0)>=604800000)return false;
            out={TimeBasis::utc(t),u32(0),nano,u32(4)};return true;
        }
        if(cls_!=1||id_!=7||length_!=92)return false;
        // validDate + validTime + fullyResolved, confirmedDate/Time available
        // and confirmed, gnssFixOK. Explicit time-only fix (5) is acceptable.
        if((p[11]&7)!=7||(p[22]&0xe0)!=0xe0||!(p[21]&1)||p[20]<2||p[20]>5||u32(12)>100000000)return false;
        tm t{};t.tm_year=(p[4]|(uint16_t(p[5])<<8))-1900;t.tm_mon=p[6]-1;t.tm_mday=p[7];
        t.tm_hour=p[8];t.tm_min=p[9];t.tm_sec=p[10];
        const int32_t nano=static_cast<int32_t>(u32(16));
        // Leap seconds (second 60), impossible civil dates and approximate
        // startup data are deferred, never normalized into an accepted time.
        if(!TimeBasis::valid(t)||nano<=-1000000000||nano>=1000000000||u32(0)>=604800000)return false;
        out={TimeBasis::utc(t),u32(0),nano,u32(12)};return true;
    }
    uint8_t payload_[92]{};uint16_t position_=0,length_=0;uint8_t state_=0,cls_=0,id_=0,a_=0,c_=0;
    unsigned frames_=0;
};
class Validator {
public:
    void reset(){count_=0;}
    unsigned count()const{return count_;}
    bool observe(const Sample &sample,int64_t receivedUs,bool large) {
        if(count_) {
            if(sample.tow==last_.tow&&sample.utc==last_.utc)return false;
            int64_t tow=static_cast<int64_t>(sample.tow)-last_.tow;
            if(tow<0)tow+=604800000;
            const int64_t utcUs=(static_cast<int64_t>(sample.utc)-last_.utc)*1000000+(sample.nano-last_.nano)/1000;
            const int64_t elapsed=receivedUs-lastUs_;
            if(tow<500||tow>2500||elapsed<400000||elapsed>3000000||
                llabs(utcUs-tow*1000)>150000||llabs(utcUs-elapsed)>400000) {
                count_=0;
            }
        }
        last_=sample;lastUs_=receivedUs;++count_;
        return count_>=(large?6u:3u);
    }
private: Sample last_{};int64_t lastUs_=0;unsigned count_=0;
};
// Pure eligibility model: wall time is used only when trusted. Monotonic
// cooldown is authoritative within a boot, even after an RTC correction.
inline bool eligible(int64_t nowUs,int64_t eligibleUs,bool trusted,time_t now,time_t lastAttempt,uint32_t interval) {
    if(nowUs<eligibleUs)return false;
    return !lastAttempt || (trusted&&now>=lastAttempt&&static_cast<uint64_t>(now-lastAttempt)>=interval);
}
}
