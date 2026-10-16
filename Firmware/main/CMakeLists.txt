idf_component_register(
                    SRCS "ui.c" "ble_temp.c" "main.c" "db18b20.c"
                    INCLUDE_DIRS "."
                    REQUIRES 
                            bt    
                            esp_lvgl_port 
                            esp_lcd_ili9341
                            lvgl
                            esp_driver_gpio 
                            esp_driver_spi 
                            esp_timer 
                            esp_rom 
                            nvs_flash
                            freertos
)