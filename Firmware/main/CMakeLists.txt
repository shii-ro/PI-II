idf_component_register(
                    SRCS "main.c" "db18b20.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_lvgl_port esp_lcd_ili9341 lvgl
                             esp_driver_gpio esp_driver_spi esp_timer esp_rom freertos
)