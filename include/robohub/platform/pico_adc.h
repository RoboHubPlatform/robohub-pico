/*
 * pico_adc.h — ADC interface implementation for Pico boards.
 *
 * The RP2350's converter is a four-input, twelve-bit one, and the SDK's ADC
 * peripheral is what drives it. This binds the HAL's contract — a level from 0 to
 * 1 — to the pins the board wired to the converter's inputs.
 *
 * A conversion is a single reading: the input is selected, the converter is
 * started, and the result is taken. The level is the reading over full scale, so
 * a caller compares it and never asks how many bits there were.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_ADC_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/adc.h"

/** The converter has four inputs. */
#define ROBOHUB_PICO_ADC_MAX_CHANNELS 4u

/* A Pico ADC line is the interface plus the pins its inputs are on. */
typedef struct RoboHubPicoADC {
    RoboHubADC interface;
    rh_u8     channel_count;
    rh_u8     resolution;               /* bits */
    rh_u32    gpio[ROBOHUB_PICO_ADC_MAX_CHANNELS];
} RoboHubPicoADC;

/* Fill in an instance for a set of input pins, in channel order. The pins must
 * be the converter's inputs: GP26 to GP29 on this part. */
void pico_adc_create(RoboHubPicoADC* adc, rh_name name, const rh_u32* gpios, rh_u8 count);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_ADC_H */
