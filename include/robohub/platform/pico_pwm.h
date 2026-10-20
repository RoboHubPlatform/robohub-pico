/*
 * pico_pwm.h — PWM interface implementation for Pico boards.
 *
 * The RP2350's PWM is a set of slices, each with two channels, and the SDK maps
 * a pin to the slice and channel that can drive it. This binds the HAL's
 * contract — a duty from 0 to 1 at a frequency — to the pins a board wired.
 *
 * A frequency is a divider and a wrap: the counter runs at the system clock
 * divided by the divider, and reaches the wrap before starting again, so the
 * frequency is the clock over the divider and the wrap together. The divider is
 * fixed and the wrap is what moves, which keeps a frequency exact to the
 * divider's resolution and leaves the duty a fraction of the wrap.
 *
 * Two channels of one slice share that slice's counter, so they share its
 * frequency. A board that wants two different frequencies wants two slices, and
 * a peripheral with one frequency is what this is.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_PWM_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/pwm.h"

/** How many channels one peripheral carries. */
#define ROBOHUB_PICO_PWM_MAX_CHANNELS 8u

/* A Pico PWM line is the interface plus the pins its channels are on. */
typedef struct RoboHubPicoPWM {
    RoboHubPWM interface;
    rh_u8     channel_count;
    rh_u32    gpio[ROBOHUB_PICO_PWM_MAX_CHANNELS];
    rh_u32    slice[ROBOHUB_PICO_PWM_MAX_CHANNELS];
    rh_u8     chan[ROBOHUB_PICO_PWM_MAX_CHANNELS];
    rh_u32    frequency_hz;                          /* as last asked for */
    rh_u16    wrap;                                 /* the counter's wrap */
    rh_f32    duty[ROBOHUB_PICO_PWM_MAX_CHANNELS];  /* as last asked for */
} RoboHubPicoPWM;

/* Fill in an instance for a set of pins, in channel order. The pins must be ones
 * the PWM can drive; the SDK says which slice and channel each is. */
void pico_pwm_create(RoboHubPicoPWM* pwm, rh_name name, const rh_u32* gpios, rh_u8 count);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_PWM_H */
