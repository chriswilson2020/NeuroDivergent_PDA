#pragma once
#include "core/App.h"
#include "data/ContactStore.h"
class ContactsApp:public App {
public:
    explicit ContactsApp(ContactStore &store):store_(store){}
    const char *id() const override { return "contacts"; }
    const char *title() const override { return "Contacts"; }
    void create(lv_obj_t *) override;
    void resume() override {}
    void suspend() override;
    void destroy() override;
    lv_obj_t *root() const override { return root_; }
private:
    enum class View { List,Details,Editor,Delete };
    void list();void rows();void details(uint32_t);void editor(uint32_t);void capture();void confirmDelete();
    static void add(lv_event_t *);static void select(lv_event_t *);static void search(lv_event_t *);
    static void page(lv_event_t *);static void edit(lv_event_t *);static void save(lv_event_t *);
    static void back(lv_event_t *);static void favourite(lv_event_t *);static void deleteAsk(lv_event_t *);static void deleteYes(lv_event_t *);
    ContactStore &store_;View view_=View::List;uint32_t selected_=0;size_t page_=0;bool draftReady_=false;
    ContactRecord draft_{};char query_[64]{};
    // Preserve full editor text across launcher navigation, including a field
    // that needs shortening before it can fit the on-disk byte limit.
    char draftText_[9][1024]{};
    lv_obj_t *root_=nullptr,*list_=nullptr,*search_=nullptr,*status_=nullptr,*fav_=nullptr;
    lv_obj_t *fields_[9]{};
};
