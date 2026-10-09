/*
 * pico_uart.h — UART HAL for Pico boards.
 *
 * A UART instance is a peripheral, two pins and a speed, and all three are the
 * board's. `pico_register_uart_link` binds them to a `RoboHubUART` a program
 * looks up by name; the operations are the SDK's.
 *
 * There are two reads. `read` is the contract's, and completes or fails; a
 * link's parser cannot use it, because a byte that has not arrived yet is not a
 * failure. `pico_uart_read_available` returns what is there and nothing more,
 * which is what a program's loop feeds a transport from.
 *
 * The peripheral is named by index rather than by the SDK's `uart0`/`uart1`
 * here, so a board's BSP can register a link without including an SDK header —
 * the BSP is compiled for the program as well as for the platform, and only the
 * platform has the SDK in scope.
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
#include "types.h"
#include "robohub/hal/uart.h"
#include <stdint.h>

/** The peripheral, its pins and the interface that uses them. */
typedef struct {
    void*    instance;  /**< `uart_inst_t*`. */
    uint32_t tx_pin;
    uint32_t rx_pin;
} RoboHubPicoUART;

/** Which UART: 0 is `uart0`, 1 is `uart1`. */
#define RH_PICO_UART0 0u
#define RH_PICO_UART1 1u

/**
 * @brief Bind a peripheral and two pins to a UART interface, and register it.
 *
 * `state` is the platform's, one per index, so a board calls this with a name
 * and pins it knows and no SDK type.
 */
void pico_register_uart_link(const char* name,
                             uint32_t instance_index,
                             uint32_t tx_pin,
                             uint32_t rx_pin);

RoboHubError pico_uart_init(RoboHubUART* self, uint32_t baudrate);
RoboHubError pico_uart_write(RoboHubUART* self, const uint8_t* data, rh_size len);
RoboHubError pico_uart_read(RoboHubUART* self, uint8_t* data, rh_size len);

/** Copy whatever is waiting into `data`, up to `max_len`; return how many. */
rh_size pico_uart_read_available(RoboHubUART* self, uint8_t* data, rh_size max_len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_UART_H */
