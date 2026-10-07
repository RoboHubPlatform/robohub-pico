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

/* The generated configuration, so a board source can leave out what the project
 * did not ask for. The program carries this header's directory; a board built
 * on its own may not have it, which is what the guard is for. */
#if defined(__has_include)
#  if __has_include("autoconf.h")
#    include "autoconf.h"
#  endif
#endif

#if defined(CONFIG_RH_HAL_ONEWIRE) && CONFIG_RH_HAL_ONEWIRE
#include "robohub/hal/onewire.h"
#include "pico_onewire.h"
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

#if defined(CONFIG_RH_HAL_ONEWIRE) && CONFIG_RH_HAL_ONEWIRE
/* One software 1-Wire bus on RH_ONEWIRE0_PIN. It is filled in at registration,
 * because the pin is the board's and the interface has to name it. */
static RoboHubPicoOneWire onewire0;
#endif

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

#if defined(CONFIG_RH_HAL_ONEWIRE) && CONFIG_RH_HAL_ONEWIRE
    /* The bus a device driver looks up by name. A project that fits a 1-Wire
     * part selects the HAL, and the board is where the pin it is wired to is
     * known. */
    pico_onewire_create(&onewire0, "onewire0", RH_ONEWIRE0_PIN);
    robohub_register_onewire("onewire0", &onewire0.interface);
#endif
}
