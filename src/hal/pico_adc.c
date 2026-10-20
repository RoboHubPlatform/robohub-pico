/*
 * pico_adc.c — ADC interface implementation for Pico boards.
 *
 * A reading is one conversion: select the input, start the converter, take the
 * result. The converter is stopped again afterwards, so a program that reads
 * once a second does not leave it running between readings.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_adc.h"

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#include "hardware/adc.h"
#endif

static RoboHubError adc_op_init(RoboHubADC* self)
{
    RoboHubPicoADC* adc = (RoboHubPicoADC*)self->context;
#ifdef RH_PICO_SDK
    adc_init();
    /* Each input is routed to the converter by its pin, which is the board's to
     * say and not the converter's to guess. */
    for (rh_u8 i = 0; i < adc->channel_count; i++) {
        adc_gpio_init(adc->gpio[i]);
    }
    return ROBOHUB_OK;
#else
    (void)adc;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

static RoboHubError adc_op_deinit(RoboHubADC* self)
{
#ifdef RH_PICO_SDK
    (void)self;
    adc_run(false);
    return ROBOHUB_OK;
#else
    (void)self;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

static rh_u8 adc_op_channels(RoboHubADC* self)
{
    return ((RoboHubPicoADC*)self->context)->channel_count;
}

static rh_u8 adc_op_resolution_bits(RoboHubADC* self)
{
    return ((RoboHubPicoADC*)self->context)->resolution;
}

static RoboHubError adc_op_read(RoboHubADC* self, rh_u8 channel, rh_f32* level)
{
    RoboHubPicoADC* adc = (RoboHubPicoADC*)self->context;
    if (level == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (channel >= adc->channel_count) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    adc_select_input((uint)channel);
    adc_run(true);
    adc_fifo_drain();
    uint16_t raw = adc_read();
    adc_run(false);
    rh_u32 full = (rh_u32)((1u << adc->resolution) - 1u);
    *level = (rh_f32)raw / (rh_f32)full;
    return ROBOHUB_OK;
#else
    (void)channel;
    *level = 0.0f;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

void pico_adc_create(RoboHubPicoADC* adc, rh_name name, const rh_u32* gpios, rh_u8 count)
{
    if (adc == NULL) {
        return;
    }
    adc->channel_count = (count > ROBOHUB_PICO_ADC_MAX_CHANNELS) ? ROBOHUB_PICO_ADC_MAX_CHANNELS : count;
    adc->resolution = 12u;
    for (rh_u8 i = 0; i < adc->channel_count; i++) {
        adc->gpio[i] = (gpios != NULL) ? gpios[i] : 0u;
    }

    adc->interface.name = name;
    adc->interface.context = adc;
    adc->interface.init = adc_op_init;
    adc->interface.deinit = adc_op_deinit;
    adc->interface.channels = adc_op_channels;
    adc->interface.resolution_bits = adc_op_resolution_bits;
    adc->interface.read = adc_op_read;
}
