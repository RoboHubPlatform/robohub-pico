/*
 * pico_gpio.c — GPIO HAL implementation for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_gpio.h"

#ifdef RH_PICO_SDK
#include "hardware/gpio.h"
#endif

RoboHubError pico_gpio_init(RoboHubGPIO* self)
{
    (void)self;
    return ROBOHUB_OK;
}

RoboHubError pico_gpio_set_mode(RoboHubGPIO* self, uint32_t pin, uint32_t mode)
{
    (void)self;
#ifdef RH_PICO_SDK
    if (mode == ROBOHUB_GPIO_MODE_OUTPUT || mode == ROBOHUB_GPIO_MODE_INPUT) {
        /* A plain pin, not an alternate function: the SIO function and a
         * direction. This is the mode a chip select or a transceiver's
         * direction pin is set to. */
        gpio_init(pin);
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, mode == ROBOHUB_GPIO_MODE_OUTPUT ? GPIO_OUT : GPIO_IN);
        return ROBOHUB_OK;
    }
    /* Anything else is the platform's own: an alternate function by number, for
     * a board that wants one. */
    gpio_set_function(pin, (gpio_function_t)mode);
#endif
    (void)pin;
    (void)mode;
    return ROBOHUB_OK;
}

RoboHubError pico_gpio_write(RoboHubGPIO* self, uint32_t pin, uint32_t value)
{
    (void)self;
#ifdef RH_PICO_SDK
    gpio_put(pin, value);
#endif
    return ROBOHUB_OK;
}

RoboHubError pico_gpio_read(RoboHubGPIO* self, uint32_t pin, uint32_t* value)
{
    (void)self;
#ifdef RH_PICO_SDK
    *value = gpio_get(pin);
#endif
    return ROBOHUB_OK;
}
