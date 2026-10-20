/*
 * pico_pwm.c — PWM interface implementation for Pico boards.
 *
 * The counter runs at the system clock over a fixed divider, and the wrap is
 * what sets the frequency: f = clock / (divider * (wrap + 1)). The divider is
 * fixed so that the wrap is the only thing that moves, which keeps a frequency
 * as exact as the divider allows and leaves the duty a plain fraction of it.
 *
 * The duty is written as a level against the wrap, so a duty is a fraction of
 * whatever the wrap is. That is why a change of frequency re-writes the duties:
 * a carrier that moved must not change how bright the LED is.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_pwm.h"

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

/* The divider the counter runs at. Fixed, so the wrap carries the frequency. */
#define PICO_PWM_DIVIDER 16.0f
/* The largest wrap the counter's register holds. */
#define PICO_PWM_WRAP_MAX 65535u
#endif

/* Write a channel's level, as a fraction of the current wrap. */
static void pwm_apply(RoboHubPicoPWM* pwm, rh_u8 channel)
{
#ifdef RH_PICO_SDK
    if (channel >= pwm->channel_count) {
        return;
    }
    rh_f32 duty = pwm->duty[channel];
    if (duty < 0.0f) {
        duty = 0.0f;
    }
    if (duty > 1.0f) {
        duty = 1.0f;
    }
    uint16_t level = (uint16_t)(duty * (rh_f32)pwm->wrap);
    pwm_set_gpio_level(pwm->gpio[channel], level);
#else
    (void)pwm;
    (void)channel;
#endif
}

static RoboHubError pwm_op_init(RoboHubPWM* self)
{
    RoboHubPicoPWM* pwm = (RoboHubPicoPWM*)self->context;
#ifdef RH_PICO_SDK
    for (rh_u8 i = 0; i < pwm->channel_count; i++) {
        pwm->slice[i] = pwm_gpio_to_slice_num(pwm->gpio[i]);
        pwm->chan[i] = (rh_u8)pwm_gpio_to_channel(pwm->gpio[i]);
    }
    /* One configuration for every slice: the wrap is the same for all of them,
     * because a peripheral has one frequency. */
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, (uint16_t)PICO_PWM_WRAP_MAX);
    for (rh_u8 i = 0; i < pwm->channel_count; i++) {
        pwm_init(pwm->slice[i], &config, true);
        pwm_set_gpio_level(pwm->gpio[i], 0);
        pwm->duty[i] = 0.0f;
    }
    pwm->wrap = (rh_u16)PICO_PWM_WRAP_MAX;
    pwm->frequency_hz = 0u;
    return ROBOHUB_OK;
#else
    (void)pwm;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

static RoboHubError pwm_op_deinit(RoboHubPWM* self)
{
    RoboHubPicoPWM* pwm = (RoboHubPicoPWM*)self->context;
#ifdef RH_PICO_SDK
    for (rh_u8 i = 0; i < pwm->channel_count; i++) {
        pwm_set_enabled(pwm->slice[i], false);
    }
    return ROBOHUB_OK;
#else
    (void)pwm;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

static rh_u8 pwm_op_channels(RoboHubPWM* self)
{
    return ((RoboHubPicoPWM*)self->context)->channel_count;
}

static RoboHubError pwm_op_set_frequency(RoboHubPWM* self, rh_u32 frequency_hz)
{
    RoboHubPicoPWM* pwm = (RoboHubPicoPWM*)self->context;
#ifdef RH_PICO_SDK
    if (frequency_hz == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    uint32_t clock = clock_get_hz(clk_sys);
    uint32_t counts = (uint32_t)((rh_f32)clock / (PICO_PWM_DIVIDER * (rh_f32)frequency_hz));
    if (counts < 2u) {
        /* Faster than the divider and the smallest wrap can express. */
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }
    if (counts > (PICO_PWM_WRAP_MAX + 1u)) {
        /* Slower than the divider and the largest wrap can express. */
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }
    pwm->wrap = (rh_u16)(counts - 1u);
    for (rh_u8 i = 0; i < pwm->channel_count; i++) {
        pwm_set_clkdiv(pwm->slice[i], PICO_PWM_DIVIDER);
        pwm_set_wrap(pwm->slice[i], pwm->wrap);
        pwm_set_enabled(pwm->slice[i], true);
        /* The level is a fraction of the wrap, which has just changed. */
        pwm_apply(pwm, i);
    }
    pwm->frequency_hz = frequency_hz;
    return ROBOHUB_OK;
#else
    (void)pwm;
    (void)frequency_hz;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

static RoboHubError pwm_op_set_duty(RoboHubPWM* self, rh_u8 channel, rh_f32 duty)
{
    RoboHubPicoPWM* pwm = (RoboHubPicoPWM*)self->context;
    if (channel >= pwm->channel_count) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (duty < 0.0f) {
        duty = 0.0f;
    }
    if (duty > 1.0f) {
        duty = 1.0f;
    }
    pwm->duty[channel] = duty;
    pwm_apply(pwm, channel);
    return ROBOHUB_OK;
}

static RoboHubError pwm_op_stop(RoboHubPWM* self, rh_u8 channel)
{
    RoboHubPicoPWM* pwm = (RoboHubPicoPWM*)self->context;
    if (channel >= pwm->channel_count) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    pwm->duty[channel] = 0.0f;
#ifdef RH_PICO_SDK
    pwm_set_gpio_level(pwm->gpio[channel], 0);
    return ROBOHUB_OK;
#else
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

void pico_pwm_create(RoboHubPicoPWM* pwm, rh_name name, const rh_u32* gpios, rh_u8 count)
{
    if (pwm == NULL) {
        return;
    }
    pwm->channel_count = (count > ROBOHUB_PICO_PWM_MAX_CHANNELS) ? ROBOHUB_PICO_PWM_MAX_CHANNELS : count;
    for (rh_u8 i = 0; i < pwm->channel_count; i++) {
        pwm->gpio[i] = (gpios != NULL) ? gpios[i] : 0u;
        pwm->slice[i] = 0u;
        pwm->chan[i] = 0u;
        pwm->duty[i] = 0.0f;
    }
    pwm->frequency_hz = 0u;
    pwm->wrap = 0u;

    pwm->interface.name = name;
    pwm->interface.context = pwm;
    pwm->interface.init = pwm_op_init;
    pwm->interface.deinit = pwm_op_deinit;
    pwm->interface.channels = pwm_op_channels;
    pwm->interface.set_frequency = pwm_op_set_frequency;
    pwm->interface.set_duty = pwm_op_set_duty;
    pwm->interface.stop = pwm_op_stop;
}
