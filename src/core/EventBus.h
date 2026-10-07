#pragma once
#include <stddef.h>
#include <stdint.h>

enum class SystemEvent : uint8_t { TimeChanged, BatteryChanged, NotificationChanged, PowerStateChanged };
using EventHandler = void (*)(SystemEvent event, void *context);

class EventBus {
public:
    static EventBus &instance();
    bool subscribe(EventHandler handler, void *context);
    void publish(SystemEvent event);
private:
    struct Subscriber { EventHandler handler; void *context; };
    Subscriber subscribers_[12]{};
    size_t count_ = 0;
};
