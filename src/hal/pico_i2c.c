/*
 * pico_i2c.c — the I2C HAL on the RP2350's I2C controllers (PICO-0015).
 *
 * A controller is brought up on the pins its board gives: they are muxed to
 * I2C and pulled up (a breakout's own pull-ups are stronger, and a bare part's
 * are needed anyway). Every transfer has a timeout, so a bus held low by a part
 * that browned out returns an error instead of stopping the node's loop.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_i2c.h"

#ifdef RH_PICO_SDK
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#endif

/* A transfer's allowance: a millisecond, and a little more per byte than a
 * byte takes at 100 kHz. */
#define PICO_I2C_TIMEOUT_US(len) (1000u + 200u * (uint32_t)(len))

void pico_i2c_create(RoboHubPicoI2C* bus, rh_name name, rh_u8 index, rh_u32 sda, rh_u32 scl)
{
    bus->interface.name = name;
    bus->interface.context = bus;
    bus->interface.init = pico_i2c_init;
    bus->interface.write = pico_i2c_write;
    bus->interface.read = pico_i2c_read;
    bus->index = index;
    bus->sda = sda;
    bus->scl = scl;
}

#ifdef RH_PICO_SDK
static i2c_inst_t* pico_i2c_instance(RoboHubI2C* self)
{
    const RoboHubPicoI2C* bus = (const RoboHubPicoI2C*)self->context;
    return bus->index == 0u ? i2c0 : i2c1;
}
#endif

// @req PICO-0015
RoboHubError pico_i2c_init(RoboHubI2C* self, uint32_t speed_hz)
{
    if (self == NULL || self->context == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    const RoboHubPicoI2C* bus = (const RoboHubPicoI2C*)self->context;
    i2c_init(pico_i2c_instance(self), speed_hz != 0u ? speed_hz : 100000u);
    gpio_set_function(bus->sda, GPIO_FUNC_I2C);
    gpio_set_function(bus->scl, GPIO_FUNC_I2C);
    gpio_pull_up(bus->sda);
    gpio_pull_up(bus->scl);
#else
    (void)speed_hz;
#endif
    return ROBOHUB_OK;
}

// @req PICO-0015
RoboHubError pico_i2c_write(RoboHubI2C* self, uint8_t addr, const uint8_t* data, size_t len)
{
    if (self == NULL || self->context == NULL || (data == NULL && len != 0u)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    int done = i2c_write_timeout_us(pico_i2c_instance(self), addr, data, len, false, PICO_I2C_TIMEOUT_US(len));
    if (done == PICO_ERROR_TIMEOUT) {
        return ROBOHUB_ERR_TIMEOUT;
    }
    return done == (int)len ? ROBOHUB_OK : ROBOHUB_ERR_IO;
#else
    (void)addr;
    return ROBOHUB_OK;
#endif
}

// @req PICO-0015
RoboHubError pico_i2c_read(RoboHubI2C* self, uint8_t addr, uint8_t* data, size_t len)
{
    if (self == NULL || self->context == NULL || data == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    int done = i2c_read_timeout_us(pico_i2c_instance(self), addr, data, len, false, PICO_I2C_TIMEOUT_US(len));
    if (done == PICO_ERROR_TIMEOUT) {
        return ROBOHUB_ERR_TIMEOUT;
    }
    return done == (int)len ? ROBOHUB_OK : ROBOHUB_ERR_IO;
#else
    (void)addr;
    (void)len;
    return ROBOHUB_OK;
#endif
}
