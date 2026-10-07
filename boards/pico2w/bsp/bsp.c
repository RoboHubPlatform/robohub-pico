/*
 * bsp.c — Board support package for Raspberry Pi Pico 2W.
 *
 * Initializes the board and registers HAL instances with the RoboHub
 * registry.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "bsp.h"
#include "platform.h"
#include "registry.h"
#include "robohub/hal/gpio.h"
#include "robohub/hal/timer.h"
#include "robohub/hal/console.h"
#include "error.h"
#include "pico_gpio.h"
#include "pico_timer.h"

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/clocks.h"
#endif

/* -------------------------------------------------------------------------- */
/*  HAL instances                                                             */
/* -------------------------------------------------------------------------- */

/* The operations are the platform's own implementations. They were null here —
 * a board that registered instances nothing could call — which built and
 * linked and then faulted the first time a program used one, because calling
 * through a null function pointer is not a diagnostic a build can give. */
static RoboHubGPIO led_gpio = {
    .name = "user_led",
    .context = 0,
    .init = pico_gpio_init,
    .set_mode = pico_gpio_set_mode,
    .write = pico_gpio_write,
    .read = pico_gpio_read,
};

static RoboHubTimer system_timer = {
    .name = "system_timer",
    .context = 0,
    .get_millis = pico_timer_get_millis,
    .get_micros = pico_timer_get_micros,
    .delay_ms = pico_timer_delay_ms,
    .delay_us = pico_timer_delay_us,
};

/* -------------------------------------------------------------------------- */
/*  Board init                                                                */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_board_register(void)
{
    RoboHubConsoleError cerr = robohub_console_init();
    if (cerr != ROBOHUB_CONSOLE_OK) {
        return ROBOHUB_ERR_IO;
    }

    bsp_register_hals();
    return ROBOHUB_OK;
}

void bsp_init(void)
{
#ifdef RH_PICO_SDK
    stdio_init_all();
#endif
}

void bsp_register_hals(void)
{
    robohub_register_gpio("user_led", &led_gpio);
    robohub_register_timer("system_timer", &system_timer);
}
