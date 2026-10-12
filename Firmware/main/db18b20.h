#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DS18B20_RES_9BIT  = 9,   /* ~94 ms,  0.5    C */
    DS18B20_RES_10BIT = 10,  /* ~188 ms, 0.25   C */
    DS18B20_RES_11BIT = 11,  /* ~375 ms, 0.125  C */
    DS18B20_RES_12BIT = 12,  /* ~750 ms, 0.0625 C */
} ds18b20_resolution_t;

typedef struct ds18b20_dev *ds18b20_handle_t;

/**
 * Called from the driver's own task after every measurement attempt.
 * On success err == ESP_OK and temp_c is valid.
 * NOTE: this is NOT the LVGL thread. Do not touch LVGL objects from here
 * without locking (see main.c for the recommended polling approach).
 */
typedef void (*ds18b20_cb_t)(float temp_c, esp_err_t err, void *user_ctx);

typedef struct {
    gpio_num_t           pin;
    ds18b20_resolution_t resolution;
    uint32_t             period_ms;   /* 0 = only measure on ds18b20_trigger() */
    ds18b20_cb_t         cb;          /* optional */
    void                *user_ctx;
    uint32_t             stack_size;
    UBaseType_t          priority;
    BaseType_t           core_id;     /* tskNO_AFFINITY or a core number */
} ds18b20_config_t;

#define DS18B20_CONFIG_DEFAULT(pin_) {          \
    .pin        = (pin_),                       \
    .resolution = DS18B20_RES_12BIT,            \
    .period_ms  = 1000,                         \
    .cb         = NULL,                         \
    .user_ctx   = NULL,                         \
    .stack_size = 3072,                         \
    .priority   = 5,                            \
    .core_id    = tskNO_AFFINITY,               \
}

/** Create the driver and start its background task. */
esp_err_t ds18b20_create(const ds18b20_config_t *cfg, ds18b20_handle_t *out);

/** Request an immediate measurement (non-blocking, safe from any task). */
esp_err_t ds18b20_trigger(ds18b20_handle_t h);

/**
 * Thread-safe, non-blocking read of the most recent result.
 * @param temp_c  last good temperature (untouched if none yet)
 * @param age_ms  age of that reading (may be NULL)
 * @return ESP_OK if the latest attempt succeeded, ESP_ERR_NOT_FINISHED if no
 *         measurement has completed yet, otherwise the last error
 *         (ESP_ERR_NOT_FOUND = no sensor, ESP_ERR_INVALID_CRC, ...).
 */
esp_err_t ds18b20_get_latest(ds18b20_handle_t h, float *temp_c, uint32_t *age_ms);

/** Stop the task and free resources. */
esp_err_t ds18b20_delete(ds18b20_handle_t h);

#ifdef __cplusplus
}
#endif