#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "db18b20.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

static const char *TAG = "ds18b20";

#define CMD_SKIP_ROM      0xCC
#define CMD_CONVERT_T     0x44
#define CMD_WRITE_SCRATCH 0x4E
#define CMD_READ_SCRATCH  0xBE

struct ds18b20_dev 
{
    gpio_num_t           pin;
    ds18b20_resolution_t res;
    uint32_t             period_ms;
    ds18b20_cb_t         cb;
    void                *user_ctx;
    TaskHandle_t         task;
    volatile bool        quit;

    portMUX_TYPE         lock;
    float                last_temp;
    bool                 have_temp;
    int64_t              last_ts_us;
    esp_err_t            last_err;
};

/* ------------------------------------------------------------------ */
/* 1-Wire low level                                                    */
/* ------------------------------------------------------------------ */
static portMUX_TYPE s_ow_mux = portMUX_INITIALIZER_UNLOCKED;

static inline void ow_low(gpio_num_t p)     { gpio_set_level(p, 0); }
static inline void ow_release(gpio_num_t p) { gpio_set_level(p, 1); }
static inline int  ow_read_pin(gpio_num_t p){ return gpio_get_level(p); }

static esp_err_t ow_reset(gpio_num_t p)
{
    if (!ow_read_pin(p)) {
        return ESP_ERR_INVALID_STATE;   /* bus stuck low: missing pull-up / short */
    }
    ow_low(p);
    esp_rom_delay_us(480);

    int presence;
    portENTER_CRITICAL(&s_ow_mux);
    ow_release(p);
    esp_rom_delay_us(70);
    presence = !ow_read_pin(p);         /* sensor pulls the line low */
    portEXIT_CRITICAL(&s_ow_mux);

    esp_rom_delay_us(410);
    return presence ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static void ow_write_bit(gpio_num_t p, int bit)
{
    portENTER_CRITICAL(&s_ow_mux);
    ow_low(p);
    if (bit) {
        esp_rom_delay_us(6);
        ow_release(p);
        esp_rom_delay_us(64);
    } else {
        esp_rom_delay_us(60);
        ow_release(p);
        esp_rom_delay_us(10);
    }
    portEXIT_CRITICAL(&s_ow_mux);
}

static int ow_read_bit(gpio_num_t p)
{
    int v;
    portENTER_CRITICAL(&s_ow_mux);
    ow_low(p);
    esp_rom_delay_us(3);
    ow_release(p);
    esp_rom_delay_us(10);
    v = ow_read_pin(p);
    esp_rom_delay_us(53);
    portEXIT_CRITICAL(&s_ow_mux);
    return v;
}

static void ow_write_byte(gpio_num_t p, uint8_t b)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(p, b & 1);
        b >>= 1;
    }
}

static uint8_t ow_read_byte(gpio_num_t p)
{
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
        b |= (uint8_t)(ow_read_bit(p) << i);
    }
    return b;
}

static uint8_t crc8(const uint8_t *d, size_t len)
{
    uint8_t crc = 0;
    while (len--) {
        uint8_t in = *d++;
        for (int i = 0; i < 8; i++) {
            uint8_t mix = (crc ^ in) & 1;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            in >>= 1;
        }
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/* DS18B20 operations                                                  */
/* ------------------------------------------------------------------ */
static uint32_t conv_time_ms(ds18b20_resolution_t r)
{
    switch (r) {
    case DS18B20_RES_9BIT:  return 94;
    case DS18B20_RES_10BIT: return 188;
    case DS18B20_RES_11BIT: return 375;
    default:                return 750;
    }
}

static esp_err_t write_resolution(struct ds18b20_dev *d)
{
    esp_err_t err = ow_reset(d->pin);
    if (err != ESP_OK) return err;

    uint8_t cfg = (uint8_t)(((d->res - 9) << 5) | 0x1F);
    ow_write_byte(d->pin, CMD_SKIP_ROM);
    ow_write_byte(d->pin, CMD_WRITE_SCRATCH);
    ow_write_byte(d->pin, 0x7F);   /* TH (unused alarm) */
    ow_write_byte(d->pin, 0x80);   /* TL (unused alarm) */
    ow_write_byte(d->pin, cfg);
    return ESP_OK;                 /* RAM only, EEPROM is not touched */
}

static esp_err_t measure(struct ds18b20_dev *d, float *out)
{
    esp_err_t err = ow_reset(d->pin);
    if (err != ESP_OK) return err;

    ow_write_byte(d->pin, CMD_SKIP_ROM);
    ow_write_byte(d->pin, CMD_CONVERT_T);

    /* Async part: yield the CPU while the sensor converts. */
    vTaskDelay(pdMS_TO_TICKS(conv_time_ms(d->res) + 10));

    err = ow_reset(d->pin);
    if (err != ESP_OK) return err;

    ow_write_byte(d->pin, CMD_SKIP_ROM);
    ow_write_byte(d->pin, CMD_READ_SCRATCH);

    uint8_t sp[9];
    for (int i = 0; i < 9; i++) sp[i] = ow_read_byte(d->pin);

    if (crc8(sp, 8) != sp[8]) return ESP_ERR_INVALID_CRC;

    int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);

    /* Power-on default (85.0 C, COUNT_REMAIN 0x0C) means conversion never ran. */
    if (raw == 0x0550 && sp[6] == 0x0C) return ESP_ERR_INVALID_RESPONSE;

    switch (d->res) {          /* low bits are undefined at lower resolutions */
    case DS18B20_RES_9BIT:  raw &= ~0x07; break;
    case DS18B20_RES_10BIT: raw &= ~0x03; break;
    case DS18B20_RES_11BIT: raw &= ~0x01; break;
    default: break;
    }

    *out = raw / 16.0f;
    return ESP_OK;
}

static void publish(struct ds18b20_dev *d, esp_err_t err, float t)
{
    portENTER_CRITICAL(&d->lock);
    d->last_err = err;
    if (err == ESP_OK) {
        d->last_temp  = t;
        d->have_temp  = true;
        d->last_ts_us = esp_timer_get_time();
    }
    portEXIT_CRITICAL(&d->lock);

    if (d->cb) d->cb(t, err, d->user_ctx);
}

static void ds18b20_task(void *arg)
{
    struct ds18b20_dev *d = arg;

    esp_err_t err = write_resolution(d);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "resolution setup failed: %s", esp_err_to_name(err));
    }

    while (!d->quit) {
        float t = 0;
        err = measure(d, &t);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "measure failed: %s", esp_err_to_name(err));
        }
        publish(d, err, t);

        TickType_t wait = d->period_ms ? pdMS_TO_TICKS(d->period_ms) : portMAX_DELAY;
        ulTaskNotifyTake(pdTRUE, wait);   /* timeout or ds18b20_trigger() */
    }

    gpio_reset_pin(d->pin);
    free(d);
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
esp_err_t ds18b20_create(const ds18b20_config_t *cfg, ds18b20_handle_t *out)
{
    if (!cfg || !out) return ESP_ERR_INVALID_ARG;
    if (cfg->resolution < DS18B20_RES_9BIT || cfg->resolution > DS18B20_RES_12BIT)
        return ESP_ERR_INVALID_ARG;

    struct ds18b20_dev *d = calloc(1, sizeof(*d));
    if (!d) return ESP_ERR_NO_MEM;

    d->pin       = cfg->pin;
    d->res       = cfg->resolution;
    d->period_ms = cfg->period_ms;
    d->cb        = cfg->cb;
    d->user_ctx  = cfg->user_ctx;
    d->last_err  = ESP_ERR_NOT_FINISHED;
    portMUX_INITIALIZE(&d->lock);

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << cfg->pin,
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,   /* open drain, readable */
        .pull_up_en   = GPIO_PULLUP_ENABLE,          /* backup only; use 4.7k external */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) { free(d); return err; }
    gpio_set_level(cfg->pin, 1);

    BaseType_t ok = xTaskCreatePinnedToCore(ds18b20_task, "ds18b20", cfg->stack_size,
                                            d, cfg->priority, &d->task, cfg->core_id);
    if (ok != pdPASS) { free(d); return ESP_ERR_NO_MEM; }

    *out = d;
    return ESP_OK;
}

esp_err_t ds18b20_trigger(ds18b20_handle_t h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    xTaskNotifyGive(h->task);
    return ESP_OK;
}

esp_err_t ds18b20_get_latest(ds18b20_handle_t h, float *temp_c, uint32_t *age_ms)
{
    if (!h || !temp_c) return ESP_ERR_INVALID_ARG;

    esp_err_t err;
    bool have;
    float t;
    int64_t ts;

    portENTER_CRITICAL(&h->lock);
    err  = h->last_err;
    have = h->have_temp;
    t    = h->last_temp;
    ts   = h->last_ts_us;
    portEXIT_CRITICAL(&h->lock);

    if (have) {
        *temp_c = t;
        if (age_ms) *age_ms = (uint32_t)((esp_timer_get_time() - ts) / 1000);
    }
    return err;
}

esp_err_t ds18b20_delete(ds18b20_handle_t h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    h->quit = true;
    xTaskNotifyGive(h->task);   /* task frees h and deletes itself */
    return ESP_OK;
}