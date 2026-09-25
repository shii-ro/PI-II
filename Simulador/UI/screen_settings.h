#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_settings_create(void);

/* Called from ui_tick() to refresh connection status/IP while this
 * screen is visible. */
void screen_settings_update(void);

#ifdef __cplusplus
}
#endif

#endif
