#pragma once
#include <stddef.h>
#include <stdint.h>
class StorageService;class SPIBusManager;
struct PackingTemplate{uint32_t id=0;uint8_t itemCount=0;char keyword[24]{};char title[32]{};char items[6][32]{};};
struct PackingState{uint64_t occurrenceKey=0;uint8_t checkedMask=0;};
class PackingStore{
public:
 static constexpr size_t kCapacity=12;PackingStore(StorageService&s,SPIBusManager&b):storage_(s),bus_(b){}
 bool load();bool save();size_t count()const{return count_;}const PackingTemplate&at(size_t i)const{return records_[i];}PackingTemplate*find(uint32_t id);PackingTemplate*match(const char*eventTitle);bool upsert(PackingTemplate&r);bool remove(uint32_t id);bool loadState(PackingState&s);bool saveState(const PackingState&s);
private:bool defaults();StorageService&storage_;SPIBusManager&bus_;PackingTemplate records_[kCapacity]{};size_t count_=0;uint32_t nextId_=1;
};
