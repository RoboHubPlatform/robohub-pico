/*
 * pico_i2c.h — I2C HAL for Pico boards.
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
#include "robohub/hal/i2c.h"
#include <stdint.h>
#include <stddef.h>

RoboHubError pico_i2c_init(RoboHubI2C* self, uint32_t speed_hz);
RoboHubError pico_i2c_write(RoboHubI2C* self, uint8_t addr, const uint8_t* data, size_t len);
RoboHubError pico_i2c_read(RoboHubI2C* self, uint8_t addr, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_I2C_H */
