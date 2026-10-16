#include <math.h>
#include <stdlib.h>
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui.h"

/* Requires in menuconfig (Component config -> LVGL -> Font usage):
 * LV_FONT_MONTSERRAT_12, _14, _20, _48 */

#define COL_BG        lv_color_hex(0x0B0F14)
#define COL_CARD      lv_color_hex(0x161C24)
#define COL_TEXT      lv_color_hex(0xF2F5F8)
#define COL_MUTED     lv_color_hex(0x7A8794)
#define COL_BLUE      lv_color_hex(0x3B9EFF)
#define COL_GREEN     lv_color_hex(0x2ED573)
#define COL_ORANGE    lv_color_hex(0xFFA502)
#define COL_RED       lv_color_hex(0xFF4757)

/* Gauge range, in tenths of a degree C */
#define GAUGE_MIN_T10   (-100)   /* -10.0 C */
#define GAUGE_MAX_T10   (500)    /*  50.0 C */

static lv_obj_t *s_arc;
static lv_obj_t *s_temp_lbl;
static lv_obj_t *s_unit_lbl;
static lv_obj_t *s_ble_pill;
static lv_obj_t *s_ble_icon;
static lv_obj_t *s_ble_text;
static lv_obj_t *s_min_lbl;
static lv_obj_t *s_max_lbl;

static float s_min =  1000.0f;
static float s_max = -1000.0f;

/* ---------- helpers ---------- */

static lv_color_t temp_color(float t)
{
    if (t < 15.0f) return COL_BLUE;
    if (t < 28.0f) return COL_GREEN;
    if (t < 38.0f) return COL_ORANGE;
    return COL_RED;
}

/* LVGL's printf has no float support by default, so format with integers. */
static void fmt_temp(char *buf, size_t len, const char *prefix, float t)
{
    int t10 = (int)lroundf(t * 10.0f);
    lv_snprintf(buf, len, "%s%s%d.%d", prefix, t10 < 0 ? "-" : "",
                abs(t10) / 10, abs(t10) % 10);
}

static void opa_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

/* ---------- BLE status ---------- */

void ui_set_ble_state(ble_temp_state_t state)
{
    if (!s_ble_pill) return;   /* UI not built yet */

    lv_color_t c;
    const char *txt;

    lv_anim_delete(s_ble_icon, opa_anim_cb);
    lv_obj_set_style_opa(s_ble_icon, LV_OPA_COVER, 0);

    switch (state) {
    case BLE_TEMP_CONNECTED:
        c = COL_GREEN;
        txt = "Connected";
        break;

    case BLE_TEMP_ADVERTISING: {
        c = COL_BLUE;
        txt = "Advertising";
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_ble_icon);
        lv_anim_set_exec_cb(&a, opa_anim_cb);
        lv_anim_set_values(&a, 255, 70);
        lv_anim_set_duration(&a, 800);
        lv_anim_set_reverse_duration(&a, 800);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
        break;
    }

    case BLE_TEMP_OFF:
    default:
        c = COL_MUTED;
        txt = "BLE off";
        break;
    }

    lv_obj_set_style_border_color(s_ble_pill, c, 0);
    lv_obj_set_style_text_color(s_ble_icon, c, 0);
    lv_obj_set_style_text_color(s_ble_text, c, 0);
    lv_label_set_text(s_ble_text, txt);
}

void ui_set_ble_state_safe(ble_temp_state_t state)
{
    if (lvgl_port_lock(100)) {
        ui_set_ble_state(state);
        lvgl_port_unlock();
    }
}

/* ---------- Temperature ---------- */

void ui_show_temp_waiting(void)
{
    lv_label_set_text(s_temp_lbl, "--.-");
    lv_obj_set_style_text_color(s_temp_lbl, COL_MUTED, 0);
    lv_obj_set_style_arc_color(s_arc, COL_MUTED, LV_PART_INDICATOR);
}

void ui_show_temp_error(void)
{
    lv_label_set_text(s_temp_lbl, "ERR");
    lv_obj_set_style_text_color(s_temp_lbl, COL_RED, 0);
    lv_obj_set_style_arc_color(s_arc, COL_RED, LV_PART_INDICATOR);
}

void ui_show_temperature(float celsius)
{
    char buf[24];
    lv_color_t col = temp_color(celsius);

    fmt_temp(buf, sizeof(buf), "", celsius);
    lv_label_set_text(s_temp_lbl, buf);
    lv_obj_set_style_text_color(s_temp_lbl, col, 0);
    lv_obj_set_style_arc_color(s_arc, col, LV_PART_INDICATOR);
    lv_arc_set_value(s_arc, (int32_t)lroundf(celsius * 10.0f));

    if (celsius < s_min) s_min = celsius;
    if (celsius > s_max) s_max = celsius;

    fmt_temp(buf, sizeof(buf), "MIN ", s_min);
    lv_label_set_text(s_min_lbl, buf);
    fmt_temp(buf, sizeof(buf), "MAX ", s_max);
    lv_label_set_text(s_max_lbl, buf);
}

/* ---------- Build the screen ---------- */

void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "TEMPERATURE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, COL_MUTED, 0);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 14);

    /* BLE pill */
    s_ble_pill = lv_obj_create(scr);
    lv_obj_set_size(s_ble_pill, LV_SIZE_CONTENT, 26);
    lv_obj_set_style_radius(s_ble_pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ble_pill, COL_CARD, 0);
    lv_obj_set_style_bg_opa(s_ble_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ble_pill, 1, 0);
    lv_obj_set_style_pad_hor(s_ble_pill, 10, 0);
    lv_obj_set_style_pad_ver(s_ble_pill, 0, 0);
    lv_obj_set_style_pad_column(s_ble_pill, 6, 0);
    lv_obj_set_flex_flow(s_ble_pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_ble_pill, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(s_ble_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_ble_pill, LV_ALIGN_TOP_RIGHT, -10, 8);

    s_ble_icon = lv_label_create(s_ble_pill);
    lv_label_set_text(s_ble_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(s_ble_icon, &lv_font_montserrat_14, 0);

    s_ble_text = lv_label_create(s_ble_pill);
    lv_obj_set_style_text_font(s_ble_text, &lv_font_montserrat_12, 0);

    /* Gauge arc */
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 190, 190);
    lv_obj_align(s_arc, LV_ALIGN_CENTER, 0, 14);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, GAUGE_MIN_T10, GAUGE_MAX_T10);
    lv_arc_set_value(s_arc, GAUGE_MIN_T10);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, COL_MUTED, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);

    /* Big number */
    s_temp_lbl = lv_label_create(scr);
    lv_label_set_text(s_temp_lbl, "--.-");
    lv_obj_set_style_text_font(s_temp_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_temp_lbl, COL_TEXT, 0);
    lv_obj_align(s_temp_lbl, LV_ALIGN_CENTER, 0, 4);

    /* Unit */
    s_unit_lbl = lv_label_create(scr);
    lv_label_set_text(s_unit_lbl, "\xC2\xB0""C");
    lv_obj_set_style_text_font(s_unit_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_unit_lbl, COL_MUTED, 0);
    lv_obj_align_to(s_unit_lbl, s_temp_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    /* Min / Max */
    s_min_lbl = lv_label_create(scr);
    lv_label_set_text(s_min_lbl, "MIN --");
    lv_obj_set_style_text_font(s_min_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_min_lbl, COL_BLUE, 0);
    lv_obj_align(s_min_lbl, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    s_max_lbl = lv_label_create(scr);
    lv_label_set_text(s_max_lbl, "MAX --");
    lv_obj_set_style_text_font(s_max_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_max_lbl, COL_ORANGE, 0);
    lv_obj_align(s_max_lbl, LV_ALIGN_BOTTOM_RIGHT, -14, -10);

    ui_set_ble_state(BLE_TEMP_OFF);
}