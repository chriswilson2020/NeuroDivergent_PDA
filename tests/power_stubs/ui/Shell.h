#pragma once
#include <string>
#include <vector>
#include <stdint.h>
struct Notifications {
    std::vector<std::string> titles;
    const char *actionTitle()const{return "Snoozed";}
    const char *actionDetail()const{return "Detail";}
    void show(const char *title,const char *,const char * =nullptr,void (*)(void *)=nullptr,void * =nullptr,uint8_t=47){titles.emplace_back(title);}
};
class Shell { public: Notifications n;Notifications &notifications(){return n;} };
