#include <Arduino.h>
#include <lvgl.h>
#include <Update.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include "esp_task_wdt.h"

extern "C"
{
#include "ui/screens.h"
}

#include "screen_manager.h"
#include "UpdatePage.h"
#include "UsbHost.h"

// Mount point of the storage (must match USB_MOUNT_POINT in UsbHost.cpp)
#define FW_ROOT        "/usb"
#define MAX_ENTRIES    200

struct Entry
{
    std::string name;
    bool        dir;
};

static std::vector<Entry> entries;
static std::string        cur_path   = FW_ROOT;
static int                sel        = 0;
static bool               need_scan  = false;
static bool               select_req = false;

static void set_title(const char *txt)
{
    if (objects.update_page_title != NULL)
        lv_label_set_text(objects.update_page_title, txt);
}

static bool is_bin(const std::string &n)
{
    if (n.size() < 4) return false;
    std::string e = n.substr(n.size() - 4);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e == ".bin";
}

static void apply_selection(void)
{
    if (objects.list_window == NULL) return;

    uint32_t cnt = lv_obj_get_child_cnt(objects.list_window);

    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *b = lv_obj_get_child(objects.list_window, i);

        if ((int)i == sel) lv_obj_add_state(b, LV_STATE_CHECKED);
        else               lv_obj_clear_state(b, LV_STATE_CHECKED);
    }

    if (sel >= 0 && (uint32_t)sel < cnt)
        lv_obj_scroll_to_view(
            lv_obj_get_child(objects.list_window, sel), LV_ANIM_OFF);
}

// Adds one row (button + label) without using lv_list_add_btn()
static void add_row(const char *symbol, const char *name)
{
    lv_obj_t *btn = lv_btn_create(objects.list_window);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, 30);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text_fmt(lbl, "%s  %s", symbol, name);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
}

static void rebuild_list(void)
{
    if (objects.list_window == NULL) return;

    lv_obj_clean(objects.list_window);

    for (const Entry &e : entries)
    {
        add_row(
            e.dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE,
            e.name.c_str());
    }

    sel = 0;
    apply_selection();
}

static void scan_dir(void)
{
    entries.clear();

    if (!usb_msc_mounted())
    {
        set_title("NO USB");
        rebuild_list();
        return;
    }

    DIR *d = opendir(cur_path.c_str());

    if (d == NULL)
    {
        set_title("READ FAIL");
        rebuild_list();
        return;
    }

    if (cur_path != FW_ROOT)
        entries.push_back({"..", true});

    std::vector<Entry> dirs, files;
    struct dirent *de;

    while ((de = readdir(d)) != NULL && (dirs.size() + files.size()) < MAX_ENTRIES)
    {
        std::string n = de->d_name;
        if (n == "." || n == "..") continue;

        if (de->d_type == DT_DIR) dirs.push_back({n, true});
        else if (is_bin(n))       files.push_back({n, false});
    }
    closedir(d);

    auto cmp = [](const Entry &a, const Entry &b){ return a.name < b.name; };
    std::sort(dirs.begin(),  dirs.end(),  cmp);
    std::sort(files.begin(), files.end(), cmp);

    entries.insert(entries.end(), dirs.begin(),  dirs.end());
    entries.insert(entries.end(), files.begin(), files.end());

    set_title(entries.empty() ? "EMPTY" : "update");
    rebuild_list();
}

static void run_update(const std::string &path)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (f == NULL) { set_title("OPEN FAIL"); return; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || !Update.begin((size_t)size))
    {
        fclose(f);
        set_title("BAD SIZE");
        return;
    }

    static uint8_t buf[4096];
    size_t   done    = 0;
    uint32_t last_ui = 0;
    bool     ok      = true;

    while (done < (size_t)size)
    {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n == 0) { ok = false; break; }

        if (Update.write(buf, n) != n) { ok = false; break; }

        done += n;
        esp_task_wdt_reset();

        if (millis() - last_ui > 200)
        {
            char t[24];
            snprintf(t, sizeof(t), "WRITE %u%%", (unsigned)(done * 100UL / size));
            set_title(t);
            lv_timer_handler();
            last_ui = millis();
        }
    }
    fclose(f);

    if (ok && Update.end(true))
    {
        set_title("DONE - REBOOT");
        lv_timer_handler();
        delay(800);
        ESP.restart();
    }
    else
    {
        Update.abort();
        set_title("UPDATE FAIL");
    }
}

// ---------------- public ----------------

void update_page_enter(void)
{
    cur_path  = FW_ROOT;
    need_scan = true;
}

void update_page_up(void)
{
    if (entries.empty()) return;
    sel = (sel > 0) ? sel - 1 : (int)entries.size() - 1;
    apply_selection();
}

void update_page_down(void)
{
    if (entries.empty()) return;
    sel = (sel + 1 < (int)entries.size()) ? sel + 1 : 0;
    apply_selection();
}

void update_page_select(void)
{
    select_req = true;   // handled outside the LVGL callback
}

void update_page_process(void)
{
    static uint32_t seen_gen = 0;

    if (!screen_manager_is(SCREEN_ID_UPDATE_PAGE))
        return;

    // Flash drive plugged / unplugged -> rescan from root
    uint32_t g = usb_msc_generation();
    if (g != seen_gen)
    {
        seen_gen  = g;
        cur_path  = FW_ROOT;
        need_scan = true;
    }

    if (need_scan)
    {
        need_scan = false;
        scan_dir();
        return;
    }

    if (!select_req) return;
    select_req = false;

    if (sel < 0 || sel >= (int)entries.size()) return;

    const Entry &e = entries[sel];

    if (e.dir)
    {
        if (e.name == "..")
        {
            size_t p = cur_path.find_last_of('/');
            cur_path = (p == std::string::npos || p == 0) ? FW_ROOT : cur_path.substr(0, p);
        }
        else
        {
            cur_path += "/" + e.name;
        }
        scan_dir();
    }
    else
    {
        run_update(cur_path + "/" + e.name);
    }
}