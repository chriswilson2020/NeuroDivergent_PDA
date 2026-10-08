#pragma once
#include "App.h"

class AppManager {
public:
    void begin(lv_obj_t *host);
    void registerApp(App *app);
    bool launch(const char *id);
    App *active() const { return active_; }
private:
    static constexpr size_t kMaxApps = 12;
    lv_obj_t *host_ = nullptr;
    App *apps_[kMaxApps]{};
    size_t count_ = 0;
    App *active_ = nullptr;
};
