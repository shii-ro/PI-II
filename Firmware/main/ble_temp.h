#pragma once
#include <stdbool.h>

typedef enum
{
    BLE_TEMP_OFF,
    BLE_TEMP_ADVERTISING,
    BLE_TEMP_CONNECTED,
} ble_temp_state_t;

void ble_temp_init(void (*relay_cb)(bool on),
                   void (*state_cb)(ble_temp_state_t state));
void ble_temp_update(float celsius);
bool ble_temp_is_connected(void);