/*
 * pico_timer.h — Timer HAL for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_TIMER_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "robohub/hal/timer.h"

uint64_t pico_timer_get_millis(RoboHubTimer* self);
uint64_t pico_timer_get_micros(RoboHubTimer* self);
void pico_timer_delay_ms(RoboHubTimer* self, uint32_t ms);
void pico_timer_delay_us(RoboHubTimer* self, uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_TIMER_H */
