/*
 * pico_uart.c — UART HAL implementation for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_uart.h"

#ifdef RH_PICO_SDK
#include "hardware/uart.h"
#include "hardware/gpio.h"
#endif

RoboHubError pico_uart_init(RoboHubUART* self, uint32_t baudrate)
{
    (void)self;
#ifdef RH_PICO_SDK
    uart_init((uart_inst_t*)self->context, baudrate);
#endif
    return ROBOHUB_OK;
}

RoboHubError pico_uart_write(RoboHubUART* self, const uint8_t* data, size_t len)
{
    (void)self;
#ifdef RH_PICO_SDK
    uart_write_blocking((uart_inst_t*)self->context, data, len);
#endif
    return ROBOHUB_OK;
}

RoboHubError pico_uart_read(RoboHubUART* self, uint8_t* data, size_t len)
{
    (void)self;
#ifdef RH_PICO_SDK
    uart_read_blocking((uart_inst_t*)self->context, data, len);
#endif
    return ROBOHUB_OK;
}
