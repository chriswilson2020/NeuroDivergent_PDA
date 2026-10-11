#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "data/CalendarStore.h"
#include "data/TaskStore.h"
#include "data/AssignmentStore.h"
#include "data/PackingStore.h"
#include "data/RoutineStore.h"
#include "data/NoteStore.h"
#include "data/TimerStore.h"
#include "data/HabitStore.h"
#include "data/MessageStore.h"

// Version-1 native companion schemas. Fail the firmware build if a C++ ABI or
// model change would otherwise silently make the desktop editor corrupt data.
static_assert(sizeof(CalendarEvent)==76 && offsetof(CalendarEvent,title)==15 && offsetof(CalendarEvent,location)==55);
static_assert(sizeof(TaskRecord)==156 && offsetof(TaskRecord,title)==12 && offsetof(TaskRecord,notes)==60);
static_assert(sizeof(AssignmentRecord)==352 && offsetof(AssignmentRecord,title)==14 && offsetof(AssignmentRecord,steps)==62);
static_assert(sizeof(PackingTemplate)==256 && offsetof(PackingTemplate,title)==29 && offsetof(PackingTemplate,items)==61);
static_assert(sizeof(RoutineRecord)==428 && offsetof(RoutineRecord,title)==5 && offsetof(RoutineRecord,steps)==41);
static_assert(sizeof(NoteRecord)==56 && offsetof(NoteRecord,title)==8);
static_assert(sizeof(TimerPreset)==44 && offsetof(TimerPreset,name)==12);
static_assert(sizeof(HabitRecord)==48 && offsetof(HabitRecord,name)==16);
static_assert(sizeof(MessageRecord)==192 && offsetof(MessageRecord,text)==24 && offsetof(MessageRecord,contactId)==8);

namespace DesktopBridge {
struct MessageDraft { uint64_t contact=0; uint32_t timestamp=0; char text[161]{}; };
inline uint32_t read32(const uint8_t *p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
inline bool parseMessage(const uint8_t *p,size_t size,MessageDraft &out) {
    if(size<30||size>189||read32(p)!=0x444D5131||p[4]!=1||p[5]!=0||p[7]!=0) return false;
    const size_t length=p[6];
    if(!length||length>160||size!=29+length||p[28+length]!=0||memchr(p+28,0,length))return false;
    uint32_t hash=2166136261u;
    for(size_t i=8;i<size;++i)if(i<24||i>=28){hash^=p[i];hash*=16777619u;}
    if(hash!=read32(p+24)||read32(p+16)!=0)return false;
    uint64_t contact=0;for(size_t i=0;i<8;++i)contact|=uint64_t(p[8+i])<<(8*i);
    if(!contact)return false;
    out.contact=contact;out.timestamp=read32(p+20);memcpy(out.text,p+28,length);out.text[length]=0;
    return true;
}
inline bool draftName(const char *name) {
    if(strlen(name)!=36||strcmp(name+32,".msg"))return false;
    for(size_t i=0;i<32;++i)if(!((name[i]>='0'&&name[i]<='9')||(name[i]>='a'&&name[i]<='f')))return false;
    return true;
}
}
