/*
 * pico_rs485.h — RS485 HAL for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_RS485_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_RS485_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "error.h"
#include "robohub/hal/rs485.h"
#include <stdint.h>
#include <stddef.h>

RoboHubError pico_rs485_init(RoboHubRS485* self, uint32_t baudrate);
RoboHubError pico_rs485_write(RoboHubRS485* self, const uint8_t* data, size_t len);
RoboHubError pico_rs485_read(RoboHubRS485* self, uint8_t* data, size_t len);
RoboHubError pico_rs485_set_direction(RoboHubRS485* self, uint32_t dir);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_RS485_H */
