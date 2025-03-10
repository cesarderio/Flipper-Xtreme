#include <storage/storage.h>
#include <assets_icons.h>
#include <gui/gui.h>
#include <gui/view_stack.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <furi.h>
#include <furi_hal.h>
#include <cli/cli.h>
#include <cli/cli_vcp.h>
#include <locale/locale.h>
#include <xtreme.h>

#include "animations/animation_manager.h"
#include "desktop/scenes/desktop_scene.h"
#include "desktop/scenes/desktop_scene_i.h"
#include "desktop/views/desktop_view_locked.h"
#include "desktop/views/desktop_view_pin_input.h"
#include "desktop/views/desktop_view_pin_timeout.h"
#include "desktop_i.h"
#include "helpers/pin.h"

#define TAG "Desktop"

static void desktop_auto_lock_arm(Desktop*);
static void desktop_auto_lock_inhibit(Desktop*);
static void desktop_start_auto_lock_timer(Desktop*);

static void desktop_loader_callback(const void* message, void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    const LoaderEvent* event = message;

    if(event->type == LoaderEventTypeApplicationStarted) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalBeforeAppStarted);
    } else if(event->type == LoaderEventTypeApplicationStopped) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalAfterAppFinished);
    }
}

static void desktop_lock_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_Lock_7x8);
}

static void desktop_clock_upd_time(Desktop* desktop, bool forced) {
    furi_assert(desktop);

    FuriHalRtcDateTime curr_dt;
    furi_hal_rtc_get_datetime(&curr_dt);

    if(forced) {
        desktop->clock_type = (locale_get_time_format() == LocaleTimeFormat24h);
    }

    if(forced || (desktop->minute != curr_dt.minute)) {
        if(desktop->clock_type) {
            desktop->hour = curr_dt.hour;
        } else {
            desktop->hour = (curr_dt.hour > 12) ? curr_dt.hour - 12 :
                                                  ((curr_dt.hour == 0) ? 12 : curr_dt.hour);
        }
        desktop->minute = curr_dt.minute;
        view_port_update(desktop->clock_viewport);
    }
}

static void desktop_clock_toggle_view(Desktop* desktop, bool is_enabled) {
    furi_assert(desktop);

    desktop_clock_upd_time(desktop, true);

    if(is_enabled) { // && !furi_timer_is_running(desktop->update_clock_timer)) {
        furi_timer_start(desktop->update_clock_timer, furi_ms_to_ticks(1000));
    } else if(!is_enabled) { //&& furi_timer_is_running(desktop->update_clock_timer)) {
        furi_timer_stop(desktop->update_clock_timer);
    }

    view_port_enabled_set(desktop->clock_viewport, is_enabled);
}

static uint8_t desktop_clock_get_num_w(uint8_t num) {
    if(num == 1) {
        return 3;
    } else if(num == 4) {
        return 6;
    } else {
        return 5;
    }
}

static const char* digit[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

static void desktop_clock_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    furi_assert(canvas);

    Desktop* desktop = context;

    uint8_t d[4] = {
        desktop->minute % 10,
        desktop->minute / 10,
        desktop->hour % 10,
        desktop->hour / 10,
    };

    canvas_set_font(canvas, FontPrimary);

    uint8_t new_w = desktop_clock_get_num_w(d[0]) + //c1
                    desktop_clock_get_num_w(d[1]) + //c2
                    desktop_clock_get_num_w(d[2]) + //c3
                    desktop_clock_get_num_w(d[3]) + //c4
                    2 + 4; // ":" + 4 separators

    // further away from the battery charge indicator, if the smallest minute is 1
    view_port_set_width(desktop->clock_viewport, new_w - !(d[0] == 1));

    uint8_t x = new_w;

    uint8_t y = 8;
    uint8_t offset_r;

    canvas_draw_str_aligned(canvas, x, y, AlignRight, AlignBottom, digit[d[0]]);
    offset_r = desktop_clock_get_num_w(d[0]);

    canvas_draw_str_aligned(canvas, x -= (offset_r + 1), y, AlignRight, AlignBottom, digit[d[1]]);
    offset_r = desktop_clock_get_num_w(d[1]);

    canvas_draw_str_aligned(canvas, x -= (offset_r + 1), y - 1, AlignRight, AlignBottom, ":");
    offset_r = 2;

    canvas_draw_str_aligned(canvas, x -= (offset_r + 1), y, AlignRight, AlignBottom, digit[d[2]]);
    offset_r = desktop_clock_get_num_w(d[2]);

    canvas_draw_str_aligned(canvas, x -= (offset_r + 1), y, AlignRight, AlignBottom, digit[d[3]]);
}

static void desktop_stealth_mode_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_Muted_8x8);
}

static bool desktop_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Desktop* desktop = (Desktop*)context;

    switch(event) {
    case DesktopGlobalBeforeAppStarted:
        animation_manager_unload_and_stall_animation(desktop->animation_manager);
        desktop_auto_lock_inhibit(desktop);
        return true;
    case DesktopGlobalAfterAppFinished:
        animation_manager_load_and_continue_animation(desktop->animation_manager);
        desktop_clock_toggle_view(desktop, XTREME_SETTINGS()->statusbar_clock);
        desktop_auto_lock_arm(desktop);
        return true;
    case DesktopGlobalAutoLock:
        if(!loader_is_locked(desktop->loader)) {
            desktop_lock(desktop, desktop->settings.auto_lock_with_pin);
        }
        return true;
    }

    return scene_manager_handle_custom_event(desktop->scene_manager, event);
}

static bool desktop_back_event_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = (Desktop*)context;
    return scene_manager_handle_back_event(desktop->scene_manager);
}

static void desktop_tick_event_callback(void* context) {
    furi_assert(context);
    Desktop* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void desktop_input_event_callback(const void* value, void* context) {
    furi_assert(value);
    furi_assert(context);
    const InputEvent* event = value;
    Desktop* desktop = context;
    if(event->type == InputTypePress) {
        desktop_start_auto_lock_timer(desktop);
    }
}

static void desktop_auto_lock_timer_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalAutoLock);
}

static void desktop_start_auto_lock_timer(Desktop* desktop) {
    furi_timer_start(
        desktop->auto_lock_timer, furi_ms_to_ticks(desktop->settings.auto_lock_delay_ms));
}

static void desktop_stop_auto_lock_timer(Desktop* desktop) {
    furi_timer_stop(desktop->auto_lock_timer);
}

static void desktop_auto_lock_arm(Desktop* desktop) {
    if(desktop->settings.auto_lock_delay_ms) {
        desktop->input_events_subscription = furi_pubsub_subscribe(
            desktop->input_events_pubsub, desktop_input_event_callback, desktop);
        desktop_start_auto_lock_timer(desktop);
    }
}

static void desktop_auto_lock_inhibit(Desktop* desktop) {
    desktop_stop_auto_lock_timer(desktop);
    if(desktop->input_events_subscription) {
        furi_pubsub_unsubscribe(desktop->input_events_pubsub, desktop->input_events_subscription);
        desktop->input_events_subscription = NULL;
    }
}

static void desktop_clock_timer_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = context;

    if(gui_get_count_of_enabled_view_port_in_layer(desktop->gui, GuiLayerStatusBarLeft) < 6) {
        desktop_clock_upd_time(desktop, false);

        view_port_enabled_set(desktop->clock_viewport, true);
    } else {
        view_port_enabled_set(desktop->clock_viewport, false);
    }
}

void desktop_lock(Desktop* desktop, bool pin_lock) {
    pin_lock = pin_lock && desktop_pin_is_valid(&desktop->settings.pin_code);
    if(!furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock)) {
        furi_hal_rtc_set_pin_fails(0);
    }
    if(pin_lock) {
        furi_hal_rtc_set_flag(FuriHalRtcFlagLock);
        Cli* cli = furi_record_open(RECORD_CLI);
        cli_session_close(cli);
        furi_record_close(RECORD_CLI);
    }

    desktop_auto_lock_inhibit(desktop);
    scene_manager_set_scene_state(
        desktop->scene_manager, DesktopSceneLocked, SCENE_LOCKED_FIRST_ENTER);
    scene_manager_next_scene(desktop->scene_manager, DesktopSceneLocked);

    DesktopStatus status = {.locked = true};
    furi_pubsub_publish(desktop->status_pubsub, &status);
}

void desktop_unlock(Desktop* desktop) {
    view_port_enabled_set(desktop->lock_icon_viewport, false);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_set_lockdown(gui, false);
    furi_record_close(RECORD_GUI);
    desktop_view_locked_unlock(desktop->locked_view);
    scene_manager_search_and_switch_to_previous_scene(desktop->scene_manager, DesktopSceneMain);
    desktop_auto_lock_arm(desktop);

    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock)) {
        furi_hal_rtc_reset_flag(FuriHalRtcFlagLock);
        furi_hal_rtc_set_pin_fails(0);
        Cli* cli = furi_record_open(RECORD_CLI);
        cli_session_open(cli, &cli_vcp);
        furi_record_close(RECORD_CLI);
    }

    DesktopStatus status = {.locked = false};
    furi_pubsub_publish(desktop->status_pubsub, &status);
}

void desktop_set_stealth_mode_state(Desktop* desktop, bool enabled) {
    desktop->in_transition = true;
    if(enabled) {
        furi_hal_rtc_set_flag(FuriHalRtcFlagStealthMode);
    } else {
        furi_hal_rtc_reset_flag(FuriHalRtcFlagStealthMode);
    }
    desktop_lock_menu_set_stealth_mode_state(desktop->lock_menu, enabled);
    view_port_enabled_set(desktop->stealth_mode_icon_viewport, enabled);
    desktop->in_transition = false;
}

Desktop* desktop_alloc() {
    Desktop* desktop = malloc(sizeof(Desktop));

    desktop->animation_manager = animation_manager_alloc();
    desktop->gui = furi_record_open(RECORD_GUI);
    desktop->scene_thread = furi_thread_alloc();
    desktop->view_dispatcher = view_dispatcher_alloc();
    desktop->scene_manager = scene_manager_alloc(&desktop_scene_handlers, desktop);

    view_dispatcher_enable_queue(desktop->view_dispatcher);
    view_dispatcher_attach_to_gui(
        desktop->view_dispatcher, desktop->gui, ViewDispatcherTypeDesktop);
    view_dispatcher_set_tick_event_callback(
        desktop->view_dispatcher, desktop_tick_event_callback, 500);

    view_dispatcher_set_event_callback_context(desktop->view_dispatcher, desktop);
    view_dispatcher_set_custom_event_callback(
        desktop->view_dispatcher, desktop_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        desktop->view_dispatcher, desktop_back_event_callback);

    desktop->lock_menu = desktop_lock_menu_alloc();
    desktop->debug_view = desktop_debug_alloc();
    desktop->hw_mismatch_popup = popup_alloc();
    desktop->locked_view = desktop_view_locked_alloc();
    desktop->pin_input_view = desktop_view_pin_input_alloc();
    desktop->pin_timeout_view = desktop_view_pin_timeout_alloc();
    desktop->slideshow_view = desktop_view_slideshow_alloc();

    desktop->main_view_stack = view_stack_alloc();
    desktop->main_view = desktop_main_alloc();
    View* dolphin_view = animation_manager_get_animation_view(desktop->animation_manager);
    view_stack_add_view(desktop->main_view_stack, desktop_main_get_view(desktop->main_view));
    view_stack_add_view(desktop->main_view_stack, dolphin_view);
    view_stack_add_view(
        desktop->main_view_stack, desktop_view_locked_get_view(desktop->locked_view));

    /* locked view (as animation view) attends in 2 scenes: main & locked,
     * because it has to draw "Unlocked" label on main scene */
    desktop->locked_view_stack = view_stack_alloc();
    view_stack_add_view(desktop->locked_view_stack, dolphin_view);
    view_stack_add_view(
        desktop->locked_view_stack, desktop_view_locked_get_view(desktop->locked_view));

    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdMain,
        view_stack_get_view(desktop->main_view_stack));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdLocked,
        view_stack_get_view(desktop->locked_view_stack));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdLockMenu,
        desktop_lock_menu_get_view(desktop->lock_menu));
    view_dispatcher_add_view(
        desktop->view_dispatcher, DesktopViewIdDebug, desktop_debug_get_view(desktop->debug_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdHwMismatch,
        popup_get_view(desktop->hw_mismatch_popup));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdPinTimeout,
        desktop_view_pin_timeout_get_view(desktop->pin_timeout_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdPinInput,
        desktop_view_pin_input_get_view(desktop->pin_input_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdSlideshow,
        desktop_view_slideshow_get_view(desktop->slideshow_view));

    // Lock icon
    desktop->lock_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->lock_icon_viewport, icon_get_width(&I_Lock_7x8));
    view_port_draw_callback_set(
        desktop->lock_icon_viewport, desktop_lock_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->lock_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->lock_icon_viewport, GuiLayerStatusBarLeft);

    // Clock
    desktop->clock_viewport = view_port_alloc();
    view_port_set_width(desktop->clock_viewport, 25);
    view_port_draw_callback_set(desktop->clock_viewport, desktop_clock_draw_callback, desktop);
    view_port_enabled_set(desktop->clock_viewport, false);
    gui_add_view_port(desktop->gui, desktop->clock_viewport, GuiLayerStatusBarRight);

    // Stealth mode icon
    desktop->stealth_mode_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->stealth_mode_icon_viewport, icon_get_width(&I_Muted_8x8));
    view_port_draw_callback_set(
        desktop->stealth_mode_icon_viewport, desktop_stealth_mode_icon_draw_callback, desktop);
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
        view_port_enabled_set(desktop->stealth_mode_icon_viewport, true);
    } else {
        view_port_enabled_set(desktop->stealth_mode_icon_viewport, false);
    }
    gui_add_view_port(desktop->gui, desktop->stealth_mode_icon_viewport, GuiLayerStatusBarLeft);

    // Special case: autostart application is already running
    desktop->loader = furi_record_open(RECORD_LOADER);
    if(loader_is_locked(desktop->loader) &&
       animation_manager_is_animation_loaded(desktop->animation_manager)) {
        animation_manager_unload_and_stall_animation(desktop->animation_manager);
    }

    desktop->notification = furi_record_open(RECORD_NOTIFICATION);
    desktop->app_start_stop_subscription = furi_pubsub_subscribe(
        loader_get_pubsub(desktop->loader), desktop_loader_callback, desktop);

    desktop->input_events_pubsub = furi_record_open(RECORD_INPUT_EVENTS);
    desktop->input_events_subscription = NULL;

    desktop->auto_lock_timer =
        furi_timer_alloc(desktop_auto_lock_timer_callback, FuriTimerTypeOnce, desktop);

    desktop->status_pubsub = furi_pubsub_alloc();

    desktop->update_clock_timer =
        furi_timer_alloc(desktop_clock_timer_callback, FuriTimerTypePeriodic, desktop);

    FuriHalRtcDateTime curr_dt;
    furi_hal_rtc_get_datetime(&curr_dt);

    desktop_clock_upd_time(desktop, true);

    furi_record_create(RECORD_DESKTOP, desktop);

    return desktop;
}

static bool desktop_check_file_flag(const char* flag_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool exists = storage_common_stat(storage, flag_path, NULL) == FSE_OK;
    furi_record_close(RECORD_STORAGE);

    return exists;
}

bool desktop_api_is_locked(Desktop* instance) {
    furi_assert(instance);
    return furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock);
}

void desktop_api_unlock(Desktop* instance) {
    furi_assert(instance);
    if(!furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock) || XTREME_SETTINGS()->pin_unlock_from_app) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopLockedEventUnlocked);
    }
}

FuriPubSub* desktop_api_get_status_pubsub(Desktop* instance) {
    furi_assert(instance);
    return instance->status_pubsub;
}

static const KeybindType keybind_types[] = {
    [InputTypeShort] = KeybindTypePress,
    [InputTypeLong] = KeybindTypeHold,
};

static const KeybindKey keybind_keys[] = {
    [InputKeyUp] = KeybindKeyUp,
    [InputKeyDown] = KeybindKeyDown,
    [InputKeyRight] = KeybindKeyRight,
    [InputKeyLeft] = KeybindKeyLeft,
};

void desktop_run_keybind(Desktop* instance, InputType _type, InputKey _key) {
    if(_type != InputTypeShort && _type != InputTypeLong) return;
    if(_key != InputKeyUp && _key != InputKeyDown && _key != InputKeyRight && _key != InputKeyLeft)
        return;

    KeybindType type = keybind_types[_type];
    KeybindKey key = keybind_keys[_key];
    const char* keybind = instance->keybinds[type][key].data;
    if(!strnlen(keybind, MAX_KEYBIND_LENGTH)) return;

    if(!strncmp(keybind, "Apps Menu", MAX_KEYBIND_LENGTH)) {
        loader_start_detached_with_gui_error(instance->loader, LOADER_APPLICATIONS_NAME, NULL);
    } else if(!strncmp(keybind, "Archive", MAX_KEYBIND_LENGTH)) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopMainEventOpenArchive);
    } else if(!strncmp(keybind, "Device Info", MAX_KEYBIND_LENGTH)) {
        loader_start_detached_with_gui_error(instance->loader, "Power", "about_battery");
    } else if(!strncmp(keybind, "Lock Menu", MAX_KEYBIND_LENGTH)) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopMainEventOpenLockMenu);
    } else if(!strncmp(keybind, "Lock Keypad", MAX_KEYBIND_LENGTH)) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopMainEventLockKeypad);
    } else if(!strncmp(keybind, "Lock with PIN", MAX_KEYBIND_LENGTH)) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopMainEventLockWithPin);
    } else {
        loader_start_detached_with_gui_error(instance->loader, keybind, NULL);
    }
}

int32_t desktop_srv(void* p) {
    UNUSED(p);

    if(!furi_hal_is_normal_boot()) {
        FURI_LOG_W(TAG, "Skipping start in special boot mode");
        return 0;
    }

    Desktop* desktop = desktop_alloc();

    bool ok = DESKTOP_SETTINGS_LOAD(&desktop->settings);
    if(ok && desktop->settings.pin_code.length) {
        ok = desktop_pin_is_valid(&desktop->settings.pin_code);
    }
    if(!ok) {
        memset(&desktop->settings, 0, sizeof(desktop->settings));
        furi_hal_rtc_reset_flag(FuriHalRtcFlagLock);
        furi_hal_rtc_set_pin_fails(0);
    }

    if(!DESKTOP_KEYBINDS_LOAD(&desktop->keybinds, sizeof(desktop->keybinds))) {
        memset(&desktop->keybinds, 0, sizeof(desktop->keybinds));
        strcpy(desktop->keybinds[KeybindTypePress][KeybindKeyUp].data, "Lock Menu");
        strcpy(desktop->keybinds[KeybindTypePress][KeybindKeyDown].data, "Archive");
        strcpy(desktop->keybinds[KeybindTypePress][KeybindKeyRight].data, "Passport");
        strcpy(
            desktop->keybinds[KeybindTypePress][KeybindKeyLeft].data,
            EXT_PATH("apps/Misc/nightstand.fap"));
        strcpy(desktop->keybinds[KeybindTypeHold][KeybindKeyRight].data, "Device Info");
        strcpy(desktop->keybinds[KeybindTypeHold][KeybindKeyLeft].data, "Lock with PIN");
    }

    desktop_clock_toggle_view(desktop, XTREME_SETTINGS()->statusbar_clock);

    scene_manager_next_scene(desktop->scene_manager, DesktopSceneMain);

    if(XTREME_SETTINGS()->lock_on_boot || furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock)) {
        desktop_lock(desktop, true);
    } else {
        if(!loader_is_locked(desktop->loader)) {
            desktop_auto_lock_arm(desktop);
        }
    }

    if(desktop_check_file_flag(SLIDESHOW_FS_PATH)) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneSlideshow);
    }

    if(!furi_hal_version_do_i_belong_here()) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneHwMismatch);
    }

    if(furi_hal_rtc_get_fault_data()) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneFault);
    }

    view_dispatcher_run(desktop->view_dispatcher);

    furi_crash("That was unexpected");

    return 0;
}
