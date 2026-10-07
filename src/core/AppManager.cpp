#include "AppManager.h"
#include <cstring>

void AppManager::begin(lv_obj_t *host) { host_ = host; }
void AppManager::registerApp(App *app) { if (app && count_ < kMaxApps) apps_[count_++] = app; }
bool AppManager::launch(const char *id) {
    App *next = nullptr;
    for (size_t i = 0; i < count_; ++i) if (std::strcmp(apps_[i]->id(), id) == 0) next = apps_[i];
    if (!next || !host_) return false;
    if (active_ == next && active_->root()) { active_->resume(); return true; }
    if (active_) { active_->suspend(); active_->destroy(); }
    active_ = next;
    active_->create(host_);
    active_->resume();
    return true;
}
