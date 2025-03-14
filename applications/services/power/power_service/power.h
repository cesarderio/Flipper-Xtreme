#pragma once

#include <stdint.h>
#include <core/pubsub.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_POWER "power"

typedef struct Power Power;

typedef enum {
    PowerBootModeNormal,
    PowerBootModeDfu,
    PowerBootModeUpdateStart,
} PowerBootMode;

typedef enum {
    PowerEventTypeStopCharging,
    PowerEventTypeStartCharging,
    PowerEventTypeFullyCharged,
    PowerEventTypeBatteryLevelChanged,
} PowerEventType;

typedef enum {
    BatteryIconOff,
    BatteryIconBar,
    BatteryIconPercent,
    BatteryIconInvertedPercent,
    BatteryIconRetro3,
    BatteryIconRetro5,
    BatteryIconBarPercent,
    BatteryIconCount,
} BatteryIcon;

typedef union {
    uint8_t battery_level;
} PowerEventData;

typedef struct {
    PowerEventType type;
    PowerEventData data;
} PowerEvent;

typedef struct {
    bool gauge_is_ok;
    bool is_charging;
    bool is_charge_capped;

    float current_charger;
    float current_gauge;

    float voltage_battery_charge_limit;
    float voltage_charger;
    float voltage_gauge;
    float voltage_vbus;

    uint32_t capacity_remaining;
    uint32_t capacity_full;

    float temperature_charger;
    float temperature_gauge;

    uint8_t charge;
    uint8_t health;
} PowerInfo;

/** Power off device
 */
void power_off(Power* power);

/** Reboot device
 *
 * @param mode      PowerBootMode
 */
void power_reboot(PowerBootMode mode);

/** Get power info
 *
 * @param power     Power instance
 * @param info      PowerInfo instance
 */
void power_get_info(Power* power, PowerInfo* info);

/** Get power event pubsub handler
 *
 * @param power     Power instance
 *
 * @return          FuriPubSub instance
 */
FuriPubSub* power_get_pubsub(Power* power);

/** Get power settings events pubsub handler
 *
 * @param power     Power instance
 *
 * @return          FuriPubSub instance
 */
FuriPubSub* power_get_settings_events_pubsub(Power* power);

/** Check battery health
 *
 * @return          true if battery is healthy
 */
bool power_is_battery_healthy(Power* power);

/** Enable or disable battery low level notification message
 *
 * @param power     Power instance
 * @param enable    true - enable, false - disable
 */
void power_enable_low_battery_level_notification(Power* power, bool enable);

/** Trigger UI update for changing battery layout
 *
 * @param power     Power instance
 */
void power_trigger_ui_update(Power* power);

/** Enable or disable battery icon
 *
 * @param power         Power instance
 * @param is_enabled    Show battery or not
 */
void power_set_battery_icon_enabled(Power* power, bool is_enabled);

#ifdef __cplusplus
}
#endif
