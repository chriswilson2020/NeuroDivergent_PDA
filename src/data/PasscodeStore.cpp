#include "PasscodeStore.h"
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <cstring>

bool PasscodeStore::validPin(const char *pin) {
    if(!pin||strlen(pin)!=6)return false;
    for(unsigned i=0;i<6;++i) {
        const char c=pin[i];
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')))return false;
    }
    return true;
}
bool PasscodeStore::validTimeout(uint16_t seconds) {
    return seconds==30||seconds==60||seconds==120||seconds==300||seconds==600||seconds==900;
}
bool PasscodeStore::digest(const char *pin,const uint8_t *salt,uint8_t *out) {
    uint8_t input[22];memcpy(input,salt,16);memcpy(input+16,pin,6);
    const bool ok=mbedtls_sha256(input,sizeof(input),out,0)==0;
    memset(input,0,sizeof(input));return ok;
}
bool PasscodeStore::load() {
    Preferences p;record_={};fault_=false;
    if(!p.begin("pda-lock",false)){fault_=true;return false;}
    const auto size=p.getBytesLength("config");
    if(size) {
        Record saved{};
        if(size!=sizeof(saved)||p.getBytes("config",&saved,sizeof(saved))!=sizeof(saved)||
           saved.version!=1||saved.enabled>1||!validTimeout(saved.seconds))fault_=true;
        else record_=saved;
    }
    p.end();return !fault_;
}
bool PasscodeStore::verify(const char *pin) const {
    if(fault_||!record_.enabled||!validPin(pin))return false;
    uint8_t actual[32]{};
    if(!digest(pin,record_.salt,actual))return false;
    uint8_t difference=0;
    for(unsigned i=0;i<sizeof(actual);++i)difference|=actual[i]^record_.digest[i];
    memset(actual,0,sizeof(actual));return difference==0;
}
bool PasscodeStore::change(const char *current,const char *next,uint16_t seconds) {
    if(fault_||(enabled()&&!verify(current))||!next||!validTimeout(seconds))return false;
    if(*next&&!validPin(next))return false;
    Record saved{};saved.seconds=seconds;saved.enabled=*next?1:0;
    if(saved.enabled) {
        esp_fill_random(saved.salt,sizeof(saved.salt));
        if(!digest(next,saved.salt,saved.digest))return false;
    }
    Preferences p;if(!p.begin("pda-lock",false))return false;
    const bool ok=p.putBytes("config",&saved,sizeof(saved))==sizeof(saved);p.end();
    if(ok)record_=saved;return ok;
}
