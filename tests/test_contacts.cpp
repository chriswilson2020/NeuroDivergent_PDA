#include "data/ContactStore.h"
#include "apps/contacts/ContactsApp.h"
#include "hardware/StorageService.h"
#include "hardware/SPIBusManager.h"
#include "ui/FormFocus.h"
#include <SD.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
static lv_obj_t *button(lv_obj_t *root,const char *text){for(uint32_t i=0;i<lv_obj_get_child_count(root);++i){auto *c=lv_obj_get_child(root,i);if(lv_obj_check_type(c,&lv_button_class)){auto *l=lv_obj_get_child(c,0);if(l&&!strcmp(lv_label_get_text(l),text))return c;}if(auto *b=button(c,text))return b;}return nullptr;}
static lv_obj_t *input(lv_obj_t *root,const char *text){for(uint32_t i=0;i<lv_obj_get_child_count(root);++i){auto *c=lv_obj_get_child(root,i);if(lv_obj_check_type(c,&lv_textarea_class)&&!strcmp(lv_textarea_get_placeholder_text(c),text))return c;if(auto *t=input(c,text))return t;}return nullptr;}
static void click(ContactsApp &app,const char *text){auto *b=button(app.root(),text);assert(b);lv_obj_send_event(b,LV_EVENT_CLICKED,nullptr);FormFocus::ensure(app.root());assert(FormFocus::belongs(lv_group_get_focused(lv_group_get_default()),app.root()));}
static lv_color32_t pixels[480*222];
static void screenshot(lv_display_t *display,const char *path){lv_obj_invalidate(lv_screen_active());lv_refr_now(display);auto *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n480 222\n255\n");for(auto p:pixels){const unsigned char rgb[]={p.red,p.green,p.blue};fwrite(rgb,1,3,file);}fclose(file);}
int main(){
    StorageService storage;SPIBusManager bus;ContactStore store(storage,bus);assert(store.load()&&!store.count());
    ContactRecord a{};strcpy(a.name,"Zoe");strcpy(a.company,"School");strcpy(a.address,"Main Street 10\nAmsterdam");strcpy(a.email,"zoe@example.com");assert(store.upsert(a)&&a.id);
    ContactRecord b{};strcpy(b.company,"Alpha Ltd");strcpy(b.mobile,"+31 1234");assert(store.upsert(b));assert(!strcmp(store.at(0).company,"Alpha Ltd"));
    assert(ContactStore::matches(a,"AMSTERDAM")&&ContactStore::matches(a,"school")&&!ContactStore::matches(a,"zzzz"));
    a.favourite=1;assert(store.upsert(a)&&store.at(0).id==a.id);
    ContactStore reload(storage,bus);assert(reload.load()&&reload.count()==2&&reload.find(a.id)->favourite);
    File::failWrite=true;auto changed=a;strcpy(changed.name,"Lost update");assert(!store.upsert(changed)&&!strcmp(store.find(a.id)->name,"Zoe"));assert(!store.remove(a.id)&&store.count()==2);File::failWrite=false;
    SD.failRenameFrom="/PocketPDA/contacts/contacts.new";assert(!store.upsert(changed));assert(reload.load()&&!strcmp(reload.find(a.id)->name,"Zoe"));
    assert(SD.rename("/PocketPDA/contacts/contacts.dat","/PocketPDA/contacts/contacts.bak"));assert(reload.load()&&reload.count()==2); // interrupted final replacement
    ContactRecord empty{};assert(!store.upsert(empty));memset(empty.name,'x',sizeof(empty.name));assert(!ContactStore::valid(empty));
    empty={};strcpy(empty.name,"   ");assert(!store.upsert(empty));
    lv_init();auto *display=lv_display_create(480,222);lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,[](lv_display_t *d,const lv_area_t *,uint8_t *){lv_display_flush_ready(d);});
    auto *host=lv_obj_create(lv_screen_active());lv_obj_set_pos(host,0,28);lv_obj_set_size(host,480,194);lv_obj_set_style_pad_all(host,0,0);lv_obj_set_style_border_width(host,0,0);lv_obj_set_scrollable(host,false);
    auto *g=lv_group_create();lv_group_set_default(g);
    ContactsApp app(store);app.create(host);FormFocus::ensure(app.root());screenshot(display,"/tmp/pocketpda-contacts-list.ppm");click(app,"+ CONTACT");
    lv_textarea_set_text(input(app.root(),"NAME"),"Gabriel");lv_textarea_set_text(input(app.root(),"COMPANY"),"School");lv_textarea_set_text(input(app.root(),"POSTAL ADDRESS"),"Somewhere 12\nTown");lv_textarea_set_text(input(app.root(),"NOTES"),"Emergency contact\nCall parent first");
    screenshot(display,"/tmp/pocketpda-contacts-editor.ppm");app.suspend();app.destroy();app.create(host);assert(!strcmp(lv_textarea_get_text(input(app.root(),"NAME")),"Gabriel"));click(app,"SAVE");assert(store.count()==3);screenshot(display,"/tmp/pocketpda-contacts-details.ppm");
    click(app,"EDIT");lv_textarea_set_text(input(app.root(),"EMAIL"),"parent@example.com");click(app,"SAVE");click(app,"DELETE");click(app,"CANCEL");assert(store.count()==3);click(app,"DELETE");click(app,"DELETE");assert(store.count()==2);
    auto *q=input(app.root(),"Search contacts / company / phone");lv_textarea_set_text(q,"unmatched");assert(lv_group_get_focused(g)==q);click(app,"NEXT");click(app,"+ CONTACT");click(app,"SAVE");assert(store.count()==2);click(app,"CANCEL");
    // Multibyte limits must report an error rather than quietly losing text,
    // including when a draft is suspended and rebuilt.
    click(app,"+ CONTACT");std::string longName;for(int i=0;i<40;++i)longName+="é";
    lv_textarea_set_text(input(app.root(),"NAME"),longName.c_str());click(app,"SAVE");assert(store.count()==2);
    app.suspend();app.destroy();app.create(host);assert(!strcmp(lv_textarea_get_text(input(app.root(),"NAME")),longName.c_str()));click(app,"CANCEL");
    app.destroy();
    for(int i=0;i<18;++i){ContactRecord r{};snprintf(r.name,sizeof(r.name),"Contact %02d",i);assert(store.upsert(r));}
    app.create(host);auto *search=input(app.root(),"Search contacts / company / phone");
    while(*lv_textarea_get_text(search))lv_textarea_delete_char(search);
    assert(lv_obj_get_child_count(lv_obj_get_child(app.root(),2))==8);
    click(app,"NEXT");assert(!strcmp(lv_label_get_text(lv_obj_get_child(app.root(),3)),"20 contacts | Page 2 / 3"));
    click(app,"PREV");app.destroy();
    storage.available=false;assert(!reload.load()&&!reload.ready());storage.available=true;
    auto blob=SD.files.at("/PocketPDA/contacts/contacts.dat");(*blob)[0]^=1;assert(!reload.load()&&!reload.ready());assert(!reload.upsert(a));
    puts("Contacts persistence, search, sort, rollback, recovery, draft, editing, confirmation and focus tests passed");
}
