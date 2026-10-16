#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "ble_temp.h"

static const char *TAG = "ble_temp";

/* PLACEHOLDERS: replace with the UUIDs from ESP32FirmwareTemplate.
 * NimBLE wants the bytes REVERSED: "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
 * becomes 0x4b,0x91,0x31,0xc3,0xc9,0xc5,0xcc,0x8f,0x9e,0x45,0xb5,0x1f,0x01,0xc2,0xaf,0x4f */
static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(
    0x4b,0x91,0x31,0xc3,0xc9,0xc5,0xcc,0x8f,0x9e,0x45,0xb5,0x1f,0x01,0xc2,0xaf,0x4f);
static const ble_uuid128_t TEMP_UUID = BLE_UUID128_INIT(
    0xa8,0x26,0x1b,0x36,0x07,0xea,0xf5,0xb7,0x88,0x46,0xe1,0x36,0x3e,0x48,0xb5,0xbe);
static const ble_uuid128_t RELAY_UUID = BLE_UUID128_INIT(
    0x18,0x75,0xdd,0x80,0x9f,0x18,0x3f,0xab,0xe3,0x4b,0x4c,0x34,0x66,0xd4,0xa1,0xcb);

static uint8_t  s_own_addr_type;
static uint16_t s_temp_val_handle;
static char     s_json[24] = "{\"t\":0.00}";   /* keep <= 20 bytes */
static size_t   s_json_len = 10;
static volatile bool s_connected = false;
static void (*s_relay_cb)(bool) = NULL;

static void start_adv(void);

static int temp_access_cb(uint16_t conn, uint16_t attr,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return os_mbuf_append(ctxt->om, s_json, s_json_len) == 0
               ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int relay_access_cb(uint16_t conn, uint16_t attr,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (OS_MBUF_PKTLEN(ctxt->om) < 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        uint8_t b = 0;
        os_mbuf_copydata(ctxt->om, 0, 1, &b);
        ESP_LOGI(TAG, "relay write: %c", b);
        if (s_relay_cb) s_relay_cb(b == '1');
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void (*s_state_cb)(ble_temp_state_t) = NULL;

static void set_state(ble_temp_state_t st)
{
    if (s_state_cb) s_state_cb(st);
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &TEMP_UUID.u,
                .access_cb  = temp_access_cb,
                .val_handle = &s_temp_val_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid       = &RELAY_UUID.u,
                .access_cb  = relay_access_cb,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }
        },
    },
    { 0 }
};

static int gap_event(struct ble_gap_event *ev, void *arg)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status == 0) {
            s_connected = true;
            ESP_LOGI(TAG, "connected");
            set_state(BLE_TEMP_CONNECTED);
        } else {
            start_adv();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_connected = false;
        ESP_LOGI(TAG, "disconnected");
        start_adv();            /* sets ADVERTISING again */
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_adv();
        break;
    }
    return 0;
}

static void start_adv(void)
{
    /* Adv packet: flags + name (the app lists devices by name). */
    const char *name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields f = {0};
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.name = (uint8_t *)name;
    f.name_len = strlen(name);
    f.name_is_complete = 1;
    ble_gap_adv_set_fields(&f);

    /* Scan response: the 128-bit service UUID (doesn't fit in the adv packet). */
    struct ble_hs_adv_fields rsp = {0};
    rsp.uuids128 = (ble_uuid128_t *)&SVC_UUID;
    rsp.num_uuids128 = 1;
    rsp.uuids128_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params p = {0};
    p.conn_mode = BLE_GAP_CONN_MODE_UND;
    p.disc_mode = BLE_GAP_DISC_MODE_GEN;

    int rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &p, gap_event, NULL);
    if (rc == 0) {
        set_state(BLE_TEMP_ADVERTISING);
    } else {
        ESP_LOGE(TAG, "adv start failed: %d", rc);
        set_state(BLE_TEMP_OFF);
    }
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &s_own_addr_type);
    start_adv();
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}


void ble_temp_init(void (*relay_cb)(bool on),
                   void (*state_cb)(ble_temp_state_t state))
{
    s_relay_cb = relay_cb;
    s_state_cb = state_cb;

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_svc_gap_device_name_set("TermoSense");

    nimble_port_freertos_init(host_task);
}

void ble_temp_update(float celsius)
{
    int c = (int)lroundf(celsius * 100.0f);
    int a = abs(c);
    s_json_len = snprintf(s_json, sizeof(s_json), "{\"t\":%s%d.%02d}",
                          c < 0 ? "-" : "", a / 100, a % 100);
    ble_gatts_chr_updated(s_temp_val_handle);   /* notifies the app if subscribed */
}

bool ble_temp_is_connected(void) { return s_connected; }