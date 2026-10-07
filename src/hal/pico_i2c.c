/*
 * pico_i2c.c — I2C HAL implementation for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_i2c.h"

#ifdef RH_PICO_SDK
#include "hardware/i2c.h"
#endif

RoboHubError pico_i2c_init(RoboHubI2C* self, uint32_t speed_hz)
{
    (void)self;
#ifdef RH_PICO_SDK
    i2c_init((i2c_inst_t*)self->context, speed_hz);
#endif
    return ROBOHUB_OK;
}

RoboHubError pico_i2c_write(RoboHubI2C* self, uint8_t addr, const uint8_t* data, size_t len)
{
    (void)self;
#ifdef RH_PICO_SDK
    return i2c_write_blocking((i2c_inst_t*)self->context, addr, data, len, false) == (int)len
        ? ROBOHUB_OK : ROBOHUB_ERR_IO;
#else
    return ROBOHUB_OK;
#endif
}

RoboHubError pico_i2c_read(RoboHubI2C* self, uint8_t addr, uint8_t* data, size_t len)
{
    (void)self;
#ifdef RH_PICO_SDK
    return i2c_read_blocking((i2c_inst_t*)self->context, addr, data, len, false) == (int)len
        ? ROBOHUB_OK : ROBOHUB_ERR_IO;
#else
    return ROBOHUB_OK;
#endif
}
