/*
 * pico_uart.c — UART HAL implementation for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_uart.h"
#include "registry.h"

#ifdef RH_PICO_SDK
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#endif

/* The two links a board may register, kept here because the peripheral and its
 * pins live for the program's life and the registry keeps a pointer. */
static RoboHubUART uart_links[2];
static RoboHubPicoUART uart_link_state[2];

void pico_register_uart_link(const char* name,
                             uint32_t instance_index,
                             uint32_t tx_pin,
                             uint32_t rx_pin)
{
    if (instance_index > 1u) {
        return;
    }
#ifdef RH_PICO_SDK
    void* instance = (instance_index == 0u) ? (void*)uart0 : (void*)uart1;
#else
    void* instance = 0;
    (void)tx_pin;
    (void)rx_pin;
#endif
    RoboHubUART* self = &uart_links[instance_index];
    RoboHubPicoUART* state = &uart_link_state[instance_index];
    state->instance = instance;
    state->tx_pin = tx_pin;
    state->rx_pin = rx_pin;
    self->name = name;
    self->context = state;
    self->init = pico_uart_init;
    self->write = pico_uart_write;
    self->read = pico_uart_read;
    (void)robohub_register_uart(name, self);
}

RoboHubError pico_uart_init(RoboHubUART* self, uint32_t baudrate)
{
    if (self == 0 || self->context == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    RoboHubPicoUART* state = (RoboHubPicoUART*)self->context;
    uart_inst_t* instance = (uart_inst_t*)state->instance;
    uart_init(instance, baudrate);
    /* The pins are the board's, so the BSP names them and this applies them:
     * without the function select the peripheral drives nothing. */
    gpio_set_function(state->tx_pin, GPIO_FUNC_UART);
    gpio_set_function(state->rx_pin, GPIO_FUNC_UART);
#else
    (void)baudrate;
#endif
    return ROBOHUB_OK;
}

RoboHubError pico_uart_write(RoboHubUART* self, const uint8_t* data, rh_size len)
{
    if (self == 0 || self->context == 0 || (data == 0 && len > 0u)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    RoboHubPicoUART* state = (RoboHubPicoUART*)self->context;
    uart_write_blocking((uart_inst_t*)state->instance, data, (size_t)len);
#else
    (void)data;
    (void)len;
#endif
    return ROBOHUB_OK;
}

RoboHubError pico_uart_read(RoboHubUART* self, uint8_t* data, rh_size len)
{
    if (self == 0 || self->context == 0 || (data == 0 && len > 0u)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    RoboHubPicoUART* state = (RoboHubPicoUART*)self->context;
    uart_read_blocking((uart_inst_t*)state->instance, data, (size_t)len);
#else
    (void)data;
    (void)len;
#endif
    return ROBOHUB_OK;
}

rh_size pico_uart_read_available(RoboHubUART* self, uint8_t* data, rh_size max_len)
{
    if (self == 0 || self->context == 0 || data == 0 || max_len == 0u) {
        return 0u;
    }
#ifdef RH_PICO_SDK
    RoboHubPicoUART* state = (RoboHubPicoUART*)self->context;
    uart_inst_t* instance = (uart_inst_t*)state->instance;
    rh_size got = 0u;
    while (got < max_len && uart_is_readable(instance)) {
        data[got++] = (uint8_t)uart_getc(instance);
    }
    return got;
#else
    (void)max_len;
    return 0u;
#endif
}
