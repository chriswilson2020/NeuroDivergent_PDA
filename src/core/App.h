#pragma once
#include <lvgl.h>

class App {
public:
    virtual ~App() = default;
    virtual const char *id() const = 0;
    virtual const char *title() const = 0;
    virtual void create(lv_obj_t *parent) = 0;
    virtual void resume() = 0;
    virtual void suspend() = 0;
    virtual void destroy() = 0;
    virtual lv_obj_t *root() const = 0;
};
