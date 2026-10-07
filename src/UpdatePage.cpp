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

#include "font_persian_16.h"

// ==================================================
// USB / Firmware Configuration
// ==================================================

#define FW_ROOT        "/usb"
#define MAX_ENTRIES    200

// ==================================================
// Firmware Version
// ==================================================

#define FIRMWARE_VERSION "v 1.0.1"

// ==================================================
// Entry
// ==================================================

struct Entry
{
    std::string name;
    bool        dir;
};

// ==================================================
// State
// ==================================================

static std::vector<Entry> entries;

static std::string cur_path =
    FW_ROOT;

static int sel =
    0;

static bool need_scan =
    false;

static bool select_req =
    false;

// ==================================================
// Version Label
// ==================================================

static lv_obj_t *version_label =
    NULL;

// ==================================================
// Create / Update Version Label
// ==================================================

static void configure_version_label(void)
{
    lv_obj_t *current_screen =
        lv_scr_act();

    if (current_screen == NULL)
        return;

    // --------------------------------------------------
    // If old version label belongs to another screen,
    // remove it.
    // --------------------------------------------------

    if (
        version_label != NULL &&
        lv_obj_get_parent(version_label) != current_screen
    )
    {
        lv_obj_del(version_label);
        version_label = NULL;
    }

    // --------------------------------------------------
    // Create Version Label
    // --------------------------------------------------

    if (version_label == NULL)
    {
        version_label =
            lv_label_create(
                current_screen
            );
    }

    if (version_label == NULL)
        return;

    // --------------------------------------------------
    // Version Text
    // --------------------------------------------------

    lv_label_set_text(
        version_label,
        FIRMWARE_VERSION
    );

    // --------------------------------------------------
    // Top Left
    // --------------------------------------------------

    lv_obj_align(
        version_label,
        LV_ALIGN_TOP_LEFT,
        4,
        2
    );

    // --------------------------------------------------
    // LTR Direction
    // --------------------------------------------------

    lv_obj_set_style_base_dir(
        version_label,
        LV_BASE_DIR_LTR,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    // --------------------------------------------------
    // Left Alignment
    // --------------------------------------------------

    lv_obj_set_style_text_align(
        version_label,
        LV_TEXT_ALIGN_LEFT,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    // --------------------------------------------------
    // Default Font
    // --------------------------------------------------

    lv_obj_set_style_text_font(
        version_label,
        LV_FONT_DEFAULT,
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    // --------------------------------------------------
    // White Color
    // --------------------------------------------------

    lv_obj_set_style_text_color(
        version_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN |
        LV_STATE_DEFAULT
    );

    // --------------------------------------------------
    // Visible
    // --------------------------------------------------

    lv_obj_clear_flag(
        version_label,
        LV_OBJ_FLAG_HIDDEN
    );

    // --------------------------------------------------
    // Bring To Foreground
    // --------------------------------------------------

    lv_obj_move_foreground(
        version_label
    );
}

// ==================================================
// Configure Persian Labels
// ==================================================

static void configure_update_labels(void)
{
    // ==================================================
    // Page Title
    // ==================================================

    if (objects.update_page_title != NULL)
    {
        lv_label_set_text(
            objects.update_page_title,
            "بروزرسانی نرم افزار"
        );

        lv_obj_set_style_text_font(
            objects.update_page_title,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.update_page_title,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        // --------------------------------------------------
        // Full Width
        // --------------------------------------------------

        lv_obj_set_width(
            objects.update_page_title,
            lv_pct(100)
        );

        // --------------------------------------------------
        // Top Center
        // --------------------------------------------------

        lv_obj_align(
            objects.update_page_title,
            LV_ALIGN_TOP_MID,
            0,
            0
        );

        // --------------------------------------------------
        // Center Text
        // --------------------------------------------------

        lv_obj_set_style_text_align(
            objects.update_page_title,
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // ==================================================
    // UP Button
    // ==================================================

    if (objects.up_button_text != NULL)
    {
        lv_label_set_text(
            objects.up_button_text,
            "بالا"
        );

        lv_obj_set_style_text_font(
            objects.up_button_text,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.up_button_text,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // ==================================================
    // DOWN Button
    // ==================================================

    if (objects.down_button_text != NULL)
    {
        lv_label_set_text(
            objects.down_button_text,
            "پایین"
        );

        lv_obj_set_style_text_font(
            objects.down_button_text,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.down_button_text,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // ==================================================
    // SELECT Button
    // ==================================================

    if (objects.select_button_text != NULL)
    {
        lv_label_set_text(
            objects.select_button_text,
            "انتخاب"
        );

        lv_obj_set_style_text_font(
            objects.select_button_text,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.select_button_text,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // ==================================================
    // EXIT Button
    // ==================================================

    if (objects.update_exit_text != NULL)
    {
        lv_label_set_text(
            objects.update_exit_text,
            "خروج"
        );

        lv_obj_set_style_text_font(
            objects.update_exit_text,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.update_exit_text,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_text_align(
            objects.update_exit_text,
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // ==================================================
    // Version
    // ==================================================

    configure_version_label();
}

// ==================================================
// Set Persian Title / Status
// ==================================================

static void set_title(const char *txt)
{
    if (objects.update_page_title != NULL)
    {
        lv_label_set_text(
            objects.update_page_title,
            txt
        );

        lv_obj_set_style_text_font(
            objects.update_page_title,
            &font_persian_16,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        lv_obj_set_style_base_dir(
            objects.update_page_title,
            LV_BASE_DIR_RTL,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );

        // --------------------------------------------------
        // Full Width
        // --------------------------------------------------

        lv_obj_set_width(
            objects.update_page_title,
            lv_pct(100)
        );

        // --------------------------------------------------
        // Top Center
        // --------------------------------------------------

        lv_obj_align(
            objects.update_page_title,
            LV_ALIGN_TOP_MID,
            0,
            0
        );

        // --------------------------------------------------
        // Center Text
        // --------------------------------------------------

        lv_obj_set_style_text_align(
            objects.update_page_title,
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN |
            LV_STATE_DEFAULT
        );
    }

    // --------------------------------------------------
    // Keep Version Visible
    // --------------------------------------------------

    configure_version_label();
}

// ==================================================
// Check BIN File
// ==================================================

static bool is_bin(const std::string &n)
{
    if (n.size() < 4)
        return false;

    std::string e =
        n.substr(
            n.size() - 4
        );

    std::transform(
        e.begin(),
        e.end(),
        e.begin(),
        ::tolower
    );

    return e == ".bin";
}

// ==================================================
// Apply Selection
// ==================================================

static void apply_selection(void)
{
    if (objects.list_window == NULL)
        return;

    uint32_t cnt =
        lv_obj_get_child_cnt(
            objects.list_window
        );

    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *b =
            lv_obj_get_child(
                objects.list_window,
                i
            );

        if ((int)i == sel)
        {
            lv_obj_add_state(
                b,
                LV_STATE_CHECKED
            );
        }
        else
        {
            lv_obj_clear_state(
                b,
                LV_STATE_CHECKED
            );
        }
    }

    if (
        sel >= 0 &&
        (uint32_t)sel < cnt
    )
    {
        lv_obj_scroll_to_view(
            lv_obj_get_child(
                objects.list_window,
                sel
            ),
            LV_ANIM_OFF
        );
    }
}

// ==================================================
// Add One Row
// ==================================================

static void add_row(
    const char *symbol,
    const char *name
)
{
    lv_obj_t *btn =
        lv_btn_create(
            objects.list_window
        );

    lv_obj_set_width(
        btn,
        lv_pct(100)
    );

    lv_obj_set_height(
        btn,
        30
    );

    lv_obj_t *lbl =
        lv_label_create(btn);

    lv_obj_set_width(
        lbl,
        lv_pct(100)
    );

    lv_label_set_long_mode(
        lbl,
        LV_LABEL_LONG_DOT
    );

    lv_label_set_text_fmt(
        lbl,
        "%s  %s",
        symbol,
        name
    );

    lv_obj_align(
        lbl,
        LV_ALIGN_LEFT_MID,
        0,
        0
    );

    // --------------------------------------------------
    // IMPORTANT:
    // list_window remains unchanged.
    // No Persian font.
    // No RTL.
    // --------------------------------------------------
}

// ==================================================
// Rebuild List
// ==================================================

static void rebuild_list(void)
{
    if (objects.list_window == NULL)
        return;

    lv_obj_clean(
        objects.list_window
    );

    for (const Entry &e : entries)
    {
        add_row(
            e.dir
                ? LV_SYMBOL_DIRECTORY
                : LV_SYMBOL_FILE,
            e.name.c_str()
        );
    }

    sel = 0;

    apply_selection();
}

// ==================================================
// Scan Directory
// ==================================================

static void scan_dir(void)
{
    entries.clear();

    // ==================================================
    // USB Not Mounted
    // ==================================================

    if (!usb_msc_mounted())
    {
        set_title(
            "فلش USB متصل نیست"
        );

        rebuild_list();

        return;
    }

    // ==================================================
    // Open Directory
    // ==================================================

    DIR *d =
        opendir(
            cur_path.c_str()
        );

    if (d == NULL)
    {
        set_title(
            "خطا در خواندن"
        );

        rebuild_list();

        return;
    }

    // ==================================================
    // Parent Directory
    // ==================================================

    if (cur_path != FW_ROOT)
    {
        entries.push_back(
            {
                "..",
                true
            }
        );
    }

    // ==================================================
    // Temporary Lists
    // ==================================================

    std::vector<Entry> dirs;
    std::vector<Entry> files;

    struct dirent *de;

    // ==================================================
    // Read Directory
    // ==================================================

    while (
        (de = readdir(d)) != NULL &&
        (dirs.size() + files.size()) < MAX_ENTRIES
    )
    {
        std::string n =
            de->d_name;

        if (
            n == "." ||
            n == ".."
        )
        {
            continue;
        }

        if (de->d_type == DT_DIR)
        {
            dirs.push_back(
                {
                    n,
                    true
                }
            );
        }
        else if (is_bin(n))
        {
            files.push_back(
                {
                    n,
                    false
                }
            );
        }
    }

    closedir(d);

    // ==================================================
    // Sort
    // ==================================================

    auto cmp =
        [](const Entry &a,
           const Entry &b)
        {
            return a.name < b.name;
        };

    std::sort(
        dirs.begin(),
        dirs.end(),
        cmp
    );

    std::sort(
        files.begin(),
        files.end(),
        cmp
    );

    // ==================================================
    // Add Directories
    // ==================================================

    entries.insert(
        entries.end(),
        dirs.begin(),
        dirs.end()
    );

    // ==================================================
    // Add BIN Files
    // ==================================================

    entries.insert(
        entries.end(),
        files.begin(),
        files.end()
    );

    // ==================================================
    // Title
    // ==================================================

    if (entries.empty())
    {
        set_title(
            "موردی پیدا نشد"
        );
    }
    else
    {
        set_title(
            "بروزرسانی نرم افزار"
        );
    }

    rebuild_list();
}

// ==================================================
// Run Firmware Update
// ==================================================

static void run_update(
    const std::string &path
)
{
    FILE *f =
        fopen(
            path.c_str(),
            "rb"
        );

    // ==================================================
    // Open Fail
    // ==================================================

    if (f == NULL)
    {
        set_title(
            "خطا در باز کردن فایل"
        );

        return;
    }

    // ==================================================
    // File Size
    // ==================================================

    fseek(
        f,
        0,
        SEEK_END
    );

    long size =
        ftell(f);

    fseek(
        f,
        0,
        SEEK_SET
    );

    // ==================================================
    // Validate Size / Begin Update
    // ==================================================

    if (
        size <= 0 ||
        !Update.begin(
            (size_t)size
        )
    )
    {
        fclose(f);

        set_title(
            "حجم فایل نامعتبر است"
        );

        return;
    }

    // ==================================================
    // Buffer
    // ==================================================

    static uint8_t buf[4096];

    size_t done =
        0;

    uint32_t last_ui =
        0;

    bool ok =
        true;

    // ==================================================
    // Write Firmware
    // ==================================================

    while (
        done < (size_t)size
    )
    {
        size_t n =
            fread(
                buf,
                1,
                sizeof(buf),
                f
            );

        if (n == 0)
        {
            ok = false;
            break;
        }

        if (
            Update.write(
                buf,
                n
            ) != n
        )
        {
            ok = false;
            break;
        }

        done += n;

        esp_task_wdt_reset();

        // ==================================================
        // Update UI
        // ==================================================

        if (
            millis() - last_ui > 200
        )
        {
            char t[40];

            snprintf(
                t,
                sizeof(t),
                "در حال بروزرسانی... %u%%",
                (unsigned)(
                    done * 100UL / size
                )
            );

            set_title(t);

            lv_timer_handler();

            last_ui =
                millis();
        }
    }

    fclose(f);

    // ==================================================
    // Update Successful
    // ==================================================

    if (
        ok &&
        Update.end(true)
    )
    {
        set_title(
            "بروزرسانی انجام شد"
        );

        lv_timer_handler();

        delay(800);

        ESP.restart();
    }

    // ==================================================
    // Update Failed
    // ==================================================

    else
    {
        Update.abort();

        set_title(
            "بروزرسانی ناموفق بود"
        );
    }
}

// ==================================================
// PUBLIC
// ==================================================

// ==================================================
// Enter Update Page
// ==================================================

void update_page_enter(void)
{
    cur_path =
        FW_ROOT;

    need_scan =
        true;

    configure_update_labels();
    configure_version_label();
}

// ==================================================
// UP
// ==================================================

void update_page_up(void)
{
    if (entries.empty())
        return;

    sel =
        (sel > 0)
            ? sel - 1
            : (int)entries.size() - 1;

    apply_selection();
}

// ==================================================
// DOWN
// ==================================================

void update_page_down(void)
{
    if (entries.empty())
        return;

    sel =
        (sel + 1 < (int)entries.size())
            ? sel + 1
            : 0;

    apply_selection();
}

// ==================================================
// SELECT REQUEST
// ==================================================

void update_page_select(void)
{
    select_req =
        true;
}

// ==================================================
// Process Update Page
// ==================================================

void update_page_process(void)
{
    static uint32_t seen_gen =
        0;

    // ==================================================
    // Page Check
    // ==================================================

    if (
        !screen_manager_is(
            SCREEN_ID_UPDATE_PAGE
        )
    )
    {
        return;
    }

    // ==================================================
    // Configure UI
    // ==================================================

    configure_update_labels();

    // ==================================================
    // USB Plug / Unplug Detection
    // ==================================================

    uint32_t g =
        usb_msc_generation();

    if (g != seen_gen)
    {
        seen_gen =
            g;

        cur_path =
            FW_ROOT;

        need_scan =
            true;
    }

    // ==================================================
    // Scan
    // ==================================================

    if (need_scan)
    {
        need_scan =
            false;

        scan_dir();

        return;
    }

    // ==================================================
    // No Selection Request
    // ==================================================

    if (!select_req)
        return;

    select_req =
        false;

    // ==================================================
    // Invalid Selection
    // ==================================================

    if (
        sel < 0 ||
        sel >= (int)entries.size()
    )
    {
        return;
    }

    const Entry &e =
        entries[sel];

    // ==================================================
    // DIRECTORY
    // ==================================================

    if (e.dir)
    {
        // --------------------------------------------------
        // Parent Directory
        // --------------------------------------------------

        if (e.name == "..")
        {
            size_t p =
                cur_path.find_last_of('/');

            cur_path =
                (
                    p == std::string::npos ||
                    p == 0
                )
                    ? FW_ROOT
                    : cur_path.substr(
                        0,
                        p
                    );
        }

        // --------------------------------------------------
        // Enter Directory
        // --------------------------------------------------

        else
        {
            cur_path +=
                "/" +
                e.name;
        }

        scan_dir();
    }

    // ==================================================
    // FILE
    // ==================================================

    else
    {
        run_update(
            cur_path +
            "/" +
            e.name
        );
    }
}
