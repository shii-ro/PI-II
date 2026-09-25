#include "ui.h"
#include "screen_main.h"
#include "screen_settings.h"
#include "lvgl.h"

static lv_obj_t *s_screen_main     = NULL;
static lv_obj_t *s_screen_settings = NULL;

void ui_init(void)
{
    s_screen_main     = screen_main_create();
    s_screen_settings = screen_settings_create();
    lv_scr_load(s_screen_main);
}

void ui_show_main(void)
{
    lv_scr_load(s_screen_main);
}

void ui_show_settings(void)
{
    lv_scr_load(s_screen_settings);
}

void ui_tick(void)
{
    screen_main_update();
    screen_settings_update();
}
