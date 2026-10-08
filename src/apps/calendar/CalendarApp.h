#pragma once
#include "core/App.h"
#include "data/CalendarStore.h"
class RTCService;

class CalendarApp : public App {
public:
    CalendarApp(CalendarStore &store, RTCService &rtc) : store_(store), rtc_(rtc) {}
    const char *id() const override { return "calendar"; }
    const char *title() const override { return "Calendar"; }
    void create(lv_obj_t *parent) override; void resume() override; void suspend() override; void destroy() override; lv_obj_t *root() const override { return root_; }
private:
    enum class View : uint8_t { Agenda, Editor };
    void showAgenda(); void showEditor(uint32_t id); void captureDraft(); void changeDay(int delta); bool parseDraft();
    void deferAgenda(); void deferEditor(uint32_t id);
    static void showAgendaAsync(void *context); static void showEditorAsync(void *context);
    static void navClicked(lv_event_t *e); static void newClicked(lv_event_t *e); static void eventClicked(lv_event_t *e);
    static void saveClicked(lv_event_t *e); static void deleteClicked(lv_event_t *e); static void cancelClicked(lv_event_t *e);
    CalendarStore &store_; RTCService &rtc_; lv_obj_t *root_ = nullptr; lv_obj_t *titleField_ = nullptr; lv_obj_t *locationField_ = nullptr;
    lv_obj_t *dateField_ = nullptr; lv_obj_t *startField_ = nullptr; lv_obj_t *endField_ = nullptr; lv_obj_t *reminderField_ = nullptr; lv_obj_t *repeatField_ = nullptr;
    View view_ = View::Agenda; int year_ = 0, month_ = 1, day_ = 1; uint32_t editingId_ = 0; CalendarEvent draft_{}; bool hasDraft_ = false;
    uint32_t pendingEditorId_ = 0; int8_t agendaFocus_ = 0; bool redrawPending_ = false;
};
