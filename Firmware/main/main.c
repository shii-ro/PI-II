#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "db18b20.h"
#include "ble_temp.h"
#include "ui.h"

#define DS18B20_PIN   GPIO_NUM_26
#define LCD_HOST      SPI2_HOST

#define LED_PIN       GPIO_NUM_23
#define LCD_PIN_SCLK  GPIO_NUM_22
#define LCD_PIN_MOSI  GPIO_NUM_21
#define LCD_PIN_CS    GPIO_NUM_5
#define LCD_PIN_DC    GPIO_NUM_19
#define LCD_PIN_RST   GPIO_NUM_18
#define LCD_H_RES     320
#define LCD_V_RES     240

static const char *TAG = "Temperature Display";

static ds18b20_handle_t s_sensor;

/* Runs on the LVGL task, so the LVGL lock is already held. */
static void temp_timer_cb(lv_timer_t *t)
{
    (void)t;
    float temp = 0;
    uint32_t age_ms = 0;
    esp_err_t err = ds18b20_get_latest(s_sensor, &temp, &age_ms);

    if (err == ESP_ERR_NOT_FINISHED) {
        ui_show_temp_waiting();
    } else if (err != ESP_OK) {
        ui_show_temp_error();
    } else {
        ui_show_temperature(temp);
        ble_temp_update(temp);
    }
}

static lv_display_t *display_init(void)
{
    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&led_config));
    gpio_set_level(LED_PIN, 1);  // LED off initially

    /* SPI bus */
    const spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* Panel IO: SPI mode 3 */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num       = LCD_PIN_CS,
        .dc_gpio_num       = LCD_PIN_DC,
        .spi_mode          = 3,
        .pclk_hz           = 20 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                            &io_config, &io_handle));

    /* Panel driver */
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    /* LVGL via esp_lvgl_port */
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = io_handle,
        .panel_handle  = panel_handle,
        .buffer_size   = LCD_H_RES * 40,
        .double_buffer = true,
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .monochrome    = false,
        .rotation      = { .swap_xy = true, .mirror_x = true, .mirror_y = false },
        .flags         = { .buff_dma = true, .swap_bytes = true },
    };
    return lvgl_port_add_disp(&disp_cfg);
}

void app_main(void)
{
    /* 1) Display + LVGL */
    lv_display_t *disp = display_init();
    (void)disp;

    /* 2) Start the sensor: this returns immediately. */
    ds18b20_config_t cfg = DS18B20_CONFIG_DEFAULT(DS18B20_PIN);
    cfg.resolution = DS18B20_RES_12BIT;
    cfg.period_ms  = 1000;
    ESP_ERROR_CHECK(ds18b20_create(&cfg, &s_sensor));

    /* 3) Build the UI and start the polling timer (LVGL lock required). */
    lvgl_port_lock(0);
    ui_create();
    lv_timer_create(temp_timer_cb, 500, NULL);
    lvgl_port_unlock();

    /* 4) BLE last: its state callback touches UI objects that must exist. */
    ble_temp_init(NULL, ui_set_ble_state_safe);

    ESP_LOGI(TAG, "running");
}