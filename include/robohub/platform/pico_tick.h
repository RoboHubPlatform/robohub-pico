/*
 * pico_tick.h — System tick and delay internals.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_TICK_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_TICK_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include <stdint.h>

uint64_t pico_timer_get_millis(void);
uint64_t pico_timer_get_micros(void);
void pico_timer_delay_ms(uint32_t ms);
void pico_timer_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_TICK_H */
