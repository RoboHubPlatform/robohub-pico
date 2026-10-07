/*
 * pico_gpio.h — GPIO HAL for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_GPIO_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "error.h"
#include "robohub/hal/gpio.h"

RoboHubError pico_gpio_init(RoboHubGPIO* self);
RoboHubError pico_gpio_set_mode(RoboHubGPIO* self, uint32_t pin, uint32_t mode);
RoboHubError pico_gpio_write(RoboHubGPIO* self, uint32_t pin, uint32_t value);
RoboHubError pico_gpio_read(RoboHubGPIO* self, uint32_t pin, uint32_t* value);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_GPIO_H */
