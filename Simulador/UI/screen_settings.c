#include "screen_settings.h"
#include "ui.h"
#include "../hal/wifi_manager.h"
#include <string.h>
#include <stdio.h>

/* Panel is 320x240. Layout constants kept explicit so it's easy to
 * re-tune if the panel size ever changes. */
#define HEADER_HEIGHT   28
#define CONNECT_VIEW_H  110   /* must stay clear of the on-screen keyboard */

static lv_obj_t *s_screen = NULL;

/* Header */
static lv_obj_t *s_header = NULL;

/* List view: shows nearby access points */
static lv_obj_t *s_list_view = NULL;
static lv_obj_t *s_ap_list = NULL;

/* Connect view: shown after an AP is tapped. Sits at the top of the
 * content area so it stays visible above the on-screen keyboard. */
static lv_obj_t *s_connect_view = NULL;
static lv_obj_t *s_ssid_label = NULL;
static lv_obj_t *s_password_ta = NULL;
static lv_obj_t *s_connect_btn = NULL;
static lv_obj_t *s_cancel_btn = NULL;
static lv_obj_t *s_status_label = NULL;

static lv_obj_t *s_keyboard = NULL;

static char s_selected_ssid[WIFI_SSID_MAX_LEN + 1] = {0};
static bool s_connecting = false;

/* ---- helpers ---- */

static void set_status(const char *text, lv_color_t color)
{
    lv_label_set_text(s_status_label, text);
    lv_obj_set_style_text_color(s_status_label, color, 0);
}

static void set_connect_btn_enabled(bool enabled)
{
    if (enabled)
        lv_obj_clear_state(s_connect_btn, LV_STATE_DISABLED);
    else
        lv_obj_add_state(s_connect_btn, LV_STATE_DISABLED);
}

static void show_list_view(void)
{
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_keyboard, NULL);

    lv_obj_add_flag(s_connect_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_list_view, LV_OBJ_FLAG_HIDDEN);
}

static void show_connect_view(const char *ssid)
{
    strncpy(s_selected_ssid, ssid, WIFI_SSID_MAX_LEN);
    s_selected_ssid[WIFI_SSID_MAX_LEN] = '\0';

    char buf[48];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s", s_selected_ssid);
    lv_label_set_text(s_ssid_label, buf);

    lv_textarea_set_text(s_password_ta, "");
    set_status("", lv_color_white());
    set_connect_btn_enabled(true);
    s_connecting = false;

    lv_obj_add_flag(s_list_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_connect_view, LV_OBJ_FLAG_HIDDEN);
}

/* ---- event callbacks ---- */

static void back_btn_event_cb(lv_event_t *e)
{
    (void)e;
    ui_show_main();
}

static void cancel_btn_event_cb(lv_event_t *e)
{
    (void)e;
    show_list_view();
}

static void ap_item_free_user_data_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    void *data = lv_obj_get_user_data(btn);
    if (data)
    {
        lv_free(data);
        lv_obj_set_user_data(btn, NULL);
    }
}

static void ap_item_clicked_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *ssid = (const char *)lv_obj_get_user_data(btn);
    if (!ssid)
        return;

    show_connect_view(ssid);
}

static void password_ta_focused_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_keyboard, s_password_ta);
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
    {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(s_keyboard, NULL);
    }
}

static void connect_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_selected_ssid[0] == '\0' || s_connecting)
        return;

    const char *password = lv_textarea_get_text(s_password_ta);
    wifi_manager_connect(s_selected_ssid, password);

    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_keyboard, NULL);

    s_connecting = true;
    set_connect_btn_enabled(false);
    set_status("Conectando...", lv_palette_main(LV_PALETTE_ORANGE));
}

static void scan_btn_event_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_clean(s_ap_list);

    wifi_ap_info_t aps[WIFI_SCAN_MAX_APS];
    int count = wifi_manager_scan(aps, WIFI_SCAN_MAX_APS);

    if (count == 0)
    {
        lv_list_add_text(s_ap_list, "Nenhuma rede encontrada");
        return;
    }

    for (int i = 0; i < count; i++)
    {
        const char *icon = aps[i].secured ? LV_SYMBOL_WIFI LV_SYMBOL_CLOSE
                                          : LV_SYMBOL_WIFI;
        lv_obj_t *btn = lv_list_add_btn(s_ap_list, icon, aps[i].ssid);

        /* Stash a heap copy of the ssid on the button so the click
         * handler knows which network was tapped, and free it again
         * when the button is destroyed (e.g. next scan clears the list). */
        char *ssid_copy = lv_malloc(WIFI_SSID_MAX_LEN + 1);
        strncpy(ssid_copy, aps[i].ssid, WIFI_SSID_MAX_LEN);
        ssid_copy[WIFI_SSID_MAX_LEN] = '\0';
        lv_obj_set_user_data(btn, ssid_copy);

        lv_obj_add_event_cb(btn, ap_item_clicked_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, ap_item_free_user_data_cb, LV_EVENT_DELETE, NULL);
    }
}

/* ---- construction ---- */

lv_obj_t *screen_settings_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ---------------- Header: back | title | scan (icon-only, compact) ---------------- */
    s_header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_header);
    lv_obj_set_size(s_header, LV_PCT(100), HEADER_HEIGHT);
    lv_obj_align(s_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_header, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(s_header, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(s_header, 6, 0);
    lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_btn = lv_btn_create(s_header);
    lv_obj_remove_style_all(back_btn);
    lv_obj_set_size(back_btn, 28, 22);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(s_header);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Wi-Fi");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *scan_btn = lv_btn_create(s_header);
    lv_obj_remove_style_all(scan_btn);
    lv_obj_set_size(scan_btn, 28, 22);
    lv_obj_align(scan_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(scan_btn, scan_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, LV_SYMBOL_REFRESH);
    lv_obj_center(scan_label);

    /* ---------------- List view: fills the remaining space ---------------- */
    s_list_view = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_list_view);
    lv_obj_set_size(s_list_view, LV_PCT(100), 240 - HEADER_HEIGHT);
    lv_obj_align_to(s_list_view, s_header, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(s_list_view, LV_OBJ_FLAG_SCROLLABLE);

    s_ap_list = lv_list_create(s_list_view);
    lv_obj_set_size(s_ap_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(s_ap_list, 0, 0);

    /* ---------------- Connect view: compact, pinned to the top so the
     * on-screen keyboard (which opens from the bottom) never covers it. ---------------- */
    s_connect_view = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_connect_view);
    lv_obj_set_size(s_connect_view, LV_PCT(100), CONNECT_VIEW_H);
    lv_obj_align_to(s_connect_view, s_header, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(s_connect_view, 12, 0);
    lv_obj_set_flex_flow(s_connect_view, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_connect_view, 8, 0);
    lv_obj_clear_flag(s_connect_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_connect_view, LV_OBJ_FLAG_HIDDEN);

    s_ssid_label = lv_label_create(s_connect_view);
    lv_obj_set_style_text_font(s_ssid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_ssid_label, lv_color_white(), 0);
    lv_label_set_text(s_ssid_label, "");

    /* Password field + Connect button share a row to save vertical space */
    lv_obj_t *entry_row = lv_obj_create(s_connect_view);
    lv_obj_remove_style_all(entry_row);
    lv_obj_set_size(entry_row, LV_PCT(100), 36);
    lv_obj_set_flex_flow(entry_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(entry_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(entry_row, LV_OBJ_FLAG_SCROLLABLE);

    s_password_ta = lv_textarea_create(entry_row);
    lv_obj_set_size(s_password_ta, 180, 36);
    lv_textarea_set_one_line(s_password_ta, true);
    lv_textarea_set_password_mode(s_password_ta, true);
    lv_textarea_set_placeholder_text(s_password_ta, "Senha");
    lv_obj_add_event_cb(s_password_ta, password_ta_focused_cb, LV_EVENT_FOCUSED, NULL);

    s_connect_btn = lv_btn_create(entry_row);
    lv_obj_set_size(s_connect_btn, 90, 36);
    lv_obj_add_event_cb(s_connect_btn, connect_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *connect_label = lv_label_create(s_connect_btn);
    lv_label_set_text(connect_label, "Conectar");
    lv_obj_center(connect_label);

    /* Status line + Cancel link share a row too */
    lv_obj_t *bottom_row = lv_obj_create(s_connect_view);
    lv_obj_remove_style_all(bottom_row);
    lv_obj_set_size(bottom_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bottom_row, LV_OBJ_FLAG_SCROLLABLE);

    s_status_label = lv_label_create(bottom_row);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_status_label, "");

    s_cancel_btn = lv_btn_create(bottom_row);
    lv_obj_remove_style_all(s_cancel_btn);
    lv_obj_add_event_cb(s_cancel_btn, cancel_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_label = lv_label_create(s_cancel_btn);
    lv_obj_set_style_text_color(cancel_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(cancel_label, "Cancelar");

    /* ---------------- Keyboard, hidden until the password field is focused ---------------- */
    s_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_keyboard, keyboard_event_cb, LV_EVENT_CANCEL, NULL);

    s_selected_ssid[0] = '\0';
    s_connecting = false;

    /* Scan right away so the list isn't empty on first open */
    scan_btn_event_cb(NULL);

    return s_screen;
}

void screen_settings_update(void)
{
    if (!s_screen)
        return;

    switch (wifi_manager_get_status())
    {
    case WIFI_STATUS_CONNECTED:
    {
        char ip[16] = {0};
        wifi_manager_get_ip(ip, sizeof(ip));
        char buf[48];
        snprintf(buf, sizeof(buf), "Conectado - %s", ip);
        set_status(buf, lv_palette_main(LV_PALETTE_GREEN));
        s_connecting = false;
        set_connect_btn_enabled(true);
        break;
    }
    case WIFI_STATUS_CONNECTING:
        set_status("Conectando...", lv_palette_main(LV_PALETTE_ORANGE));
        break;
    case WIFI_STATUS_FAILED:
        set_status("Falha na conex\xC3\xA3o", lv_palette_main(LV_PALETTE_RED));
        s_connecting = false;
        set_connect_btn_enabled(true);
        break;
    case WIFI_STATUS_DISCONNECTED:
    default:
        break;
    }
}
