#include "TaskStore.h"
#include "StoreIO.h"
#include "hardware/SPIBusManager.h"
#include "hardware/StorageService.h"
#include <Arduino.h>
#include <SD.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>
static constexpr uint32_t kMagic = 0x54415331; // TAS1
bool TaskStore::load() { lastImportCount_=0; if(importCsv()) return true; const bool ok = StoreIO::load(storage_, bus_, "/PocketPDA/tasks/tasks.dat", kMagic, records_, kCapacity, count_); nextId_ = 1; for (size_t i=0;i<count_;++i) if(records_[i].id>=nextId_) nextId_=records_[i].id+1; return ok; }
bool TaskStore::save() { return StoreIO::save(storage_, bus_, "/PocketPDA/tasks/tasks.dat", kMagic, records_, count_); }
TaskRecord *TaskStore::find(uint32_t id) { for(size_t i=0;i<count_;++i) if(records_[i].id==id) return &records_[i]; return nullptr; }
bool TaskStore::upsert(TaskRecord &record) { TaskRecord *old=find(record.id); if(old)*old=record; else { if(count_>=kCapacity)return false; record.id=nextId_++; records_[count_++]=record; } return save(); }
bool TaskStore::remove(uint32_t id) { for(size_t i=0;i<count_;++i) if(records_[i].id==id){ memmove(&records_[i],&records_[i+1],(count_-i-1)*sizeof(TaskRecord));--count_;return save(); } return false; }
bool TaskStore::setCompleted(uint32_t id,bool completed){
    TaskRecord *r=find(id); if(!r)return false;
    if(completed && r->recurring && r->dueYear){
        struct tm due{}; due.tm_year=r->dueYear-1900; due.tm_mon=r->dueMonth-1; due.tm_mday=r->dueDay; due.tm_isdst=-1;
        if(r->recurring==2)due.tm_mday+=1;
        else if(r->recurring==4)due.tm_mon+=1;
        else due.tm_mday+=r->recurring==3?1:7;
        mktime(&due);
        if(r->recurring==3){while(due.tm_wday==0||due.tm_wday==6){due.tm_mday+=1;mktime(&due);}}
        r->dueYear=due.tm_year+1900; r->dueMonth=due.tm_mon+1; r->dueDay=due.tm_mday; r->completed=0;
    } else r->completed=completed;
    return save();
}

bool TaskStore::importCsv(){
    if(!storage_.mounted())return false;
    memset(records_,0,sizeof(records_)); size_t importedCount=0; bool valid=true;
    {
        SPIBusManager::Guard guard(bus_); if(!guard)return false;
        File input=SD.open("/PocketPDA/tasks/import.csv",FILE_READ); if(!input)return false;
        char line[220]; bool first=true;
        while(valid&&input.available()){
            const size_t length=input.readBytesUntil('\n',line,sizeof(line)-1); line[length]=0;
            if(length&&line[length-1]=='\r')line[length-1]=0;
            if(first){first=false;if(!strncmp(line,"title,",6))continue;}
            if(!line[0]||line[0]=='#')continue;
            if(importedCount>=kCapacity){valid=false;break;}
            char *fields[6]{}; size_t fieldCount=0; char *cursor=line;
            while(fieldCount<6){fields[fieldCount++]=cursor;char *comma=strchr(cursor,',');if(!comma)break;*comma=0;cursor=comma+1;}
            if(fieldCount<6||!fields[0][0]){valid=false;break;}
            TaskRecord &task=records_[importedCount]; task.id=importedCount+1;
            strlcpy(task.title,fields[0],sizeof(task.title)); strlcpy(task.notes,fields[1],sizeof(task.notes));
            if(fields[2][0]){
                int year=0,month=0,day=0;
                if(sscanf(fields[2],"%d-%d-%d",&year,&month,&day)!=3||year<2024||year>2099||month<1||month>12||day<1||day>31){valid=false;break;}
                task.dueYear=year;task.dueMonth=month;task.dueDay=day;
            }
            char *end=nullptr; const long priority=strtol(fields[3],&end,10);
            if(!end||*end||priority<0||priority>2){valid=false;break;} task.priority=priority;
            task.reminder=(!strcmp(fields[4],"1")||!strcmp(fields[4],"yes")||!strcmp(fields[4],"true"));
            if(!strcmp(fields[5],"weekly"))task.recurring=1;
            else if(!strcmp(fields[5],"daily"))task.recurring=2;
            else if(!strcmp(fields[5],"weekdays"))task.recurring=3;
            else if(!strcmp(fields[5],"monthly"))task.recurring=4;
            else if(fields[5][0]&&strcmp(fields[5],"once")){valid=false;break;}
            ++importedCount;
        }
        input.close();
    }
    if(!valid||!importedCount)return false;
    count_=importedCount;nextId_=importedCount+1;
    if(!save()){count_=0;return false;}
    {
        SPIBusManager::Guard guard(bus_);if(guard){SD.remove("/PocketPDA/tasks/last-import.csv");SD.rename("/PocketPDA/tasks/import.csv","/PocketPDA/tasks/last-import.csv");}
    }
    lastImportCount_=importedCount;return true;
}
