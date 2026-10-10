#include "OccurrenceLedger.h"
#include <Preferences.h>
#include <cstdio>
bool OccurrenceLedger::claim(char kind,uint32_t id,time_t occurrence) {
    if(!healthy_)return false;
    char key[12];snprintf(key,sizeof(key),"%c%08lx",kind,static_cast<unsigned long>(id));
    healthy_=false;
    Preferences p;if(!p.begin(name_,false))return false;
    const int64_t previous=p.getLong64(key,0);
    const bool fresh=occurrence>previous;
    const bool ok=!fresh||p.putLong64(key,occurrence)==sizeof(int64_t);p.end();healthy_=ok;return fresh&&ok;
}
