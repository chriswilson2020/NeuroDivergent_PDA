#pragma once
#include <cstddef>
#include <cstring>
#include <vector>
class Preferences {
public:
    inline static bool failWrite=false,failOpen=false;
    inline static std::vector<unsigned char> saved;
    bool begin(const char*,bool){return !failOpen;}
    void end(){}
    size_t getBytesLength(const char*){return saved.size();}
    size_t getBytes(const char*,void *data,size_t size){if(size!=saved.size())return 0;memcpy(data,saved.data(),size);return size;}
    size_t putBytes(const char*,const void *data,size_t size){if(failWrite)return 0;const auto *p=static_cast<const unsigned char*>(data);saved.assign(p,p+size);return size;}
};
