#pragma once
#include "core/App.h"
#include <stddef.h>
#include <stdint.h>
class StorageService;
class SPIBusManager;

class FilesApp : public App {
public:
    FilesApp(StorageService &storage, SPIBusManager &bus) : storage_(storage), bus_(bus) {}
    const char *id() const override { return "files"; }
    const char *title() const override { return "Files"; }
    void create(lv_obj_t *parent) override;
    void resume() override {}
    void suspend() override {}
    void destroy() override;
    lv_obj_t *root() const override { return root_; }
private:
    struct Entry { char name[64]{}; uint32_t size = 0; bool directory = false; };
    enum class View : uint8_t { List, Preview };
    void loadEntries();
    void showList();
    void showPreview(const char *name);
    void goUp();
    void makePath(const char *name, char *output, size_t capacity) const;
    static void entryClicked(lv_event_t *event);
    static void upClicked(lv_event_t *event);
    static void backClicked(lv_event_t *event);
    static void deleteClicked(lv_event_t *event);
    StorageService &storage_;
    SPIBusManager &bus_;
    lv_obj_t *root_ = nullptr;
    Entry entries_[32]{};
    size_t count_ = 0;
    View view_ = View::List;
    char currentPath_[128] = "/PocketPDA/files";
    char previewPath_[192]{};
    char preview_[1537]{};
};
