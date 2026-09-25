#include "wifi_manager.h"
#include <string.h>

static const wifi_driver_t *s_driver = NULL;

void wifi_manager_init(const wifi_driver_t *driver)
{
    s_driver = driver;
    if (s_driver && s_driver->init) {
        s_driver->init();
    }
}

int wifi_manager_scan(wifi_ap_info_t *out_list, int max_count)
{
    if (!s_driver || !s_driver->scan) return 0;
    return s_driver->scan(out_list, max_count);
}

bool wifi_manager_connect(const char *ssid, const char *password)
{
    if (!s_driver || !s_driver->connect) return false;
    return s_driver->connect(ssid, password);
}

void wifi_manager_disconnect(void)
{
    if (s_driver && s_driver->disconnect) {
        s_driver->disconnect();
    }
}

wifi_status_t wifi_manager_get_status(void)
{
    if (!s_driver || !s_driver->get_status) return WIFI_STATUS_DISCONNECTED;
    return s_driver->get_status();
}

bool wifi_manager_get_ip(char *out_ip, size_t out_ip_len)
{
    if (!s_driver || !s_driver->get_ip) return false;
    return s_driver->get_ip(out_ip, out_ip_len);
}
