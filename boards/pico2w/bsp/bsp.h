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
#define RH_CONSOLE_UART         uart0
#define RH_CONSOLE_BAUDRATE     115200
#define RH_CONSOLE_TX_PIN       0
#define RH_CONSOLE_RX_PIN       1

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
/*  Board functions                                                           */
/* -------------------------------------------------------------------------- */
void bsp_init(void);
void bsp_register_hals(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_PICO2W_BSP_BSP_H */
