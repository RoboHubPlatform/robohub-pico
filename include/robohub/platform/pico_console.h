/*
 * pico_console.h — Console HAL for Pico boards via UART.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_CONSOLE_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "robohub/hal/console.h"

RoboHubConsoleError pico_console_init(void);
RoboHubConsoleError pico_console_write(const char* data, size_t len);
RoboHubConsoleError pico_console_puts(const char* text);

/* Advance a console transport that has to be polled — the USB one — and do
 * nothing on one that does not. Call it from a loop that sleeps. */
void robohub_console_task(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_CONSOLE_H */
