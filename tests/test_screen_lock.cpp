#include "core/LockPolicy.h"
#include "core/NotificationManager.h"
#include "data/PasscodeStore.h"
#include "ui/LockScreen.h"
#include "ui/FormFocus.h"
#include "hardware/HapticService.h"
#include <Preferences.h>
#include <esp_timer.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
static unsigned hapticCalls=0;
void HapticService::play(uint8_t) { ++hapticCalls; }
static bool isLocked=false;
static NotificationManager notifications;
static void changed(void *,bool locked){isLocked=locked;notifications.setLocked(locked);}
static lv_obj_t *findButton(lv_obj_t *root,const char *name) {
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) {
        auto *child=lv_obj_get_child(root,i);
        if(lv_obj_check_type(child,&lv_button_class)) {
            auto *label=lv_obj_get_child(child,0);
            if(label&&lv_obj_check_type(label,&lv_label_class)&&!strcmp(lv_label_get_text(label),name))return child;
        }
        if(auto *found=findButton(child,name))return found;
    }
    return nullptr;
}
int main() {
    LockPolicy policy;policy.configure(true,30,1000);
    assert(!policy.expired(30999)&&policy.expired(31000));
    policy.activity(10000);assert(policy.deadline()==40000);
    policy.lock();assert(policy.locked()&&!policy.deadline());
    policy.failed(10000);policy.failed(10000);assert(policy.canTry(10000));
    policy.failed(10000);assert(!policy.canTry(14999)&&policy.canTry(15000));
    policy.unlock(20000);assert(!policy.locked()&&policy.deadline()==50000);
    // No wall-clock adjustment or notification activity can extend this.
    assert(policy.expired(1000000));
    policy.configure(false,30,1000000);assert(!policy.expired(2000000));

    PasscodeStore store;assert(store.load()&&!store.enabled());
    assert(store.saveOwner("","Temporary owner","",""));
    assert(store.saveOwner("","","","")&&!*store.owner().name);
    Preferences::owner={1,2};PasscodeStore badOwner;assert(badOwner.load()&&!badOwner.enabled()&&!*badOwner.owner().name);
    Preferences::owner.clear();
    assert(!store.change("","12345",30));assert(!store.change("","12345!",30));
    assert(!store.change("","abcdefg",30));assert(!store.change("","abc de",30));
    assert(!store.change("","123456",45));
    assert(store.change("","123456",30)&&store.verify("123456")&&!store.verify("654321"));
    const auto saved=Preferences::saved;
    const unsigned char plain[]={'1','2','3','4','5','6'};
    assert(std::search(saved.begin(),saved.end(),plain,plain+6)==saved.end());
    assert(!store.change("000000","654321",60));
    Preferences::failWrite=true;assert(!store.change("123456","654321",60));
    assert(store.verify("123456")&&store.timeoutSeconds()==30);Preferences::failWrite=false;
    PasscodeStore reloaded;assert(reloaded.load()&&reloaded.verify("123456"));
    assert(store.change("123456","654321",60));assert(!store.verify("123456"));
    assert(store.change("654321","aB3def",30)&&store.verify("aB3def"));
    assert(!store.verify("ab3def")); // Case-sensitive, with unchanged legacy numeric hashes.
    PasscodeStore alphaReload;assert(alphaReload.load()&&alphaReload.verify("aB3def"));
    assert(store.change("aB3def","",30)&&!store.enabled());
    Preferences::saved={1,2};PasscodeStore corrupt;assert(!corrupt.load()&&corrupt.enabled()&&corrupt.fault());
    assert(!corrupt.change("","123456",30));
    Preferences::saved.clear();Preferences::failOpen=true;
    PasscodeStore unavailable;assert(!unavailable.load()&&unavailable.enabled());Preferences::failOpen=false;
    assert(store.change("","123456",30));
    assert(!store.saveOwner("000000","Gabriel","+31 123456789","parent@example.com"));
    const auto credentials=Preferences::saved;
    assert(store.saveOwner("123456","Gabriel","+31 123456789","parent@example.com"));
    assert(Preferences::saved==credentials&&store.verify("123456"));
    PasscodeStore ownerReload;assert(ownerReload.load()&&!strcmp(ownerReload.owner().name,"Gabriel"));
    Preferences::failWrite=true;assert(!store.saveOwner("123456","Wrong","",""));Preferences::failWrite=false;
    assert(!strcmp(store.owner().name,"Gabriel"));
    assert(!store.saveOwner("123456","Bad\nName","",""));
    assert(!store.saveOwner("123456","Gabriel","",std::string(64,'x').c_str()));

    lv_init();lv_display_create(480,222);auto *group=lv_group_create();lv_group_set_default(group);
    auto *keyboard=lv_indev_create();lv_indev_set_type(keyboard,LV_INDEV_TYPE_KEYPAD);
    auto *encoder=lv_indev_create();lv_indev_set_type(encoder,LV_INDEV_TYPE_ENCODER);
    auto *app=lv_obj_create(lv_screen_active());auto *privateField=lv_textarea_create(app);
    FormFocus::enter(privateField);FormFocus::bindInputs(group);
    HapticService haptic;notifications.begin(lv_screen_active(),haptic);
    LockScreen lock;lock.begin(store,changed,nullptr,0);
    assert(lock.locked()&&isLocked&&!lock.enteringPasscode());
    auto *ownerGroup=lv_indev_get_group(keyboard);assert(lv_group_get_obj_count(ownerGroup)==1);
    notifications.show("Message","Private contents stay hidden");
    assert(lv_indev_get_group(keyboard)==ownerGroup&&notifications.active());
    notifications.dismiss();assert(notifications.active());hapticCalls=0;
    lock.activity(0);assert(lock.enteringPasscode());auto *lockGroup=lv_indev_get_group(keyboard);
    assert(lockGroup!=group&&lv_indev_get_group(encoder)==lockGroup);
    auto *pin=lv_group_get_focused(lockGroup);
    assert(lv_obj_check_type(pin,&lv_textarea_class));
    assert(lv_textarea_get_password_mode(pin));
    notifications.show("Message","Private message");
    assert(hapticCalls==1); // Lock does not suppress an incoming-message alert.
    assert(lv_indev_get_group(keyboard)==lockGroup);notifications.dismiss();assert(notifications.active());
    for(int i=0;i<25;++i){lv_group_focus_next(lockGroup);assert(FormFocus::belongs(lv_group_get_focused(lockGroup),lv_layer_top()));}
    auto *unlock=findButton(lv_layer_top(),"UNLOCK");assert(unlock);
    for(int i=0;i<3;++i){lv_textarea_set_text(pin,"000000");lv_obj_send_event(unlock,LV_EVENT_CLICKED,nullptr);assert(lock.locked());}
    lv_textarea_set_text(pin,"123456");lv_obj_send_event(unlock,LV_EVENT_CLICKED,nullptr);assert(lock.locked());
    fakeLockUs=5000000;lock.update(5000);
    lv_textarea_set_text(pin,"123456");lv_obj_send_event(unlock,LV_EVENT_CLICKED,nullptr);
    assert(!lock.locked()&&!isLocked);assert(!*lv_textarea_get_text(pin));
    // The queued private reminder becomes available only after unlocking.
    assert(notifications.active()&&lv_indev_get_group(keyboard)!=lockGroup);
    notifications.dismiss();FormFocus::bindInputs(group);
    hapticCalls=0;
    lock.update(34999);assert(!lock.locked());lock.update(35000);assert(lock.locked());
    assert(!lock.enteringPasscode());lock.activity(35000);
    notifications.show("Timer","Time is up",nullptr,nullptr,nullptr,47,true);
    notifications.show("Daily alarm","Wake up",nullptr,nullptr,nullptr,47,true);
    notifications.show("Calendar","Lesson",nullptr,nullptr,nullptr,47,true);
    notifications.show("Task","Due",nullptr,nullptr,nullptr,47,true);
    assert(hapticCalls==4); // All simultaneous/queued alerts still vibrate now.
    assert(lv_display_get_inactive_time(nullptr)==0); // Alarm display wake retained.
    assert(lv_indev_get_group(keyboard)==lockGroup); // No alarm bypasses the PIN.
    lock.setAlertPending(true);
    fakeLockUs=35000000;
    // Wheel-accessible keypad can unlock without a keyboard/modifier.
    for(const char *digit:{"1","2","3","4","5","6"}) {
        auto *button=findButton(lv_layer_top(),digit);assert(button);lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);
    }
    lv_obj_send_event(findButton(lv_layer_top(),"OK"),LV_EVENT_CLICKED,nullptr);assert(!lock.locked());
    assert(store.change("123456","gabbyx",30));
    lock.preferencesChanged(35000);lock.lock();
    assert(!lock.enteringPasscode());lock.activity(35000);
    lv_textarea_set_text(pin,"secret");lock.back();assert(lock.locked()&&!lock.enteringPasscode()&&!*lv_textarea_get_text(pin));
    lock.activity(35000);lv_textarea_set_text(pin,"secret");lock.update(50000);
    assert(lock.locked()&&!lock.enteringPasscode()&&!*lv_textarea_get_text(pin));
    lock.activity(50000);fakeLockUs=50000000;
    lv_textarea_set_text(pin,"");lv_group_focus_obj(pin);
    for(const char c: "gabbyx")if(c){uint32_t key=c;lv_obj_send_event(pin,LV_EVENT_KEY,&key);}
    assert(!strcmp(lv_textarea_get_text(pin),"gabbyx"));
    lv_obj_send_event(unlock,LV_EVENT_CLICKED,nullptr);assert(!lock.locked());
    puts("Passcode persistence, alphanumeric validation/unlock, failure rollback, startup, inactivity, retry delay, modal privacy and keypad tests passed");
}
