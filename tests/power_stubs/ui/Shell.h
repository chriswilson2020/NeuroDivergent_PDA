#pragma once
#include <string>
#include <vector>
#include <stdint.h>
struct Notifications {
    bool accepts=true;
    bool canAccept()const{return accepts;}
    void (*action)(void*)=nullptr;void *context=nullptr;
    std::vector<std::string> titles;
    const char *actionTitle()const{return "Snoozed";}
    const char *actionDetail()const{return "Detail";}
    void show(const char *title,const char *,const char * =nullptr,void (*callback)(void *)=nullptr,void *ctx =nullptr,uint8_t=47){titles.emplace_back(title);action=callback;context=ctx;}
};
class Shell { public: Notifications n;Notifications &notifications(){return n;} };
