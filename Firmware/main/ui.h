#pragma once

#include "ble_temp.h"   /* ble_temp_state_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Build the screen. Call with the LVGL lock held (lvgl_port_lock). */
void ui_create(void);

/* ---- Temperature (call with the LVGL lock held, e.g. from an lv_timer) ---- */
void ui_show_temperature(float celsius);
void ui_show_temp_waiting(void);   /* sensor has no reading yet */
void ui_show_temp_error(void);     /* sensor read failed */

/* ---- BLE status ---- */
/* Call with the LVGL lock held. */
void ui_set_ble_state(ble_temp_state_t state);

/* Takes the LVGL lock itself: safe to pass as the ble_temp_init() state
 * callback, since it runs on the NimBLE host task. */
void ui_set_ble_state_safe(ble_temp_state_t state);

#ifdef __cplusplus
}
#endif