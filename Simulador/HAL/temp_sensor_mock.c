#include "temp_sensor.h"
#include <stdlib.h>

static float s_current_temp_c = 24.0f;
static bool  s_initialized     = false;

static bool mock_init(void)
{
    s_current_temp_c = 24.0f;
    s_initialized = true;
    return true;
}

static bool mock_read_celsius(float *out_celsius)
{
    if (!s_initialized || !out_celsius) {
        return false;
    }

    /* +/- 0.3C random step */
    float step = ((float)(rand() % 61) - 30.0f) / 100.0f;
    s_current_temp_c += step;

    if (s_current_temp_c < 10.0f) s_current_temp_c = 10.0f;
    if (s_current_temp_c > 45.0f) s_current_temp_c = 45.0f;

    *out_celsius = s_current_temp_c;
    return true;
}

const temp_sensor_driver_t temp_sensor_driver_mock = {
    .init         = mock_init,
    .read_celsius = mock_read_celsius,
};
