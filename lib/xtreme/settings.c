#include "xtreme.h"
#include "private.h"
#include <furi_hal.h>
#include <flipper_format/flipper_format.h>

#define TAG "XtremeSettings"

XtremeSettings xtreme_settings = {
    .asset_pack = "", // SFW
    .anim_speed = 100, // 100%
    .cycle_anims = 0, // Meta.txt
    .unlock_anims = false, // OFF
    .fallback_anim = true, // ON
    .wii_menu = true, // ON
    .lock_on_boot = false, // OFF
    .bad_pins_format = false, // OFF
    .pin_unlock_from_app = false, // OFF
    .lockscreen_time = true, // ON
    .lockscreen_seconds = false, // OFF
    .lockscreen_date = true, // ON
    .lockscreen_statusbar = true, // ON
    .lockscreen_prompt = true, // ON
    .battery_icon = BatteryIconBarPercent, // Bar %
    .statusbar_clock = false, // OFF
    .status_icons = true, // ON
    .bar_borders = true, // ON
    .bar_background = false, // OFF
    .sort_dirs_first = true, // ON
    .show_hidden_files = false, // OFF
    .show_internal_tab = false, // OFF
    .favorite_timeout = 0, // OFF
    .bad_bt = false, // USB
    .bad_bt_remember = false, // OFF
    .dark_mode = false, // OFF
    .rgb_backlight = false, // OFF
    .butthurt_timer = 21600, // 6 H
    .charge_cap = 100, // 100%
};

void XTREME_SETTINGS_LOAD() {
    if(!furi_hal_is_normal_boot()) return;

    XtremeSettings* x = &xtreme_settings;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_existing(file, XTREME_SETTINGS_PATH)) {
        FuriString* string = furi_string_alloc();
        if(flipper_format_read_string(file, "asset_pack", string)) {
            strlcpy(x->asset_pack, furi_string_get_cstr(string), XTREME_ASSETS_PACK_NAME_LEN);
        }
        furi_string_free(string);
        uint32_t u;
        int32_t i;
        bool b;
        flipper_format_rewind(file);
        if(flipper_format_read_uint32(file, "anim_speed", &u, 1)) {
            x->anim_speed = CLAMP(u, 300U, 25U);
        }
        flipper_format_rewind(file);
        if(flipper_format_read_int32(file, "cycle_anims", &i, 1)) {
            x->cycle_anims = CLAMP(i, 86400, -1);
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "unlock_anims", &b, 1)) {
            x->unlock_anims = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "fallback_anim", &b, 1)) {
            x->fallback_anim = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "wii_menu", &b, 1)) {
            x->wii_menu = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "bad_pins_format", &b, 1)) {
            x->bad_pins_format = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "pin_unlock_from_app", &b, 1)) {
            x->pin_unlock_from_app = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "lock_on_boot", &b, 1)) {
            x->lock_on_boot = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "lockscreen_time", &b, 1)) {
            x->lockscreen_time = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "lockscreen_seconds", &b, 1)) {
            x->lockscreen_seconds = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "lockscreen_date", &b, 1)) {
            x->lockscreen_date = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "lockscreen_statusbar", &b, 1)) {
            x->lockscreen_statusbar = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "lockscreen_prompt", &b, 1)) {
            x->lockscreen_prompt = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_uint32(file, "battery_icon", &u, 1)) {
            x->battery_icon = CLAMP(u, BatteryIconCount - 1U, 0U);
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "statusbar_clock", &b, 1)) {
            x->statusbar_clock = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "status_icons", &b, 1)) {
            x->status_icons = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "bar_borders", &b, 1)) {
            x->bar_borders = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "bar_background", &b, 1)) {
            x->bar_background = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "sort_dirs_first", &b, 1)) {
            x->sort_dirs_first = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "show_hidden_files", &b, 1)) {
            x->show_hidden_files = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "show_internal_tab", &b, 1)) {
            x->show_internal_tab = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_uint32(file, "favorite_timeout", &u, 1)) {
            x->favorite_timeout = CLAMP(u, 60U, 0U);
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "bad_bt", &b, 1)) {
            x->bad_bt = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "bad_bt_remember", &b, 1)) {
            x->bad_bt_remember = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "dark_mode", &b, 1)) {
            x->dark_mode = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_bool(file, "rgb_backlight", &b, 1)) {
            x->rgb_backlight = b;
        }
        flipper_format_rewind(file);
        if(flipper_format_read_uint32(file, "butthurt_timer", &u, 1)) {
            x->butthurt_timer = CLAMP(u, 172800U, 0U);
        }
        flipper_format_rewind(file);
        if(flipper_format_read_uint32(file, "charge_cap", &u, 1)) {
            x->charge_cap = CLAMP(u, 100U, 5U);
        }
    }
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void XTREME_SETTINGS_SAVE() {
    if(!furi_hal_is_normal_boot()) return;

    XtremeSettings* x = &xtreme_settings;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_always(file, XTREME_SETTINGS_PATH)) {
        flipper_format_write_string_cstr(file, "asset_pack", x->asset_pack);
        flipper_format_write_uint32(file, "anim_speed", &x->anim_speed, 1);
        flipper_format_write_int32(file, "cycle_anims", &x->cycle_anims, 1);
        flipper_format_write_bool(file, "unlock_anims", &x->unlock_anims, 1);
        flipper_format_write_bool(file, "fallback_anim", &x->fallback_anim, 1);
        flipper_format_write_bool(file, "wii_menu", &x->wii_menu, 1);
        flipper_format_write_bool(file, "bad_pins_format", &x->bad_pins_format, 1);
        flipper_format_write_bool(file, "pin_unlock_from_app", &x->pin_unlock_from_app, 1);
        flipper_format_write_bool(file, "lock_on_boot", &x->lock_on_boot, 1);
        flipper_format_write_bool(file, "lockscreen_time", &x->lockscreen_time, 1);
        flipper_format_write_bool(file, "lockscreen_seconds", &x->lockscreen_seconds, 1);
        flipper_format_write_bool(file, "lockscreen_date", &x->lockscreen_date, 1);
        flipper_format_write_bool(file, "lockscreen_statusbar", &x->lockscreen_statusbar, 1);
        flipper_format_write_bool(file, "lockscreen_prompt", &x->lockscreen_prompt, 1);
        flipper_format_write_uint32(file, "battery_icon", &x->battery_icon, 1);
        flipper_format_write_bool(file, "statusbar_clock", &x->statusbar_clock, 1);
        flipper_format_write_bool(file, "status_icons", &x->status_icons, 1);
        flipper_format_write_bool(file, "bar_borders", &x->bar_borders, 1);
        flipper_format_write_bool(file, "bar_background", &x->bar_background, 1);
        flipper_format_write_bool(file, "sort_dirs_first", &x->sort_dirs_first, 1);
        flipper_format_write_bool(file, "show_hidden_files", &x->show_hidden_files, 1);
        flipper_format_write_bool(file, "show_internal_tab", &x->show_internal_tab, 1);
        flipper_format_write_uint32(file, "favorite_timeout", &x->favorite_timeout, 1);
        flipper_format_write_bool(file, "bad_bt", &x->bad_bt, 1);
        flipper_format_write_bool(file, "bad_bt_remember", &x->bad_bt_remember, 1);
        flipper_format_write_bool(file, "dark_mode", &x->dark_mode, 1);
        flipper_format_write_bool(file, "rgb_backlight", &x->rgb_backlight, 1);
        flipper_format_write_uint32(file, "butthurt_timer", &x->butthurt_timer, 1);
        flipper_format_write_uint32(file, "charge_cap", &x->charge_cap, 1);
    }
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

XtremeSettings* XTREME_SETTINGS() {
    return &xtreme_settings;
}
