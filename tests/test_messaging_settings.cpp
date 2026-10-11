#include "data/MessagingSettingsStore.h"
#include <Preferences.h>
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
    MessagingSettingsStore settings;
    settings.load();
    uint8_t first[32]={1},replacement[32]={2};
    assert(settings.upsertContact(7,"Gabriel",first));
    Preferences::failWrite=true;
    assert(!settings.upsertContact(7,"Changed",replacement));
    assert(!strcmp(settings.findContact(7)->name,"Gabriel"));
    assert(settings.findContact(7)->key[0]==1);
    assert(!settings.removeContact(7));
    assert(settings.findContact(7));
    assert(!settings.upsertContact(8,"New",replacement));
    assert(!settings.findContact(8));
    Preferences::failWrite=false;
    assert(settings.upsertContact(7,"Gabriel",replacement));
    assert(settings.findContact(7)->key[0]==2);
    assert(settings.removeContact(7));
    assert(!settings.findContact(7));
    MessagingSettingsStore reloaded;reloaded.load();
    assert(!reloaded.findContact(7));
    puts("Messaging contact persistence tests passed");
}
