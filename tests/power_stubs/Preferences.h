#pragma once
#include <map>
#include <string>
#include <stdint.h>
#include <vector>
#include <cstring>
class Preferences {
public:
    inline static std::map<std::string,int64_t> values;
    inline static bool fail=false;
    inline static std::map<std::string,std::vector<uint8_t>> bytes;
    bool begin(const char *name,bool){prefix=std::string(name)+"/";return !fail;}
    int64_t getLong64(const char *key,int64_t fallback){auto i=values.find(prefix+key);return i==values.end()?fallback:i->second;}
    size_t putLong64(const char *key,int64_t value){if(fail)return 0;values[prefix+key]=value;return sizeof(value);}
    void end(){}
    uint8_t getUChar(const char *key,uint8_t fallback){return getLong64(key,fallback);}
    size_t putUChar(const char *key,uint8_t value){return putLong64(key,value)?1:0;}
    size_t getBytesLength(const char *key){return bytes[prefix+key].size();}
    size_t getBytes(const char *key,void *out,size_t n){auto &b=bytes[prefix+key];if(b.size()!=n)return 0;memcpy(out,b.data(),n);return n;}
    size_t putBytes(const char *key,const void *data,size_t n){if(fail)return 0;const auto *p=static_cast<const uint8_t*>(data);bytes[prefix+key]=std::vector<uint8_t>(p,p+n);return n;}
private:std::string prefix;
};
