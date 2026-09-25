#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        /* Called once at startup. Return false if the sensor/bus failed to init. */
        bool (*init)(void);

        /* Returns true and fills *out_celsius on success, false on read error
         * (e.g. sensor disconnected, CRC fail on the 1-Wire bus, etc). */
        bool (*read_celsius)(float *out_celsius);
    } temp_sensor_driver_t;

    /* Wire up a driver (mock or real). Must be called before any read. */
    void temp_sensor_init(const temp_sensor_driver_t *driver);

    /* Convenience wrapper the UI calls; returns false if no driver is set
     * or the underlying read failed (UI should show "--" in that case). */
    bool temp_sensor_read_celsius(float *out_celsius);

#ifdef __cplusplus
}
#endif
