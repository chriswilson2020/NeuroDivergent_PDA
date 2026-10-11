#pragma once
#include "core/NotificationManager.h"
class Shell {
public:
    NotificationManager n;
    NotificationManager &notifications(){return n;}
};
