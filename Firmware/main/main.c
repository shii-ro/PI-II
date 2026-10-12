#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "db18b20.h"

#define DS18B20_PIN   GPIO_NUM_4
#define LCD_HOST      SPI2_HOST

#define LED_PIN GPIO_NUM_23
#define LCD_PIN_SCLK  GPIO_NUM_22
#define LCD_PIN_MOSI  GPIO_NUM_21
#define LCD_PIN_CS    GPIO_NUM_5
#define LCD_PIN_DC    GPIO_NUM_19
#define LCD_PIN_RST   GPIO_NUM_18
#define LCD_H_RES     240
#define LCD_V_RES     320

static const char *TAG = "Temperature Display";

static ds18b20_handle_t s_sensor;
static lv_obj_t *s_label;

static void temp_timer_cb(lv_timer_t *t)
{
    (void)t;
    float temp = 0;
    uint32_t age_ms = 0;
    esp_err_t err = ds18b20_get_latest(s_sensor, &temp, &age_ms);

    if (err == ESP_ERR_NOT_FINISHED) {
        lv_label_set_text(s_label, "-- \xC2\xB0""C");
    } else if (err != ESP_OK) {
        lv_label_set_text(s_label, "Sensor error");
    } else {
        /* LVGL's printf has no float support by default, so use integers. */
        int t10 = (int)lroundf(temp * 10.0f);
        lv_label_set_text_fmt(s_label, "%s%d.%d \xC2\xB0""C",
                              t10 < 0 ? "-" : "", abs(t10) / 10, abs(t10) % 10);
    }
}

static void ui_create(void)
{
    s_label = lv_label_create(lv_screen_active());   /* LVGL 9; LVGL 8: lv_scr_act() */
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_14, 0);
    lv_obj_center(s_label);
    lv_label_set_text(s_label, "-- \xC2\xB0""C");

    lv_timer_create(temp_timer_cb, 500, NULL);
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

    /* Panel IO: component's default macro (40 MHz, SPI mode 0, 8-bit cmd/param) */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config =
    ILI9341_PANEL_IO_SPI_CONFIG(LCD_PIN_CS, LCD_PIN_DC, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    /* Panel driver */
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = 
    {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    /* If the image is mirrored/flipped on your module, tweak these: */
    // ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    // ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    /* LVGL via esp_lvgl_port */
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = 
    {
        .io_handle     = io_handle,
        .panel_handle  = panel_handle,
        .buffer_size   = LCD_H_RES * 40,
        .double_buffer = true,
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .monochrome    = false,
        .rotation      = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
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

    /* 3) Build the UI (LVGL port lock required). */
    lvgl_port_lock(0);
    ui_create();
    lvgl_port_unlock();

    ESP_LOGI(TAG, "running");
}