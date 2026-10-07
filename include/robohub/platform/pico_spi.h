/*
 * pico_spi.h — SPI HAL for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_SPI_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "error.h"
#include "robohub/hal/spi.h"
#include <stdint.h>
#include <stddef.h>

RoboHubError pico_spi_init(RoboHubSPI* self, uint32_t speed_hz);
RoboHubError pico_spi_transfer(RoboHubSPI* self, const uint8_t* tx, uint8_t* rx, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_SPI_H */
