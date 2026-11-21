/*
 * pico_i2c.h — the I2C HAL on the RP2350's I2C controllers.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_I2C_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "error.h"
#include "types.h"
#include "robohub/hal/i2c.h"
#include <stdint.h>
#include <stddef.h>

/** One controller on two pins: the interface a driver is handed, first. */
typedef struct RoboHubPicoI2C {
    RoboHubI2C interface;
    rh_u8      index;   /* 0 or 1: i2c0 or i2c1 */
    rh_u32     sda;
    rh_u32     scl;
} RoboHubPicoI2C;

/** Fill in a controller's interface; its pins are muxed when it is brought up. */
void pico_i2c_create(RoboHubPicoI2C* bus, rh_name name, rh_u8 index, rh_u32 sda, rh_u32 scl);

RoboHubError pico_i2c_init(RoboHubI2C* self, uint32_t speed_hz);
RoboHubError pico_i2c_write(RoboHubI2C* self, uint8_t addr, const uint8_t* data, size_t len);
RoboHubError pico_i2c_read(RoboHubI2C* self, uint8_t addr, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_I2C_H */
