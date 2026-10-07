/*
 * pico_rs485.c — RS485 HAL implementation for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_rs485.h"

RoboHubError pico_rs485_init(RoboHubRS485* self, uint32_t baudrate)
{
    (void)self;
    (void)baudrate;
    return ROBOHUB_ERR_NOT_SUPPORTED;
}

RoboHubError pico_rs485_write(RoboHubRS485* self, const uint8_t* data, size_t len)
{
    (void)self;
    (void)data;
    (void)len;
    return ROBOHUB_ERR_NOT_SUPPORTED;
}

RoboHubError pico_rs485_read(RoboHubRS485* self, uint8_t* data, size_t len)
{
    (void)self;
    (void)data;
    (void)len;
    return ROBOHUB_ERR_NOT_SUPPORTED;
}

RoboHubError pico_rs485_set_direction(RoboHubRS485* self, uint32_t dir)
{
    (void)self;
    (void)dir;
    return ROBOHUB_ERR_NOT_SUPPORTED;
}
