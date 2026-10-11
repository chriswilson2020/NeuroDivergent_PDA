#include "core/DesktopBridge.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

struct Header { uint32_t magic; uint16_t version,size,count,reserved; uint32_t checksum; };
static_assert(sizeof(Header)==16);
template<typename T> void fixture(const std::string &root,const char *name,uint32_t magic,T record,size_t titleOffset,const char *title) {
    *reinterpret_cast<uint32_t*>(&record)=41;
    strcpy(reinterpret_cast<char*>(&record)+titleOffset,title);
    uint32_t hash=2166136261u;auto *p=reinterpret_cast<uint8_t*>(&record);
    for(size_t i=0;i<sizeof(record);++i){hash^=p[i];hash*=16777619u;}
    Header h{magic,1,sizeof(T),1,0,hash};std::ofstream f(root+"/"+name+".dat",std::ios::binary);
    f.write(reinterpret_cast<char*>(&h),sizeof(h));f.write(reinterpret_cast<char*>(&record),sizeof(record));assert(f.good());
}
int main(int argc,char **argv) {
    assert(argc==2||argc==3);std::string root=argv[1];
    if(argc==3){
        std::ifstream file(root+"/swift-draft.msg",std::ios::binary);std::vector<uint8_t> p((std::istreambuf_iterator<char>(file)),{});
        DesktopBridge::MessageDraft d;assert(DesktopBridge::parseMessage(p.data(),p.size(),d));assert(d.contact==0x1122334455667788ULL);assert(!strcmp(d.text,"Hello from the desktop"));
        p.back()=1;assert(!DesktopBridge::parseMessage(p.data(),p.size(),d));
        p.back()=0;p[8]^=1;assert(!DesktopBridge::parseMessage(p.data(),p.size(),d));
        for(size_t n=0;n<29;++n)assert(!DesktopBridge::parseMessage(p.data(),n,d));
        assert(DesktopBridge::draftName("0123456789abcdef0123456789abcdef.msg"));assert(!DesktopBridge::draftName("../draft.msg"));
        std::cout<<"Swift-to-firmware message protocol tests passed\n";return 0;
    }
    CalendarEvent event;event.year=2026;event.month=10;event.day=10;event.recurrence=190;strcpy(event.location,"e0066");
    fixture(root,"calendar",0x43414C31,event,offsetof(CalendarEvent,title),"Mathematics");
    fixture(root,"tasks",0x54415331,TaskRecord{},offsetof(TaskRecord,title),"Pack bag");
    fixture(root,"assignments",0x41534731,AssignmentRecord{},offsetof(AssignmentRecord,title),"Science project");
    fixture(root,"packing",0x50414331,PackingTemplate{},offsetof(PackingTemplate,title),"PE kit");
    fixture(root,"routines",0x52544E31,RoutineRecord{},offsetof(RoutineRecord,title),"Morning");
    fixture(root,"notes",0x4E4F5431,NoteRecord{},offsetof(NoteRecord,title),"Remember this");
    fixture(root,"clock",0x54494D31,TimerPreset{},offsetof(TimerPreset,name),"Focus");
    HabitRecord habit;habit.streak=7;habit.lastCompletedDate=20261010;
    fixture(root,"habits",0x48414231,habit,offsetof(HabitRecord,name),"Drink water");
    MessageRecord msg;msg.contactId=0x1122334455667788ULL;msg.messageId=9;msg.timestamp=1791630000;msg.unread=1;
    fixture(root,"messages",0x4D534731,msg,offsetof(MessageRecord,text),"Hello from the pager");
    std::cout<<"Firmware-format fixtures generated\n";
}
