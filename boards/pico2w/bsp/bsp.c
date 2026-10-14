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

#if defined(CONFIG_RH_HAL_DHT) && CONFIG_RH_HAL_DHT
#include "robohub/hal/dht.h"
#include "pico_dht.h"
#endif

#if defined(CONFIG_RH_HAL_UART) && CONFIG_RH_HAL_UART
#include "robohub/hal/uart.h"
#include "pico_uart.h"
#endif

/* Under a kernel, the SDK's background alarm — which is what pumps TinyUSB in a
 * bare-metal build — is not running, so the CDC ring buffers are never drained:
 * the device enumerates and the console is silent. This task is that pump. It
 * exists only when FreeRTOS is, and only when the console is the USB one. */
#if defined(CONFIG_RH_FREERTOS) && CONFIG_RH_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#if defined(CONFIG_RH_CONSOLE_USB) && CONFIG_RH_CONSOLE_USB
#include "tusb.h"
#endif

static void usb_cdc_service(void* argument)
{
    (void)argument;
    for (;;) {
#if defined(CONFIG_RH_CONSOLE_USB) && CONFIG_RH_CONSOLE_USB
        tud_task();
#endif
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static void usb_cdc_service_start(void)
{
    (void)xTaskCreate(usb_cdc_service, "usb_cdc", 512, 0,
                      tskIDLE_PRIORITY + 1, 0);
}
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

/* A general GPIO controller, so a board's carrier can drive a pin the platform
 * did not name — a transceiver's direction pin, a chip select. The LED instance
 * is the same operations under a friendlier name. */
static RoboHubGPIO gpio0 = {
    .name = "gpio0",
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

#if defined(CONFIG_RH_HAL_DHT) && CONFIG_RH_HAL_DHT
/* One software DHT line on RH_DHT0_PIN, filled in at registration for the same
 * reason the 1-Wire bus is. */
static RoboHubPicoDHT dht0;
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

#if defined(CONFIG_RH_FREERTOS) && CONFIG_RH_FREERTOS
    /* Before the scheduler starts, so the pump is running as soon as it does. */
    usb_cdc_service_start();
#endif

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
    robohub_register_gpio("gpio0", &gpio0);
    robohub_register_timer("system_timer", &system_timer);

#if defined(CONFIG_RH_HAL_ONEWIRE) && CONFIG_RH_HAL_ONEWIRE
    /* The bus a device driver looks up by name. A project that fits a 1-Wire
     * part selects the HAL, and the board is where the pin it is wired to is
     * known. */
    pico_onewire_create(&onewire0, "onewire0", RH_ONEWIRE0_PIN);
    robohub_register_onewire("onewire0", &onewire0.interface);
#endif

#if defined(CONFIG_RH_HAL_DHT) && CONFIG_RH_HAL_DHT
    /* The line a DHT driver looks up by name. A project that fits a DHT selects
     * the HAL, and the board is where the pin it is wired to is known. */
    pico_dht_create(&dht0, "dht0", RH_DHT0_PIN);
    robohub_register_dht("dht0", &dht0.interface);
#endif

#if defined(CONFIG_RH_HAL_UART) && CONFIG_RH_HAL_UART
    /* A program that needs a downstream link looks up "uart1"; uart0 is here
     * too, so a board on TTL where the console is USB can use it as a link. The
     * platform owns the peripherals, so the board names an index and its pins
     * and no SDK type. */
    pico_register_uart_link("uart0", RH_UART0_INDEX, RH_CONSOLE_TX_PIN, RH_CONSOLE_RX_PIN);
    pico_register_uart_link("uart1", RH_UART1_INDEX, RH_UART1_TX_PIN, RH_UART1_RX_PIN);
#endif
}
