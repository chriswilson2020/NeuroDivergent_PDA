#include "core/RadioSettingsInput.h"
#include <cassert>
#include <cstdio>
int main(){
    MessagingSettings settings;
    assert(!RadioSettingsInput::validate("868300","7","14","30",settings));
    assert(settings.spreadingFactor==7);
    const char*bad[]={""," ","-1","+8","8x","7.5","999999999999999999999999999999"};
    for(auto*value:bad){
        assert(RadioSettingsInput::validate("868300",value,"14","30",settings));
        assert(settings.spreadingFactor==7);
    }
    assert(RadioSettingsInput::validate("","8","14","30",settings));
    assert(RadioSettingsInput::validate("868300","8","","30",settings));
    assert(RadioSettingsInput::validate("868300","8","14","",settings));
    assert(settings.spreadingFactor==7); // Later error must not partially apply SF8.
    assert(RadioSettingsInput::validate("862999","8","14","30",settings));
    assert(RadioSettingsInput::validate("870001","8","14","30",settings));
    assert(RadioSettingsInput::validate("868300","6","14","30",settings));
    assert(RadioSettingsInput::validate("868300","13","14","30",settings));
    assert(RadioSettingsInput::validate("868300","8","15","30",settings));
    assert(RadioSettingsInput::validate("868300","8","14","301",settings));
    assert(!RadioSettingsInput::validate("863000","7","2","10",settings));
    assert(!RadioSettingsInput::validate("870000","12","14","300",settings));
    puts("Radio settings validation tests passed");
}
