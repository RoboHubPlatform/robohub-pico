/*
 * bsp.h — Board support package for Raspberry Pi Pico 2W.
 *
 * Defines pin mappings, clock configuration, and board-specific
 * initialization for the Pico 2W (RP2350).
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_PICO2W_BSP_BSP_H
#define BOARDS_PICO2W_BSP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  Board identification                                                      */
/* -------------------------------------------------------------------------- */
#define RH_BOARD_PICO_2W        1
#define RH_MCU_RP2350           1
#define RH_CPU_CORTEX_M33       1

/* -------------------------------------------------------------------------- */
/*  Clock configuration                                                       */
/* -------------------------------------------------------------------------- */
#define RH_SYS_CLOCK_HZ         150000000u
#define RH_TICK_HZ              1000u

/* -------------------------------------------------------------------------- */
/*  LED                                                                       */
/* -------------------------------------------------------------------------- */
/* The Pico 2W has no LED on a GPIO. Its light is on the wireless chip —
 * `CYW43_WL_GPIO_LED_PIN`, which the board header states and which needs the
 * CYW43 driver to reach. GP15 is an ordinary pin, so driving it lights nothing
 * and a dark board says nothing about whether a program is running. */
#define RH_LED_PIN              (-1)
#define RH_LED_ACTIVE_HIGH      1

/* -------------------------------------------------------------------------- */
/*  UART console                                                              */
/* -------------------------------------------------------------------------- */
/* UARTs are named by index at the board layer: uart0 is index 0 and uart1 is
 * index 1. The SDK's `uart0`/`uart1` are the platform's business, because only
 * the platform compiles with the SDK in scope — this header is read by a
 * program's build too. */
#define RH_UART0_INDEX          0
#define RH_UART1_INDEX          1
#define RH_CONSOLE_BAUDRATE     115200
#define RH_CONSOLE_TX_PIN       0
#define RH_CONSOLE_RX_PIN       1

/* A second UART, for a downstream link: an RS-485 transceiver, or another board
 * on TTL. uart0 carries the console; uart1 is free on GPIO 4/5, which the board
 * routes to no other peripheral. */
#define RH_UART1_TX_PIN         4
#define RH_UART1_RX_PIN         5

/* -------------------------------------------------------------------------- */
/*  I2C                                                                       */
/* -------------------------------------------------------------------------- */
#define RH_I2C0_INSTANCE        i2c0
#define RH_I2C0_SDA_PIN         16
#define RH_I2C0_SCL_PIN         17
#define RH_I2C1_INSTANCE        i2c1
#define RH_I2C1_SDA_PIN         18
#define RH_I2C1_SCL_PIN         19

/* -------------------------------------------------------------------------- */
/*  SPI                                                                       */
/* -------------------------------------------------------------------------- */
#define RH_SPI0_INSTANCE        spi0
#define RH_SPI0_TX_PIN          19
#define RH_SPI0_RX_PIN          20
#define RH_SPI0_SCK_PIN         18
#define RH_SPI0_CSN_PIN         17

/* -------------------------------------------------------------------------- */
/*  1-Wire                                                                    */
/* -------------------------------------------------------------------------- */
/* A 1-Wire bus here is a software one on one pin, so any free GPIO will do.
 * GP28 is free on the Pico 2W and carries no peripheral assignment; it is also
 * an ADC pin, which costs nothing when it is used as a digital line. */
#define RH_ONEWIRE0_PIN         28

/* -------------------------------------------------------------------------- */
/*  Board functions                                                           */
/* -------------------------------------------------------------------------- */
void bsp_init(void);
void bsp_register_hals(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_PICO2W_BSP_BSP_H */
