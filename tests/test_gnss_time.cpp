#include "core/GnssTimeEngine.h"
#include "core/Deadline.h"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace GnssTime;
static tm civil(int year,int month,int day,int hour,int minute,int second=0) {
    tm t{};t.tm_year=year-1900;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;t.tm_sec=second;t.tm_isdst=-1;return t;
}
static std::vector<uint8_t> packet(time_t epoch,uint32_t tow,uint8_t valid=0x37) {
    tm t{};gmtime_r(&epoch,&t);std::vector<uint8_t> p={0xb5,0x62,1,0x21,20,0};p.resize(26);
    for(unsigned i=0;i<4;++i){p[6+i]=(tow>>(8*i))&255;p[10+i]=(100000>>(8*i))&255;}
    const unsigned y=t.tm_year+1900;p[18]=y&255;p[19]=y>>8;p[20]=t.tm_mon+1;p[21]=t.tm_mday;
    p[22]=t.tm_hour;p[23]=t.tm_min;p[24]=t.tm_sec;p[25]=valid;
    uint8_t a=0,b=0;for(size_t i=2;i<p.size();++i){a+=p[i];b+=a;}p.push_back(a);p.push_back(b);return p;
}
struct FakePort:Port {
    int64_t us=0;time_t wall=TimeBasis::utc(civil(2026,10,10,12,0));bool trusted=true,powered=false,powerOk=true,saveOk=true,writeOk=true;
    unsigned writes=0,saves=0,powerOns=0;History persisted{};
    int64_t monotonicUs()const override{return us;}
    bool readRtc(time_t &out)override{if(trusted)out=wall;return trusted;}
    bool writeRtc(time_t value)override{++writes;if(!writeOk)return false;wall=value;trusted=true;return true;}
    bool save(const History &h)override{++saves;if(!saveOk)return false;persisted=h;return true;}
    bool power(bool value)override{powered=value;if(value)++powerOns;return powerOk;}
    void advance(unsigned seconds){us+=int64_t(seconds)*1000000;wall+=seconds;}
};
static void send(Engine &e,const std::vector<uint8_t> &p){for(auto b:p)e.feed(b);}
static void synchronize(int drift,bool correction) {
    FakePort p;Engine e(p);e.begin({},{});assert(!p.powered);const time_t satellite=p.wall-drift;
    assert(e.start(true,true,false,true,80));assert(p.powered&&p.persisted.result==Result::Searching);
    for(unsigned i=1;i<=3;++i){p.advance(1);e.update(true,false,true,80);send(e,packet(satellite+i,100000+i*1000));}
    assert(!e.active()&&!p.powered);assert(p.writes==unsigned(correction));assert(e.history().drift==drift);
    assert(e.history().result==(correction?Result::Corrected:Result::Checked));
    assert(e.waitSeconds()==14400-3);
}
int main() {
    TimeBasis::configure();
    time_t local=0;assert(TimeBasis::local(civil(2026,7,1,0,30),local));
    tm utc{};gmtime_r(&local,&utc);assert(utc.tm_mday==30&&utc.tm_mon==5&&utc.tm_hour==22);
    assert(!TimeBasis::local(civil(2026,3,29,2,30),local));
    const time_t gap=TimeBasis::scheduled(civil(2026,3,29,2,30));tm view{};localtime_r(&gap,&view);assert(view.tm_hour==3&&view.tm_min==0);
    assert(TimeBasis::local(civil(2026,10,25,2,30),local));assert(local==TimeBasis::utc(civil(2026,10,25,0,30)));
    assert(Deadline::daily(local,2,30)>local+23*3600); // don't repeat second fold
    assert(TimeBasis::valid(civil(2028,2,29,0,0)));assert(!TimeBasis::valid(civil(2026,2,29,0,0)));
    synchronize(1,false);synchronize(2,false);synchronize(5,true);synchronize(10,true);synchronize(-5,true);
    FakePort p;Engine e(p);e.begin({},{});p.advance(5);e.update(true,false,true,19);assert(!e.active()&&!p.powered);
    e.update(false,false,true,80);assert(!e.active());e.update(true,true,true,80);assert(!e.active());
    e.update(true,false,true,80);assert(e.active());p.advance(60);e.update(true,false,true,80);
    assert(e.history().result==Result::Timeout&&!p.powered&&e.waitSeconds()==14340);
    // A reset uses persisted attempt time, and not the last successful time.
    Engine reboot(p);reboot.begin({},p.persisted);reboot.update(true,false,true,80);assert(!reboot.active());
    p.trusted=false;Engine uncertain(p);uncertain.begin({},p.persisted);uncertain.update(true,false,true,80);assert(!uncertain.active());
    p.advance(14400);uncertain.update(true,false,true,80);assert(uncertain.active());uncertain.cancel();assert(!p.powered);
    assert(e.start(true,true,false,true,80));e.update(true,true,true,80);assert(!p.powered&&e.history().result==Result::Cancelled);
    assert(e.start(true,true,false,true,80));e.update(false,false,true,80);assert(!p.powered);
    assert(e.start(true,true,false,true,80));e.update(true,false,true,4);assert(!p.powered&&e.history().result==Result::LowBattery);
    p.saveOk=false;assert(!e.start(true,true,false,true,80));assert(!p.powered);p.saveOk=true;
    Parser parser;Sample sample{};const auto good=packet(p.wall,123000);
    for(auto b:good)parser.feed(b,sample);assert(sample.utc==p.wall);
    auto bad=good;bad.back()^=1;sample={};for(auto b:bad)assert(!parser.feed(b,sample));
    for(auto b:packet(p.wall,123000,0x33))assert(!parser.feed(b,sample)); // UTC invalid
    for(auto b:packet(p.wall,123000,0x07))assert(!parser.feed(b,sample)); // unknown UTC standard
    const char *nmea="$GPRMC,120000,A,0000,N,0000,E,0,0,101026,,,A*00\r\n";for(const char *c=nmea;*c;++c)assert(!parser.feed(*c,sample));
    // Very large corrections require six progressing observations.
    p.trusted=true;p.writeOk=true;assert(e.start(true,true,false,true,80));time_t sat=p.wall-3600;
    for(unsigned i=1;i<=6;++i){p.advance(1);e.update(true,false,true,80);send(e,packet(sat+i,200000+i*1000));if(i<6)assert(e.active());}
    assert(!e.active()&&!p.powered&&e.history().result==Result::Corrected);
    // A stale replay cannot satisfy the consecutive-fresh-sample requirement.
    assert(e.start(true,true,false,true,80));const auto stale=packet(p.wall,300000);
    for(int i=0;i<10;++i){p.advance(1);e.update(true,false,true,80);send(e,stale);assert(e.active());}
    e.cancel();assert(!p.powered);
    // Manual acquisition remains bounded even with automatic sync disabled.
    TimeSyncPreferences disabled{};disabled.automatic=0;Engine manual(p);manual.begin(disabled,{});
    assert(!manual.start(true,true,false,false,80));assert(manual.start(true,true,false,true,80));
    p.advance(179);manual.update(true,false,true,80);assert(manual.active());p.advance(1);manual.update(true,false,true,80);
    assert(!manual.active()&&!p.powered&&manual.history().result==Result::Timeout);
    // Readback failure must release the GNSS receiver and preserve old RTC.
    assert(e.start(true,true,false,true,80));p.writeOk=false;sat=p.wall-10;const time_t before=p.wall;
    for(unsigned i=1;i<=3;++i){p.advance(1);e.update(true,false,true,80);send(e,packet(sat+i,400000+i*1000));}
    assert(!p.powered&&!e.active()&&e.history().result==Result::RtcError&&p.wall==before+3);p.writeOk=true;
    // A failed rail-off readback is retried, not advertised as safely idle.
    assert(e.start(true,true,false,true,80));p.powerOk=false;e.cancel();assert(e.state()==State::PoweringOff);
    p.powerOk=true;e.update(false,false,true,80);assert(!e.active()&&!p.powered);
    puts("GNSS engine, protocol and timezone tests passed");
}
