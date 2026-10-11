#include "ContactsApp.h"
#include "ui/FormFocus.h"
#include "ui/FormWidgets.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace {
constexpr size_t pageSize=8;
const char *labels[]={"NAME","COMPANY","JOB TITLE","MOBILE","OTHER PHONE","EMAIL","POSTAL ADDRESS","WEBSITE","NOTES"};
char *field(ContactRecord &r,size_t i){char *values[]={r.name,r.company,r.job,r.mobile,r.phone,r.email,r.address,r.website,r.notes};return values[i];}
const size_t sizes[]={64,64,48,32,32,96,160,96,256};
lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int width=460){auto *o=lv_label_create(parent);lv_label_set_text(o,text);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);return o;}
lv_obj_t *panel(lv_obj_t *parent,int y,int height){auto *o=lv_obj_create(parent);lv_obj_set_pos(o,0,y);lv_obj_set_size(o,469,height);lv_obj_set_style_pad_all(o,4,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_flex_flow(o,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(o,5,0);return o;}
ContactsApp *self(lv_event_t *e){return static_cast<ContactsApp*>(lv_event_get_user_data(e));}
}
void ContactsApp::create(lv_obj_t *parent){root_=lv_obj_create(parent);lv_obj_set_size(root_,LV_PCT(100),LV_PCT(100));lv_obj_set_style_pad_all(root_,5,0);lv_obj_set_style_border_width(root_,0,0);lv_obj_set_style_radius(root_,0,0);lv_obj_set_scrollable(root_,false);if(view_==View::Editor&&draftReady_)editor(selected_);else if(view_==View::Details||view_==View::Delete)details(selected_);else list();}
void ContactsApp::suspend(){if(view_==View::Editor)capture();}
void ContactsApp::destroy(){if(root_)lv_obj_delete(root_);root_=list_=search_=status_=fav_=nullptr;for(auto &f:fields_)f=nullptr;}
void ContactsApp::list(){FormFocus::View focus(root_);view_=View::List;draftReady_=false;lv_obj_clean(root_);
    search_=FormWidgets::text(root_,"Search contacts / company / phone",0,0,353,34);lv_textarea_set_max_length(search_,63);lv_textarea_set_text(search_,query_);lv_obj_add_event_cb(search_,search,LV_EVENT_VALUE_CHANGED,this);
    auto *b=FormWidgets::button(root_,"+ CONTACT",360,0,109,34,true);lv_obj_add_event_cb(b,add,LV_EVENT_CLICKED,this);if(!store_.ready())lv_obj_add_state(b,LV_STATE_DISABLED);
    list_=panel(root_,39,105);status_=label(root_,"",0,151,275);lv_obj_set_style_text_font(status_,&lv_font_montserrat_12,0);
    for(int i=0;i<2;++i){auto *p=FormWidgets::button(root_,i?"NEXT":"PREV",281+i*96,149,90,33);lv_obj_set_user_data(p,reinterpret_cast<void*>(uintptr_t(i+1)));lv_obj_add_event_cb(p,page,LV_EVENT_CLICKED,this);}
    rows();FormFocus::enter(search_);
}
void ContactsApp::rows(){
    // Rebuild only the results, retaining keyboard focus and the search caret.
    lv_obj_clean(list_);size_t matches=0;for(size_t i=0;i<store_.count();++i)if(ContactStore::matches(store_.at(i),query_))++matches;
    if(page_*pageSize>=matches)page_=matches?(matches-1)/pageSize:0;
    size_t index=0;for(size_t i=0;i<store_.count();++i){const auto &r=store_.at(i);if(!ContactStore::matches(r,query_))continue;if(index++<page_*pageSize||index>(page_+1)*pageSize)continue;
        auto *b=lv_button_create(list_);Theme::styleButton(b);lv_obj_set_size(b,LV_PCT(100),42);lv_obj_set_user_data(b,reinterpret_cast<void*>(uintptr_t(r.id)));lv_obj_add_event_cb(b,select,LV_EVENT_CLICKED,this);
        char text[144];snprintf(text,sizeof(text),"%s%s%s%s",r.favourite?"* ":"",*r.name?r.name:r.company,*r.name&&*r.company?"  |  ":"",*r.name?r.company:"");
        auto *l=lv_label_create(b);lv_obj_set_width(l,LV_PCT(100));lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_label_set_text(l,text);lv_obj_center(l);
    }
    if(!matches)label(list_,store_.ready()?"No contacts found. Use + CONTACT to add one.":store_.error(),0,0,440);
    char text[80];snprintf(text,sizeof(text),"%u contacts | Page %u / %u",unsigned(matches),unsigned(page_+1),unsigned(std::max(size_t(1),(matches+pageSize-1)/pageSize)));lv_label_set_text(status_,text);
}
void ContactsApp::details(uint32_t id){auto *r=store_.find(id);if(!r){list();return;}FormFocus::View focus(root_);view_=View::Details;selected_=id;draftReady_=false;lv_obj_clean(root_);
    auto *title=label(root_,*r->name?r->name:r->company,0,0,360);lv_obj_set_style_text_font(title,&lv_font_montserrat_20,0);lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);
    auto *f=FormWidgets::button(root_,r->favourite?"* FAV":"FAV",371,0,98,29,r->favourite);lv_obj_add_event_cb(f,favourite,LV_EVENT_CLICKED,this);
    auto *body=panel(root_,34,108);ContactRecord copy=*r;
    for(size_t i=0;i<9;++i)if(*field(copy,i)){
        auto *heading=label(body,labels[i],0,0,443);lv_obj_set_style_text_font(heading,&lv_font_montserrat_12,0);
        const char *p=field(copy,i);
        // Small focusable text blocks keep every part of long addresses/notes
        // reachable with the wheel, without needing a touchscreen.
        while(*p){size_t n=0;while(p[n]&&p[n]!='\n'&&n<48)++n;
            while(n&&p[n]&&(static_cast<unsigned char>(p[n])&0xC0)==0x80)--n;
            if(!n&&*p!='\n')n=1;
            char text[52]{};memcpy(text,p,n);auto *l=label(body,text,0,0,443);lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);
            lv_group_add_obj(lv_group_get_default(),l);lv_obj_set_scroll_on_focus(l,true);
            p+=n;if(*p=='\n')++p;
        }
    }
    const char *names[]={"EDIT","DELETE","BACK"};lv_event_cb_t callbacks[]={edit,deleteAsk,back};
    for(int i=0;i<3;++i){auto *b=FormWidgets::button(root_,names[i],i*158,148,153,35,i==0);lv_obj_add_event_cb(b,callbacks[i],LV_EVENT_CLICKED,this);}
    FormFocus::enter(lv_obj_get_child(root_,3));
}
void ContactsApp::editor(uint32_t id){FormFocus::View focus(root_);view_=View::Editor;selected_=id;lv_obj_clean(root_);
    if(!draftReady_){draft_={};if(const auto *r=store_.find(id))draft_=*r;for(size_t i=0;i<9;++i)snprintf(draftText_[i],sizeof(draftText_[i]),"%s",field(draft_,i));draftReady_=true;}
    auto *body=panel(root_,0,123);
    for(size_t i=0;i<9;++i){auto *l=label(body,labels[i],0,0,440);lv_obj_set_style_text_font(l,&lv_font_montserrat_12,0);
        fields_[i]=FormWidgets::text(body,labels[i],0,0,440,(i==6||i==8)?68:34,i!=6&&i!=8);lv_textarea_set_max_length(fields_[i],sizes[i]-1);lv_textarea_set_text(fields_[i],draftText_[i]);}
    fav_=lv_checkbox_create(body);lv_checkbox_set_text(fav_,"Favourite");if(draft_.favourite)lv_obj_add_state(fav_,LV_STATE_CHECKED);
    status_=label(root_,"Name or company required. Scroll for all fields.",0,126,465);lv_obj_set_style_text_font(status_,&lv_font_montserrat_12,0);
    auto *s=FormWidgets::button(root_,"SAVE",0,148,230,35,true);lv_obj_add_event_cb(s,save,LV_EVENT_CLICKED,this);
    auto *b=FormWidgets::button(root_,"CANCEL",238,148,231,35);lv_obj_add_event_cb(b,back,LV_EVENT_CLICKED,this);FormFocus::enter(fields_[0]);
}
void ContactsApp::capture(){for(size_t i=0;i<9;++i)if(fields_[i])snprintf(draftText_[i],sizeof(draftText_[i]),"%s",lv_textarea_get_text(fields_[i]));if(fav_)draft_.favourite=lv_obj_has_state(fav_,LV_STATE_CHECKED);}
void ContactsApp::confirmDelete(){FormFocus::View focus(root_);view_=View::Delete;lv_obj_clean(root_);auto *r=store_.find(selected_);char text[160];snprintf(text,sizeof(text),"Delete contact?\n\n%s\nThis cannot be undone.",r?(*r->name?r->name:r->company):"Missing contact");label(root_,text,12,8,440);
    status_=label(root_,"",12,108,440);lv_obj_set_style_text_font(status_,&lv_font_montserrat_12,0);
    auto *cancel=FormWidgets::button(root_,"CANCEL",0,148,230,35,true);lv_obj_add_event_cb(cancel,back,LV_EVENT_CLICKED,this);
    auto *yes=FormWidgets::button(root_,"DELETE",238,148,231,35);lv_obj_add_event_cb(yes,deleteYes,LV_EVENT_CLICKED,this);FormFocus::enter(cancel);
}
void ContactsApp::add(lv_event_t *e){auto *s=self(e);s->draftReady_=false;s->editor(0);}
void ContactsApp::select(lv_event_t *e){self(e)->details(uint32_t(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)))));}
void ContactsApp::search(lv_event_t *e){auto *s=self(e);snprintf(s->query_,sizeof(s->query_),"%s",lv_textarea_get_text(s->search_));s->page_=0;s->rows();}
void ContactsApp::page(lv_event_t *e){auto *s=self(e);const auto n=reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)));if(n==1){if(s->page_)--s->page_;}else ++s->page_;s->rows();FormFocus::enter(lv_event_get_target_obj(e));}
void ContactsApp::edit(lv_event_t *e){auto *s=self(e);s->draftReady_=false;s->editor(s->selected_);}
void ContactsApp::save(lv_event_t *e){auto *s=self(e);s->capture();for(size_t i=0;i<9;++i){if(strlen(s->draftText_[i])>=sizes[i]){char error[90];snprintf(error,sizeof(error),"%s too long (max %u UTF-8 bytes).",labels[i],unsigned(sizes[i]-1));lv_label_set_text(s->status_,error);FormFocus::enter(s->fields_[i]);return;}strcpy(field(s->draft_,i),s->draftText_[i]);}if(!s->store_.upsert(s->draft_)){lv_label_set_text(s->status_,s->store_.error());return;}s->details(s->draft_.id);}
void ContactsApp::back(lv_event_t *e){auto *s=self(e);s->draftReady_=false;if((s->view_==View::Editor||s->view_==View::Delete)&&s->selected_)s->details(s->selected_);else s->list();}
void ContactsApp::favourite(lv_event_t *e){auto *s=self(e);if(const auto *r=s->store_.find(s->selected_)){auto copy=*r;copy.favourite=!copy.favourite;if(s->store_.upsert(copy))s->details(copy.id);else{auto *l=lv_obj_get_child(lv_event_get_target_obj(e),0);lv_label_set_text(l,"SD ERROR");}}}
void ContactsApp::deleteAsk(lv_event_t *e){self(e)->confirmDelete();}
void ContactsApp::deleteYes(lv_event_t *e){auto *s=self(e);if(s->store_.remove(s->selected_))s->list();else lv_label_set_text(s->status_,s->store_.error());}
