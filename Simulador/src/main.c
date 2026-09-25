#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <SDL.h>
#include "hal/hal.h"

#include "UI/ui.h"
#include "HAL/temp_sensor.h"
#include "HAL/wifi_manager.h"

extern const temp_sensor_driver_t temp_sensor_driver_mock;
extern const wifi_driver_t wifi_driver_mock;

int main(int argc, char **argv)
{
	(void)argc; /*Unused*/
	(void)argv; /*Unused*/

	/*Initialize LVGL*/
	lv_init();

	/*Initialize the HAL (display, input devices, tick) for LVGL*/
	sdl_hal_init(320, 240);

	temp_sensor_init(&temp_sensor_driver_mock);
	wifi_manager_init(&wifi_driver_mock);

	ui_init();

	while (1)
	{
		ui_tick();
		uint32_t sleep_time_ms = lv_timer_handler();
		if (sleep_time_ms == LV_NO_TIMER_READY)
		{
			sleep_time_ms = LV_DEF_REFR_PERIOD;
		}
		usleep(sleep_time_ms * 1000);
	}

	return 0;
}
