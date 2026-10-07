#include "TaskStore.h"
#include "StoreIO.h"
#include <cstring>
#include <time.h>
static constexpr uint32_t kMagic = 0x54415331; // TAS1
bool TaskStore::load() { const bool ok = StoreIO::load(storage_, bus_, "/PocketPDA/tasks/tasks.dat", kMagic, records_, kCapacity, count_); nextId_ = 1; for (size_t i=0;i<count_;++i) if(records_[i].id>=nextId_) nextId_=records_[i].id+1; return ok; }
bool TaskStore::save() { return StoreIO::save(storage_, bus_, "/PocketPDA/tasks/tasks.dat", kMagic, records_, count_); }
TaskRecord *TaskStore::find(uint32_t id) { for(size_t i=0;i<count_;++i) if(records_[i].id==id) return &records_[i]; return nullptr; }
bool TaskStore::upsert(TaskRecord &record) { TaskRecord *old=find(record.id); if(old)*old=record; else { if(count_>=kCapacity)return false; record.id=nextId_++; records_[count_++]=record; } return save(); }
bool TaskStore::remove(uint32_t id) { for(size_t i=0;i<count_;++i) if(records_[i].id==id){ memmove(&records_[i],&records_[i+1],(count_-i-1)*sizeof(TaskRecord));--count_;return save(); } return false; }
bool TaskStore::setCompleted(uint32_t id,bool completed){
    TaskRecord *r=find(id); if(!r)return false;
    if(completed && r->recurring && r->dueYear){
        struct tm due{}; due.tm_year=r->dueYear-1900; due.tm_mon=r->dueMonth-1; due.tm_mday=r->dueDay+7; due.tm_isdst=-1; mktime(&due);
        r->dueYear=due.tm_year+1900; r->dueMonth=due.tm_mon+1; r->dueDay=due.tm_mday; r->completed=0;
    } else r->completed=completed;
    return save();
}
