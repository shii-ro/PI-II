#include "screen_main.h"
#include "ui.h"
#include "../hal/temp_sensor.h"
#include "../hal/wifi_manager.h"
#include <stdio.h>
#include <time.h>
#include <locale.h>

/* ---- Layout constants ----------------------------------------------- */
#define HEADER_HEIGHT        32
#define HEADER_PAD           8

/* ---- Screen widgets ---------------------------------------------------*/
static lv_obj_t *s_screen        = NULL;

/* Header */
static lv_obj_t *s_header        = NULL;
static lv_obj_t *s_datetime_label = NULL;
static lv_obj_t *s_wifi_label    = NULL;

/* Temperature dashboard */
static lv_obj_t *s_temp_label    = NULL;
static lv_obj_t *s_status_label  = NULL; /* "OK" / "Sensor error" */
static lv_obj_t *s_min_label     = NULL;
static lv_obj_t *s_max_label     = NULL;
static lv_obj_t *s_updated_label = NULL;

/* ---- Local state -------------------------------------------------------*/
static float s_min_temp = 0.0f;
static float s_max_temp = 0.0f;
static bool  s_have_reading = false;
static time_t s_last_update = 0;

/* ---- Helpers -------------------------------------------------------- */

static void update_datetime(void)
{
    time_t now = time(NULL);
    struct tm tm_now;

    localtime_s(&tm_now, &now);

    static const char *weekdays[] = {
        "dom", "seg", "ter", "qua",
        "qui", "sex", "sáb"
    };

    static const char *months[] = {
        "jan", "fev", "mar", "abr",
        "mai", "jun", "jul", "ago",
        "set", "out", "nov", "dez"
    };

    char buf[32];

    snprintf(buf, sizeof(buf),
             "%s %02d %s %02d:%02d",
             weekdays[tm_now.tm_wday],
             tm_now.tm_mday,
             months[tm_now.tm_mon],
             tm_now.tm_hour,
             tm_now.tm_min);

    lv_label_set_text(s_datetime_label, buf);
}

static void update_wifi_status(void)
{
    wifi_status_t status = wifi_manager_get_status();
    lv_color_t color;
    const char *text;

    switch (status)
    {
    case WIFI_STATUS_CONNECTED:
        text = LV_SYMBOL_WIFI " Connectado";
        color = lv_palette_main(LV_PALETTE_GREEN);
        break;
    case WIFI_STATUS_CONNECTING:
        text = LV_SYMBOL_WIFI " Connectando...";
        color = lv_palette_main(LV_PALETTE_ORANGE);
        break;
    case WIFI_STATUS_FAILED:
        text = LV_SYMBOL_WIFI " Falha";
        color = lv_palette_main(LV_PALETTE_RED);
        break;
    case WIFI_STATUS_DISCONNECTED:
    default:
        text = LV_SYMBOL_WIFI " Desconectado";
        color = lv_palette_main(LV_PALETTE_GREY);
        break;
    }

    lv_label_set_text(s_wifi_label, text);
    lv_obj_set_style_text_color(s_wifi_label, color, 0);
}

static void update_temperature(void)
{
    float temp_c;
    char buf[16];

    if (temp_sensor_read_celsius(&temp_c))
    {
        snprintf(buf, sizeof(buf), "%.1f\xC2\xB0 C", (double)temp_c);
        lv_label_set_text(s_temp_label, buf);
        lv_obj_set_style_text_color(s_temp_label, lv_color_white(), 0);

        if (!s_have_reading)
        {
            s_min_temp = temp_c;
            s_max_temp = temp_c;
            s_have_reading = true;
        }
        else
        {
            if (temp_c < s_min_temp)
                s_min_temp = temp_c;

            if (temp_c > s_max_temp)
                s_max_temp = temp_c;
        }

        s_last_update = time(NULL);

        lv_label_set_text(s_status_label, "Sensor OK");
        lv_obj_set_style_text_color(
            s_status_label,
            lv_palette_main(LV_PALETTE_GREEN),
            0
        );
    }
    else
    {
        lv_label_set_text(s_temp_label, "--.- C");
        lv_obj_set_style_text_color(
            s_temp_label,
            lv_palette_main(LV_PALETTE_GREY),
            0
        );

        lv_label_set_text(s_status_label, "Erro no Sensor");
        lv_obj_set_style_text_color(
            s_status_label,
            lv_palette_main(LV_PALETTE_RED),
            0
        );
    }

    /* Min / Max */
    if (s_have_reading)
    {
        snprintf(buf, sizeof(buf), "Min %.1f\xC2\xB0", (double)s_min_temp);
        lv_label_set_text(s_min_label, buf);

        snprintf(buf, sizeof(buf), "Max %.1f\xC2\xB0", (double)s_max_temp);
        lv_label_set_text(s_max_label, buf);
    }
    else
    {
        lv_label_set_text(s_min_label, "Min --.-");
        lv_label_set_text(s_max_label, "Max --.-");
    }

    /* Last updated */
    if (s_last_update != 0)
    {
        struct tm tm_upd;

        localtime_s(&tm_upd, &s_last_update);

        char tbuf[16];
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm_upd);

        snprintf(buf, sizeof(buf), "Atualizado %s", tbuf);
        lv_label_set_text(s_updated_label, buf);
    }
    else
    {
        lv_label_set_text(s_updated_label, "Atualizado --:--:--");
    }
}

/* ---- Event callbacks -------------------------------------------------*/

static void settings_btn_event_cb(lv_event_t *e)
{
    (void)e;
    ui_show_settings();
}

/* ---- Public API --------------------------------------------------------*/

lv_obj_t *screen_main_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    /* ---------------- Header bar ---------------- */
    s_header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_header);
    lv_obj_set_size(s_header, LV_PCT(100), HEADER_HEIGHT);
    lv_obj_align(s_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_header, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(s_header, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(s_header, HEADER_PAD, 0);
    lv_obj_set_style_border_width(s_header, 0, 0);
    lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    s_datetime_label = lv_label_create(s_header);
    lv_obj_set_style_text_font(s_datetime_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_datetime_label, lv_color_white(), 0);
    lv_label_set_text(s_datetime_label, "--- -- ---  --:--");
    lv_obj_align(s_datetime_label, LV_ALIGN_LEFT_MID, 0, 0);

    s_wifi_label = lv_label_create(s_header);
    lv_obj_set_style_text_font(s_wifi_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_wifi_label, LV_SYMBOL_WIFI " Disconnected");
    lv_obj_align(s_wifi_label, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Thin divider under the header */
    lv_obj_t *divider = lv_obj_create(s_screen);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, LV_PCT(100), 1);
    lv_obj_align_to(divider, s_header, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    /* ---------------- Temperature dashboard ---------------- */
    s_temp_label = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_temp_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_temp_label, lv_color_white(), 0);
    lv_label_set_text(s_temp_label, "--.- C");
    lv_obj_align(s_temp_label, LV_ALIGN_CENTER, 0, -30);

    s_status_label = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_status_label, "Sensor --");
    lv_obj_align_to(s_status_label, s_temp_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    /* Row: Min | Max | Updated */
    lv_obj_t *stats_row = lv_obj_create(s_screen);
    lv_obj_remove_style_all(stats_row);
    lv_obj_set_flex_flow(stats_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(stats_row, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_align_to(stats_row, s_status_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
    lv_obj_clear_flag(stats_row, LV_OBJ_FLAG_SCROLLABLE);

    s_min_label = lv_label_create(stats_row);
    lv_obj_set_style_text_font(s_min_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_min_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_label_set_text(s_min_label, "Min --.-");

    s_max_label = lv_label_create(stats_row);
    lv_obj_set_style_text_font(s_max_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_max_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_label_set_text(s_max_label, "Max --.-");

    s_updated_label = lv_label_create(stats_row);
    lv_obj_set_style_text_font(s_updated_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_updated_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_updated_label, "Atualizado --:--:--");

    /* ---------------- Settings button ---------------- */
    lv_obj_t *settings_btn = lv_btn_create(s_screen);
    lv_obj_set_size(settings_btn, 50, 50);
    lv_obj_align(settings_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(settings_btn, settings_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *gear_label = lv_label_create(settings_btn);
    lv_label_set_text(gear_label, LV_SYMBOL_SETTINGS);
    lv_obj_center(gear_label);

    /* Reset stat tracking whenever the screen is (re)created */
    s_have_reading = false;
    s_last_update = 0;

    /* Paint an initial frame immediately so it doesn't show placeholders */
    screen_main_update();

    return s_screen;
}

void screen_main_update(void)
{
    if (!s_screen)
        return;

    update_datetime();
    update_wifi_status();
    update_temperature();
}
