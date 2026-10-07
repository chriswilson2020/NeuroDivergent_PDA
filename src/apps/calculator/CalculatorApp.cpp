#include "CalculatorApp.h"
#include "ui/FormWidgets.h"
#include "ui/Theme.h"
#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
class ExpressionParser {
public:
    explicit ExpressionParser(const char *text) : cursor_(text) {}
    bool parse(double &result) { error_ = false; result = expression(); spaces(); return !error_ && *cursor_ == 0 && std::isfinite(result); }
private:
    void spaces() { while (*cursor_ == ' ') ++cursor_; }
    double number() { spaces(); char *end = nullptr; double value = strtod(cursor_, &end); if (end == cursor_) { error_ = true; return 0; } cursor_ = end; return value; }
    double factor() { spaces(); if (*cursor_ == '-') { ++cursor_; return -factor(); } if (*cursor_ == '(') { ++cursor_; double value = expression(); spaces(); if (*cursor_ != ')') error_ = true; else ++cursor_; return value; } return number(); }
    double term() { double value = factor(); for (;;) { spaces(); char op = *cursor_; if (op != '*' && op != '/') return value; ++cursor_; double rhs = factor(); if (op == '/' && rhs == 0) { error_ = true; return 0; } value = op == '*' ? value * rhs : value / rhs; } }
    double expression() { double value = term(); for (;;) { spaces(); char op = *cursor_; if (op != '+' && op != '-') return value; ++cursor_; double rhs = term(); value = op == '+' ? value + rhs : value - rhs; } }
    const char *cursor_; bool error_ = false;
};
}

void CalculatorApp::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent); lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100)); lv_obj_set_style_pad_all(root_, 5, 0); lv_obj_set_style_border_width(root_, 0, 0); lv_obj_set_style_radius(root_, 0, 0); lv_obj_set_scrollable(root_, false);
    expression_ = FormWidgets::text(root_, "Type an expression", 0, 0, 290, 38); lv_textarea_set_max_length(expression_, sizeof(expressionText_) - 1); lv_textarea_set_text(expression_, expressionText_); lv_obj_add_event_cb(expression_, expressionReady, LV_EVENT_READY, this);
    lv_obj_t *resultPanel = lv_obj_create(root_); Theme::stylePanel(resultPanel); lv_obj_set_pos(resultPanel, 297, 0); lv_obj_set_size(resultPanel, 172, 72); lv_obj_set_scrollable(resultPanel, false);
    lv_obj_t *caption = lv_label_create(resultPanel); lv_label_set_text(caption, "RESULT"); lv_obj_set_style_text_color(caption, Theme::color(0x1E6675), 0); lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 0, -3);
    result_ = lv_label_create(resultPanel); lv_label_set_text(result_, resultText_); lv_obj_set_width(result_, LV_PCT(100)); lv_obj_set_style_text_align(result_, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_set_style_text_font(result_, &lv_font_montserrat_24, 0); lv_obj_align(result_, LV_ALIGN_BOTTOM_RIGHT, 0, 3);
    static const char *keys[] = {"7","8","9","/", "4","5","6","*", "1","2","3","-", "C","0","=","+"};
    for (int i = 0; i < 16; ++i) { const int col = i % 4, row = i / 4; lv_obj_t *button = FormWidgets::button(root_, keys[i], col * 72, 43 + row * 35, 67, 31, strcmp(keys[i], "=") == 0); lv_obj_set_user_data(button, const_cast<char *>(keys[i])); lv_obj_add_event_cb(button, buttonClicked, LV_EVENT_CLICKED, this); }
    lv_obj_t *help = lv_label_create(root_); lv_label_set_text(help, "Keyboard: + - * / ( )\nPress Enter or ="); lv_obj_set_style_text_color(help, Theme::color(0x6C716F), 0); lv_obj_set_pos(help, 307, 87);
    lv_group_focus_obj(expression_);
}
void CalculatorApp::capture() { if (expression_) strlcpy(expressionText_, lv_textarea_get_text(expression_), sizeof(expressionText_)); }
void CalculatorApp::suspend() { capture(); }
void CalculatorApp::destroy() { capture(); if (root_) { lv_obj_delete(root_); root_ = nullptr; } expression_ = result_ = nullptr; }
void CalculatorApp::append(const char *text) { if (!expression_ || !text) return; lv_textarea_add_text(expression_, text); capture(); }
void CalculatorApp::calculate() { capture(); double value = 0; ExpressionParser parser(expressionText_); if (!parser.parse(value)) strlcpy(resultText_, "ERROR", sizeof(resultText_)); else { snprintf(resultText_, sizeof(resultText_), "%.8g", value); } if (result_) lv_label_set_text(result_, resultText_); }
void CalculatorApp::expressionReady(lv_event_t *event) { static_cast<CalculatorApp *>(lv_event_get_user_data(event))->calculate(); }
void CalculatorApp::buttonClicked(lv_event_t *event) { auto *self = static_cast<CalculatorApp *>(lv_event_get_user_data(event)); const char *key = static_cast<const char *>(lv_obj_get_user_data(lv_event_get_target_obj(event))); if (!key) return; if (!strcmp(key, "C")) { lv_textarea_set_text(self->expression_, ""); self->expressionText_[0] = 0; strlcpy(self->resultText_, "0", sizeof(self->resultText_)); lv_label_set_text(self->result_, self->resultText_); } else if (!strcmp(key, "=")) self->calculate(); else self->append(key); }
