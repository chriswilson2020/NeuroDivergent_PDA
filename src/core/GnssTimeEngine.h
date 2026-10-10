#pragma once
#include "GnssTimeProtocol.h"
#include "data/TimeSyncPreferences.h"

namespace GnssTime {
enum class State:uint8_t { Disabled,Waiting,PoweringOn,Acquiring,Validating,Comparing,PoweringOff };
enum class Result:uint8_t { Never,Searching,Checked,Corrected,Timeout,Cancelled,Unavailable,StorageError,RtcError,LowBattery,Interrupted };
struct History {
    uint32_t version=1;
    int64_t attempted=0,successful=0,drift=0;
    Result result=Result::Never;
    uint8_t hasAttempt=0;
    uint8_t driftKnown=0;
    uint8_t reserved[5]{};
};
class Port {
public:
    virtual ~Port()=default;
    virtual int64_t monotonicUs()const=0;
    virtual bool readRtc(time_t &value)=0;
    virtual bool writeRtc(time_t value)=0;
    virtual bool save(const History &value)=0;
    virtual bool power(bool enabled)=0;
};
class Engine {
public:
    explicit Engine(Port &port):port_(port){}
    void begin(const TimeSyncPreferences &prefs,const History &history) {
        prefs_=prefs;history_=history;
        if(history_.version!=1||history_.hasAttempt>1||history_.driftKnown>1||static_cast<unsigned>(history_.result)>static_cast<unsigned>(Result::Interrupted)||
            (history_.attempted&&(history_.attempted<1704067200||history_.attempted>=4102444800LL))||
            (history_.successful&&(history_.successful<1704067200||history_.successful>=4102444800LL))||
            history_.drift<-5000000000LL||history_.drift>5000000000LL)history_={};
        port_.power(false);time_t wall=0;
        int64_t delayUs=history_.hasAttempt?intervalUs():5000000;
        if(history_.hasAttempt&&port_.readRtc(wall)&&history_.attempted>0&&wall>=history_.attempted)
            delayUs=intervalUs()-(static_cast<int64_t>(wall)-history_.attempted)*1000000;
        eligibleUs_=port_.monotonicUs()+(delayUs>0?delayUs:0);
        if(history_.result==Result::Searching){history_.result=Result::Interrupted;port_.save(history_);}
        state_=prefs_.automatic?State::Waiting:State::Disabled;
    }
    void configure(const TimeSyncPreferences &prefs) {
        const auto old=intervalUs();prefs_=prefs;
        if(history_.hasAttempt)eligibleUs_+=intervalUs()-old;
        if(!prefs_.automatic&&!manual_)cancel();
        if(!active())state_=prefs_.automatic?State::Waiting:State::Disabled;
    }
    bool start(bool manual,bool awake,bool usb,bool batteryKnown,uint8_t battery) {
        if(active()||!awake||usb)return false;
        if(!batteryKnown||battery<(manual?5:20))return false;
        time_t wall=0;port_.readRtc(wall);
        history_.attempted=wall;history_.hasAttempt=1;history_.result=Result::Searching;
        startedUs_=port_.monotonicUs();eligibleUs_=startedUs_+intervalUs();manual_=manual;
        timeoutUs_=startedUs_+(manual?180000000:60000000);validator_.reset();parser_.reset();
        // Write BEFORE power-on so resets never turn into repeated searches.
        if(!port_.save(history_)){history_.result=Result::StorageError;return false;}
        state_=State::PoweringOn;
        if(!port_.power(true)){finish(Result::Unavailable);return false;}
        return true;
    }
    void update(bool awake,bool usb,bool batteryKnown,uint8_t battery) {
        if(state_==State::PoweringOff){finish(history_.result);return;}
        if(active()) {
            if(usb||!awake){finish(Result::Cancelled);return;}
            if(!batteryKnown||battery<(manual_?5:20)){finish(Result::LowBattery);return;}
            if(port_.monotonicUs()>=timeoutUs_){finish(Result::Timeout);return;}
            if(state_==State::PoweringOn&&port_.monotonicUs()-startedUs_>=200000)state_=State::Acquiring;
        } else if(prefs_.automatic&&port_.monotonicUs()>=eligibleUs_)start(false,awake,usb,batteryKnown,battery);
    }
    void feed(uint8_t byte) {
        if(state_!=State::Acquiring&&state_!=State::Validating)return;
        Sample sample;if(!parser_.feed(byte,sample))return;
        const int64_t observedUs=port_.monotonicUs();time_t rtc=0;
        const bool trusted=port_.readRtc(rtc);
        const bool large=!trusted||llabs(static_cast<int64_t>(rtc)-sample.utc)>300;
        state_=State::Validating;
        if(!validator_.observe(sample,observedUs,large))return;
        state_=State::Comparing;
        const int64_t ageUs=port_.monotonicUs()-observedUs;
        if(ageUs<0||ageUs>1000000){validator_.reset();state_=State::Acquiring;return;}
        const time_t candidate=sample.utc+(static_cast<int64_t>(sample.nano)+ageUs*1000)/1000000000;
        history_.drift=trusted?static_cast<int64_t>(rtc)-candidate:0;
        history_.driftKnown=trusted;
        const bool correct=!trusted||llabs(history_.drift)>2;
        if(correct&&!port_.writeRtc(candidate)){finish(Result::RtcError);return;}
        history_.successful=candidate;
        // Rebase the attempt timestamp onto the newly verified UTC basis.
        history_.attempted=candidate-(port_.monotonicUs()-startedUs_)/1000000;
        finish(correct?Result::Corrected:Result::Checked);
    }
    void cancel(){if(active())finish(Result::Cancelled);}
    bool active()const{return state_!=State::Disabled&&state_!=State::Waiting;}
    State state()const{return state_;}
    const History &history()const{return history_;}
    uint32_t elapsedSeconds()const{return active()?(port_.monotonicUs()-startedUs_)/1000000:0;}
    uint32_t waitSeconds()const {const auto left=eligibleUs_-port_.monotonicUs();return left>0?(left+999999)/1000000:0;}
    unsigned samples()const{return validator_.count();}
    unsigned frames()const{return parser_.frames();}
    uint32_t maximumSeconds()const{return static_cast<uint32_t>((timeoutUs_-startedUs_)/1000000);}
private:
    int64_t intervalUs()const{return int64_t(prefs_.intervalHours)*3600000000LL;}
    void finish(Result result) {
        state_=State::PoweringOff;
        const bool off=port_.power(false);
        history_.result=off?result:Result::Unavailable;
        if(!port_.save(history_))history_.result=Result::StorageError;
        if(off){state_=prefs_.automatic?State::Waiting:State::Disabled;manual_=false;}
    }
    Port &port_;TimeSyncPreferences prefs_{};History history_{};Parser parser_;Validator validator_;
    State state_=State::Disabled;int64_t startedUs_=0,timeoutUs_=0,eligibleUs_=0;bool manual_=false;
};
inline const char *resultName(Result r) {
    switch(r){case Result::Never:return "Not checked";case Result::Searching:return "Searching";
    case Result::Checked:return "Clock within 2 seconds";case Result::Corrected:return "Clock corrected";
    case Result::Timeout:return "No validated time (timeout)";case Result::Cancelled:return "Cancelled";
    case Result::Unavailable:return "GNSS unavailable";case Result::StorageError:return "History storage error";
    case Result::RtcError:return "RTC write/readback failed";case Result::LowBattery:return "Low battery";
    case Result::Interrupted:return "Search interrupted by reboot";}return "Unknown";
}
}
