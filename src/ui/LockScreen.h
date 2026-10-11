#pragma once
#include "core/LockPolicy.h"
#include <lvgl.h>
class PasscodeStore;

class LockScreen {
public:
    using Changed=void(*)(void*,bool);
    void begin(PasscodeStore &store,Changed changed,void *context,uint64_t now);
    void update(uint64_t now);
    void activity(uint64_t now) { policy_.activity(now); }
    void preferencesChanged(uint64_t now);
    void lock();
    void ensureFocus();
    void setAlertPending(bool pending);
    bool locked() const { return policy_.locked(); }
    uint64_t deadlineMs() const { return policy_.deadline(); }
private:
    static void submit(lv_event_t *);
    static void digit(lv_event_t *);
    void tryUnlock();
    void status(const char *text);
    PasscodeStore *store_=nullptr;
    Changed changed_=nullptr;void *context_=nullptr;
    LockPolicy policy_;
    uint64_t now_=0;
    uint32_t shownWait_=0;
    lv_obj_t *root_=nullptr,*pin_=nullptr,*status_=nullptr,*alert_=nullptr;
    lv_group_t *group_=nullptr;
};
