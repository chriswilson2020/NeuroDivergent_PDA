#include "ui/FormFocus.h"
#include "core/AppManager.h"
#include "core/NotificationManager.h"
#include "hardware/HapticService.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
void HapticService::play(uint8_t) {} // only physical haptics are stubbed

// Exercise production focus policy and app lifecycle with real LVGL widgets,
// not a mocked focus implementation. Data stores/hardware are not involved.
class TestApp : public App {
public:
    TestApp(const char *name,bool readOnly=false):name_(name),readOnly_(readOnly) {}
    const char *id() const override { return name_; }
    const char *title() const override { return name_; }
    lv_obj_t *root() const override { return root_; }
    void create(lv_obj_t *parent) override { root_=lv_obj_create(parent); rebuild(false); }
    void resume() override {}
    void suspend() override {}
    void destroy() override { lv_obj_delete(root_);root_=nullptr; }
    void rebuild(bool populated) {
        FormFocus::View focusView(root_);
        lv_obj_clean(root_);
        lv_label_create(root_);
        if(readOnly_) return;
        first=lv_button_create(root_);
        if(!populated) return; // empty list early-return path
        auto *list=lv_obj_create(root_);
        for(int i=0;i<6;++i) lv_button_create(list);
    }
    lv_obj_t *first=nullptr;
private:
    const char *name_;
    bool readOnly_;
    lv_obj_t *root_=nullptr;
};

static void rebuildAction(void *context) { static_cast<TestApp*>(context)->rebuild(true); }

int main() {
    lv_init();lv_display_create(480,222);
    auto *group=lv_group_create();lv_group_set_default(group);
    auto *encoder=lv_indev_create();lv_indev_set_type(encoder,LV_INDEV_TYPE_ENCODER);
    auto *keyboard=lv_indev_create();lv_indev_set_type(keyboard,LV_INDEV_TYPE_KEYPAD);
    FormFocus::bindInputs(group);
    auto *screen=lv_screen_active();
    auto *launcher=lv_obj_create(screen);
    auto *launchButton=lv_button_create(launcher);
    lv_obj_set_hidden(launcher,true);
    TestApp apps[]={
        {"today",true},{"transition",true},{"capture"},{"calendar"},
        {"tasks"},{"assignments"},{"packing"},{"routines"},{"notes"},
        {"clock"},{"habits"},{"messages"},{"files"},{"settings"}
    };
    AppManager manager;manager.begin(screen);
    for(auto &app:apps) manager.registerApp(&app);
    for(int pass=0;pass<4;++pass) for(auto &app:apps) {
        assert(manager.launch(app.id()));
        assert(FormFocus::belongs(lv_group_get_focused(group),app.root()));
        // Round trip through launcher restores a visible, owned target.
        auto *before=lv_group_get_focused(group);
        lv_obj_set_hidden(app.root(),true);lv_obj_set_hidden(launcher,false);
        FormFocus::enter(launchButton);lv_group_set_editing(group,true);
        lv_obj_set_hidden(launcher,true);lv_obj_set_hidden(app.root(),false);
        FormFocus::ensure(app.root(),before);
        assert(lv_group_get_focused(group)==before&&!lv_group_get_editing(group));
        // Back from a text editor rebuilds the original list, including empty
        // list returns. It must not retain textarea editing mode.
        for(bool populated:{false,true,false}) {
            auto *field=lv_textarea_create(app.root());FormFocus::enter(field);
            lv_group_set_editing(group,true);
            app.rebuild(populated);
            assert(FormFocus::belongs(lv_group_get_focused(group),app.root()));
            assert(!lv_group_get_editing(group));
        }
        assert(manager.launch(app.id())); // same-app resume
        assert(FormFocus::belongs(lv_group_get_focused(group),app.root()));
    }

    auto *root=manager.active()->root();
    lv_obj_clean(root);
    // Disabled Add button (e.g. full habit store) and hidden parent controls
    // must be skipped, while off-screen scrollable rows remain reachable.
    auto *disabled=lv_button_create(root);lv_obj_add_state(disabled,LV_STATE_DISABLED);
    auto *hidden=lv_obj_create(root);lv_button_create(hidden);lv_obj_set_hidden(hidden,true);
    auto *list=lv_obj_create(root);auto *row=lv_button_create(list);
    lv_obj_set_y(row,1000);FormFocus::ensure(root);
    assert(lv_group_get_focused(group)==row);
    FormFocus::ensure(root,disabled);assert(lv_group_get_focused(group)==row);
    auto *deleted=lv_button_create(root);lv_obj_delete(deleted);
    FormFocus::ensure(root,deleted);assert(lv_group_get_focused(group)==row);
    auto *field=lv_textarea_create(root);FormFocus::enter(field);
    lv_group_set_editing(group,true);FormFocus::ensure(root);
    assert(lv_group_get_focused(group)==field&&lv_group_get_editing(group));
    auto *dropdown=lv_dropdown_create(root);FormFocus::enter(dropdown);
    lv_dropdown_open(dropdown);lv_group_set_editing(group,true);FormFocus::ensure(root);
    assert(lv_group_get_focused(group)==dropdown&&lv_group_get_editing(group));
    lv_dropdown_close(dropdown);

    // Notifications isolate both input devices; forward AND reverse rotation
    // wraps entirely inside the modal. A single-action reminder also wraps.
    auto *modal=lv_obj_create(screen);auto *dismiss=lv_button_create(modal);
    auto *action=lv_button_create(modal);auto *modalGroup=lv_group_create();
    lv_group_add_obj(modalGroup,dismiss);lv_group_add_obj(modalGroup,action);
    FormFocus::enter(dismiss);FormFocus::bindInputs(modalGroup);
    assert(lv_indev_get_group(encoder)==modalGroup&&lv_indev_get_group(keyboard)==modalGroup);
    lv_group_focus_prev(modalGroup);assert(lv_group_get_focused(modalGroup)==action);
    lv_group_focus_next(modalGroup);assert(lv_group_get_focused(modalGroup)==dismiss);
    lv_obj_set_hidden(action,true);
    for(int i=0;i<20;++i) {
        lv_group_focus_next(modalGroup);lv_group_focus_prev(modalGroup);
        assert(lv_group_get_focused(modalGroup)==dismiss);
    }
    // An action can rebuild the app while another notification is active.
    // Its input ownership must survive application focus changes.
    static_cast<TestApp*>(manager.active())->rebuild(true);
    FormFocus::ensure(modal,nullptr,modalGroup);FormFocus::bindInputs(modalGroup);
    assert(lv_group_get_focused(modalGroup)==dismiss);
    lv_obj_set_hidden(modal,true);FormFocus::bindInputs(group);FormFocus::ensure(root);
    assert(lv_indev_get_group(encoder)==group&&lv_indev_get_group(keyboard)==group);
    assert(FormFocus::belongs(lv_group_get_focused(group),root));
    lv_obj_delete(modal);lv_group_delete(modalGroup);
    // Exercise the actual notification manager, including queued reminders
    // and action callbacks which rebuild application controls.
    HapticService haptic;NotificationManager notifications;
    notifications.begin(screen,haptic);
    FormFocus::enter(lv_group_get_focused(group));
    auto *before=lv_group_get_focused(group);
    notifications.show("Confirm","Rebuild app","OPEN",rebuildAction,manager.active());
    auto *notificationGroup=lv_indev_get_group(encoder);
    assert(notificationGroup!=group&&lv_indev_get_group(keyboard)==notificationGroup);
    notifications.show("Next reminder","Queued notification");
    lv_group_focus_next(notificationGroup);
    auto *open=lv_group_get_focused(notificationGroup);
    lv_obj_send_event(open,LV_EVENT_CLICKED,nullptr);
    notifications.ensureFocus();
    assert(notifications.active());
    assert(lv_indev_get_group(encoder)==notificationGroup);
    assert(FormFocus::usable(lv_group_get_focused(notificationGroup)));
    notifications.dismiss();FormFocus::ensure(root,before);
    assert(!notifications.active()&&lv_indev_get_group(encoder)==group);
    assert(FormFocus::belongs(lv_group_get_focused(group),root));
    puts("LVGL focus recovery, 14 app lifecycles, rebuild/Back, empty/full, editing and modal tests passed");
}
