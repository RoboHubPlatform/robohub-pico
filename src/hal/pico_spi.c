/*
 * pico_spi.c — SPI HAL implementation for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_spi.h"

#ifdef RH_PICO_SDK
#include "hardware/spi.h"
#endif

RoboHubError pico_spi_init(RoboHubSPI* self, uint32_t speed_hz)
{
    (void)self;
#ifdef RH_PICO_SDK
    spi_init((spi_inst_t*)self->context, speed_hz);
#endif
    return ROBOHUB_OK;
}

RoboHubError pico_spi_transfer(RoboHubSPI* self, const uint8_t* tx, uint8_t* rx, size_t len)
{
    (void)self;
#ifdef RH_PICO_SDK
    spi_write_read_blocking((spi_inst_t*)self->context, tx, rx, len);
#endif
    return ROBOHUB_OK;
}
