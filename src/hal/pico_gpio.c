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
    gpio_set_function(pin, (gpio_function_t)mode);
#endif
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
