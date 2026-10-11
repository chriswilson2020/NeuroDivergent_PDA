#pragma once
#include <cstddef>
#include <cstring>
#include <vector>
class Preferences {
public:
    inline static bool failWrite=false,failOpen=false;
    inline static std::vector<unsigned char> saved;
    inline static std::vector<unsigned char> owner;
    static std::vector<unsigned char> &blob(const char *key){return !strcmp(key,"owner")?owner:saved;}
    bool begin(const char*,bool){return !failOpen;}
    void end(){}
    size_t getBytesLength(const char *key){return blob(key).size();}
    size_t getBytes(const char *key,void *data,size_t size){auto &b=blob(key);if(size!=b.size())return 0;memcpy(data,b.data(),size);return size;}
    size_t putBytes(const char *key,const void *data,size_t size){if(failWrite)return 0;const auto *p=static_cast<const unsigned char*>(data);blob(key).assign(p,p+size);return size;}
};
