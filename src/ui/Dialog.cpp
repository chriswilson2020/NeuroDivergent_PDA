#include "Dialog.h"
namespace Dialog {
void showInfo(const char *title, const char *message) {
    lv_obj_t *box = lv_msgbox_create(nullptr);
    lv_msgbox_add_title(box, title);
    lv_msgbox_add_text(box, message);
    lv_msgbox_add_close_button(box);
    lv_obj_center(box);
}
}
