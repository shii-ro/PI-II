#include "temp_sensor.h"

static const temp_sensor_driver_t *s_driver = NULL;

void temp_sensor_init(const temp_sensor_driver_t *driver)
{
    s_driver = driver;
    if (s_driver && s_driver->init) {
        s_driver->init();
    }
}

bool temp_sensor_read_celsius(float *out_celsius)
{
    if (!s_driver || !s_driver->read_celsius) {
        return false;
    }
    return s_driver->read_celsius(out_celsius);
}
