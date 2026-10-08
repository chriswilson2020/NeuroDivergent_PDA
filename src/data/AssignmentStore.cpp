#include "AssignmentStore.h"
#include "StoreIO.h"
#include <cstring>

namespace { constexpr uint32_t kMagic = 0x41534731; }

bool AssignmentStore::load(){const bool ok=StoreIO::load(storage_,bus_,"/PocketPDA/assignments/assignments.dat",kMagic,records_,kCapacity,count_);nextId_=1;for(size_t i=0;i<count_;++i)if(records_[i].id>=nextId_)nextId_=records_[i].id+1;return ok;}
bool AssignmentStore::save(){return StoreIO::save(storage_,bus_,"/PocketPDA/assignments/assignments.dat",kMagic,records_,count_);}
AssignmentRecord *AssignmentStore::find(uint32_t id){for(size_t i=0;i<count_;++i)if(records_[i].id==id)return &records_[i];return nullptr;}
bool AssignmentStore::upsert(AssignmentRecord &record){AssignmentRecord *old=find(record.id);if(old)*old=record;else{if(count_>=kCapacity)return false;record.id=nextId_++;records_[count_++]=record;}return save();}
bool AssignmentStore::remove(uint32_t id){for(size_t i=0;i<count_;++i)if(records_[i].id==id){memmove(&records_[i],&records_[i+1],(count_-i-1)*sizeof(AssignmentRecord));--count_;return save();}return false;}
bool AssignmentStore::completeNextStep(uint32_t id){AssignmentRecord *record=find(id);if(!record||record->completed)return false;if(record->currentStep+1>=record->stepCount)record->completed=1;else ++record->currentStep;return save();}
