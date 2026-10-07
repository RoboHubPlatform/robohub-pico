/*
 * pico_uart.h — UART HAL for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_UART_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "error.h"
#include "robohub/hal/uart.h"
#include <stdint.h>
#include <stddef.h>

RoboHubError pico_uart_init(RoboHubUART* self, uint32_t baudrate);
RoboHubError pico_uart_write(RoboHubUART* self, const uint8_t* data, size_t len);
RoboHubError pico_uart_read(RoboHubUART* self, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_UART_H */
