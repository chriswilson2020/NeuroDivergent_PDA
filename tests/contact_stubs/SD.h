#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <algorithm>
#define FILE_READ "r"
#define FILE_WRITE "w"
using Blob=std::vector<unsigned char>;
class File {
public:
    File()=default;explicit File(std::shared_ptr<Blob> b):bytes(b){}
    explicit operator bool()const{return bool(bytes);}
    size_t read(uint8_t *out,size_t size){if(!bytes)return 0;size=std::min(size,bytes->size()-cursor);memcpy(out,bytes->data()+cursor,size);cursor+=size;return size;}
    size_t write(const uint8_t *in,size_t size){if(!bytes||failWrite)return 0;if(cursor+size>bytes->size())bytes->resize(cursor+size);memcpy(bytes->data()+cursor,in,size);cursor+=size;return size;}
    void flush(){}void close(){}
    inline static bool failWrite=false;
private:std::shared_ptr<Blob> bytes;size_t cursor=0;
};
class FakeSD {
public:
    std::map<std::string,std::shared_ptr<Blob>> files;
    std::string failRenameFrom;
    bool exists(const char *p){return files.count(p);}
    bool remove(const char *p){return files.erase(p);}
    bool rename(const char *a,const char *b){if(a==failRenameFrom){failRenameFrom.clear();return false;}auto i=files.find(a);if(i==files.end()||files.count(b))return false;files[b]=i->second;files.erase(i);return true;}
    File open(const char *p,const char *mode){if(*mode=='w')files[p]=std::make_shared<Blob>();auto i=files.find(p);return i==files.end()?File{}:File(i->second);}
};
inline FakeSD SD;
