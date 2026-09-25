#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID_MAX_LEN 32
#define WIFI_SCAN_MAX_APS 16

typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED,
} wifi_status_t;

typedef struct {
    char    ssid[WIFI_SSID_MAX_LEN + 1];
    int8_t  rssi;      /* dBm */
    bool    secured;   /* false = open network */
} wifi_ap_info_t;

/*
 * Wi-Fi HAL. Mock today, esp_wifi-backed on the ESP32 later — same
 * interface, so screen_settings.c doesn't change.
 */
typedef struct {
    bool (*init)(void);

    /* Blocking-ish scan; fills out_list (max max_count), returns count found. */
    int (*scan)(wifi_ap_info_t *out_list, int max_count);

    /* Kicks off a (possibly async) connection attempt. */
    bool (*connect)(const char *ssid, const char *password);

    void (*disconnect)(void);

    wifi_status_t (*get_status)(void);

    /* Fills out_ip (e.g. "192.168.1.42"); returns false if not connected. */
    bool (*get_ip)(char *out_ip, size_t out_ip_len);
} wifi_driver_t;

void wifi_manager_init(const wifi_driver_t *driver);

int  wifi_manager_scan(wifi_ap_info_t *out_list, int max_count);
bool wifi_manager_connect(const char *ssid, const char *password);
void wifi_manager_disconnect(void);
wifi_status_t wifi_manager_get_status(void);
bool wifi_manager_get_ip(char *out_ip, size_t out_ip_len);

#ifdef __cplusplus
}
#endif
