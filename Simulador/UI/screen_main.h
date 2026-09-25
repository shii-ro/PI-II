#ifndef SCREEN_MAIN_H
#define SCREEN_MAIN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_main_create(void);
void screen_main_update(void);

#ifdef __cplusplus
}

#endif
#endif
