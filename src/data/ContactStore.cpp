#include "ContactStore.h"
#include "StoreIO.h"
#include <esp_heap_caps.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>

namespace {
constexpr const char *path="/PocketPDA/contacts/contacts.dat";
constexpr const char *candidate="/PocketPDA/contacts/contacts.new";
constexpr const char *previous="/PocketPDA/contacts/contacts.bak";
constexpr uint32_t magic=0x434E5431;
int compare(const char *a,const char *b) {
    while(*a&&*b&&tolower(static_cast<unsigned char>(*a))==tolower(static_cast<unsigned char>(*b))){++a;++b;}
    return tolower(static_cast<unsigned char>(*a))-tolower(static_cast<unsigned char>(*b));
}
bool contains(const char *text,const char *query) {
    if(!*query)return true;
    for(;*text;++text){const char *a=text,*b=query;while(*a&&*b&&tolower(static_cast<unsigned char>(*a))==tolower(static_cast<unsigned char>(*b))){++a;++b;}if(!*b)return true;}
    return false;
}
template<size_t N> bool terminated(const char (&text)[N]) { return memchr(text,0,N)!=nullptr; }
bool meaningful(const char *text){for(;*text;++text)if(!isspace(static_cast<unsigned char>(*text)))return true;return false;}
}
ContactStore::~ContactStore(){free(records_);}
bool ContactStore::valid(const ContactRecord &r) {
    return r.favourite<=1&&terminated(r.name)&&terminated(r.company)&&terminated(r.job)&&terminated(r.mobile)&&terminated(r.phone)&&
        terminated(r.email)&&terminated(r.address)&&terminated(r.website)&&terminated(r.notes)&&(meaningful(r.name)||meaningful(r.company));
}
bool ContactStore::matches(const ContactRecord &r,const char *query) {
    return !query||contains(r.name,query)||contains(r.company,query)||contains(r.job,query)||contains(r.mobile,query)||contains(r.phone,query)||contains(r.email,query)||contains(r.address,query)||contains(r.website,query)||contains(r.notes,query);
}
void ContactStore::sort(){std::sort(records_,records_+count_,[](const ContactRecord &a,const ContactRecord &b){if(a.favourite!=b.favourite)return a.favourite>b.favourite;return compare(*a.name?a.name:a.company,*b.name?b.name:b.company)<0;});}
const ContactRecord *ContactStore::find(uint32_t id) const {for(size_t i=0;i<count_;++i)if(records_[i].id==id)return &records_[i];return nullptr;}
bool ContactStore::load() {
    ready_=false;count_=0;error_="microSD unavailable";
    if(!storage_.mounted())return false;
    if(!records_)records_=static_cast<ContactRecord*>(heap_caps_calloc(kCapacity,sizeof(ContactRecord),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!records_){error_="Contact memory unavailable";return false;}
    {
        SPIBusManager::Guard guard(bus_);if(!guard){error_="microSD busy";return false;}
        if(!SD.exists(path)&&SD.exists(previous)&&!SD.rename(previous,path)){error_="Contact recovery failed";return false;}
    }
    if(!StoreIO::load(storage_,bus_,path,magic,records_,kCapacity,count_)){count_=0;error_="Contacts file invalid; restore a backup";return false;}
    for(size_t i=0;i<count_;++i) {
        bool ok=records_[i].id&&valid(records_[i]);
        for(size_t j=0;j<i;++j)if(records_[i].id==records_[j].id)ok=false;
        if(!ok){count_=0;error_="Contacts file invalid; restore a backup";return false;}
    }
    sort();ready_=true;error_="";return true;
}
bool ContactStore::save() {
    if(!ready_){error_="Contacts unavailable; check microSD";return false;}
    // Keep the previous file intact until a complete checksummed replacement
    // exists. load() recovers .bak if power fails between the final renames.
    error_="Could not save contacts; changes not saved";
    if(!StoreIO::save(storage_,bus_,candidate,magic,records_,count_))return false;
    SPIBusManager::Guard guard(bus_);if(!guard)return false;
    if(SD.exists(previous)&&!SD.remove(previous))return false;
    const bool had=SD.exists(path);
    if(had&&!SD.rename(path,previous))return false;
    if(!SD.rename(candidate,path)){if(had)SD.rename(previous,path);return false;}
    error_="";return true;
}
bool ContactStore::upsert(ContactRecord &r) {
    if(!ready_){error_="Contacts unavailable; check microSD";return false;}
    if(!valid(r)){error_="Enter a name or company";return false;}
    size_t index=count_;for(size_t i=0;i<count_;++i)if(records_[i].id==r.id){index=i;break;}
    const bool adding=!r.id;
    if(!adding&&index==count_){error_="Contact no longer exists";return false;}
    if(adding&&count_==kCapacity){error_="Contact storage is full (512)";return false;}
    ContactRecord next=r,old{};
    if(adding){uint32_t max=0;for(size_t i=0;i<count_;++i)max=std::max(max,records_[i].id);if(max==UINT32_MAX){error_="Contact IDs exhausted";return false;}next.id=max+1;++count_;}
    else old=records_[index];
    records_[index]=next;
    if(!save()){if(adding)--count_;else records_[index]=old;return false;}
    r=next;sort();return true;
}
bool ContactStore::remove(uint32_t id) {
    if(!ready_)return false;
    size_t index=0;while(index<count_&&records_[index].id!=id)++index;
    if(index==count_){error_="Contact no longer exists";return false;}
    ContactRecord old=records_[index];memmove(records_+index,records_+index+1,(count_-index-1)*sizeof(ContactRecord));--count_;
    if(!save()){memmove(records_+index+1,records_+index,(count_-index)*sizeof(ContactRecord));records_[index]=old;++count_;return false;}
    return true;
}
