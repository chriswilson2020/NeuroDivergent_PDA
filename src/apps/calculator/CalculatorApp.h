#pragma once
#include "core/App.h"

class CalculatorApp : public App {
public:
    const char *id() const override { return "calculator"; }
    const char *title() const override { return "Calculator"; }
    void create(lv_obj_t *parent) override;
    void resume() override {}
    void suspend() override;
    void destroy() override;
    lv_obj_t *root() const override { return root_; }
private:
    static void buttonClicked(lv_event_t *event);
    static void expressionReady(lv_event_t *event);
    void append(const char *text);
    void calculate();
    void capture();
    lv_obj_t *root_ = nullptr;
    lv_obj_t *expression_ = nullptr;
    lv_obj_t *result_ = nullptr;
    char expressionText_[65]{};
    char resultText_[48] = "0";
};
