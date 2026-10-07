#include "NoteStore.h"
#include "StoreIO.h"
#include <Arduino.h>
#include <SD.h>
#include <cstring>
static constexpr uint32_t kMagic = 0x4E4F5431; // NOT1
static void bodyPath(uint32_t id, char *out, size_t size) { snprintf(out, size, "/PocketPDA/notes/%08lx.txt", static_cast<unsigned long>(id)); }
bool NoteStore::load(){ const bool ok=StoreIO::load(storage_,bus_,"/PocketPDA/notes/index.dat",kMagic,records_,kCapacity,count_);nextId_=1;for(size_t i=0;i<count_;++i)if(records_[i].id>=nextId_)nextId_=records_[i].id+1;return ok; }
bool NoteStore::save(){return StoreIO::save(storage_,bus_,"/PocketPDA/notes/index.dat",kMagic,records_,count_);}
NoteRecord *NoteStore::find(uint32_t id){for(size_t i=0;i<count_;++i)if(records_[i].id==id)return &records_[i];return nullptr;}
bool NoteStore::upsert(NoteRecord &record,const char *body){if(!storage_.mounted())return false;NoteRecord *old=find(record.id);if(!old){if(count_>=kCapacity)return false;record.id=nextId_++;records_[count_++]=record;old=&records_[count_-1];}else *old=record;old->modified=millis()/1000;char path[64],tmp[72];bodyPath(old->id,path,sizeof(path));snprintf(tmp,sizeof(tmp),"%s.tmp",path);bool ok=false;{SPIBusManager::Guard guard(bus_);if(!guard)return false;SD.remove(tmp);File f=SD.open(tmp,FILE_WRITE);if(!f)return false;ok=f.print(body?body:"")>0 || !body || !*body;f.flush();f.close();if(ok){SD.remove(path);ok=SD.rename(tmp,path);}}if(!ok)return false;return save();}
bool NoteStore::remove(uint32_t id){for(size_t i=0;i<count_;++i)if(records_[i].id==id){char path[64];bodyPath(id,path,sizeof(path));{SPIBusManager::Guard guard(bus_);if(guard)SD.remove(path);}memmove(&records_[i],&records_[i+1],(count_-i-1)*sizeof(NoteRecord));--count_;return save();}return false;}
bool NoteStore::readBody(uint32_t id,char *buffer,size_t capacity){if(!buffer||capacity==0)return false;buffer[0]=0;if(!storage_.mounted())return false;char path[64];bodyPath(id,path,sizeof(path));SPIBusManager::Guard guard(bus_);if(!guard)return false;File f=SD.open(path,FILE_READ);if(!f)return true;size_t got=f.read(reinterpret_cast<uint8_t*>(buffer),capacity-1);buffer[got]=0;f.close();return true;}
