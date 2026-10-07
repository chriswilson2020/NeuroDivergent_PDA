#include "EventBus.h"

EventBus &EventBus::instance() { static EventBus bus; return bus; }
bool EventBus::subscribe(EventHandler handler, void *context) {
    if (!handler || count_ >= 12) return false;
    subscribers_[count_++] = {handler, context};
    return true;
}
void EventBus::publish(SystemEvent event) {
    for (size_t i = 0; i < count_; ++i) subscribers_[i].handler(event, subscribers_[i].context);
}
