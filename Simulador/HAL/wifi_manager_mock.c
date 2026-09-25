#include "wifi_manager.h"
#include <string.h>
#include <stdio.h>

/*
 * Fakes a small set of nearby access points and a connection attempt
 * that takes a few status polls to "complete" (like real Wi-Fi does),
 * so the settings screen's spinner/state logic gets exercised before
 * you ever touch esp_wifi.
 */

static const wifi_ap_info_t s_fake_aps[] = {
    { "HomeNetwork_5G",  -42, true  },
    { "HomeNetwork_2G",  -55, true  },
    { "Office-Guest",    -61, false },
    { "Neighbor_WiFi",   -78, true  },
};
#define FAKE_AP_COUNT (sizeof(s_fake_aps) / sizeof(s_fake_aps[0]))

static wifi_status_t s_status = WIFI_STATUS_DISCONNECTED;
static char           s_connecting_ssid[WIFI_SSID_MAX_LEN + 1] = {0};
static int            s_connect_polls_left = 0;

static bool mock_init(void)
{
    s_status = WIFI_STATUS_DISCONNECTED;
    return true;
}

static int mock_scan(wifi_ap_info_t *out_list, int max_count)
{
    int count = (int)FAKE_AP_COUNT;
    if (count > max_count) count = max_count;
    memcpy(out_list, s_fake_aps, count * sizeof(wifi_ap_info_t));
    return count;
}

static bool mock_connect(const char *ssid, const char *password)
{
    (void)password; /* mock doesn't validate credentials */
    if (!ssid || ssid[0] == '\0') return false;

    strncpy(s_connecting_ssid, ssid, WIFI_SSID_MAX_LEN);
    s_connecting_ssid[WIFI_SSID_MAX_LEN] = '\0';
    s_status = WIFI_STATUS_CONNECTING;
    s_connect_polls_left = 3; /* pretend it takes ~3 status checks */
    return true;
}

static void mock_disconnect(void)
{
    s_status = WIFI_STATUS_DISCONNECTED;
    s_connecting_ssid[0] = '\0';
}

static wifi_status_t mock_get_status(void)
{
    if (s_status == WIFI_STATUS_CONNECTING) {
        if (--s_connect_polls_left <= 0) {
            s_status = WIFI_STATUS_CONNECTED;
        }
    }
    return s_status;
}

static bool mock_get_ip(char *out_ip, size_t out_ip_len)
{
    if (s_status != WIFI_STATUS_CONNECTED) return false;
    snprintf(out_ip, out_ip_len, "192.168.1.42");
    return true;
}

const wifi_driver_t wifi_driver_mock = {
    .init         = mock_init,
    .scan         = mock_scan,
    .connect      = mock_connect,
    .disconnect   = mock_disconnect,
    .get_status   = mock_get_status,
    .get_ip       = mock_get_ip,
};
