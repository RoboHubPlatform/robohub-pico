/*
 * pico_onewire.h — 1-Wire HAL implementation for Pico boards.
 *
 * The bus is one GPIO driven open-drain: the line is pulled low to signal and
 * released to let the pull-up return it high. The RP2350 has no 1-Wire
 * peripheral, so the timing is produced by the core, in a critical section, at
 * the clock the board runs at.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_ONEWIRE_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_ONEWIRE_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/onewire.h"

/* -------------------------------------------------------------------------- */
/*  Instance                                                                  */
/* -------------------------------------------------------------------------- */

/* A Pico 1-Wire bus is the interface plus the pin it is wired to. The interface
 * is first, so a pointer to the interface a driver holds is a pointer to the
 * bus the platform knows, the way a platform's other instances are. */
typedef struct RoboHubPicoOneWire {
    RoboHubOneWire interface;
    rh_u32         pin;
} RoboHubPicoOneWire;

/* Fill in an instance for a pin. The pin's pull-up is the board's — usually a
 * 4.7 kOhm resistor to 3V3 — and the internal pull-up is enabled as a fallback
 * so a bare line still reads high between transactions. */
void pico_onewire_create(RoboHubPicoOneWire* bus, rh_name name, rh_u32 pin);

/* -------------------------------------------------------------------------- */
/*  HAL operations                                                            */
/* -------------------------------------------------------------------------- */

RoboHubError pico_onewire_init(RoboHubOneWire* self);
RoboHubError pico_onewire_deinit(RoboHubOneWire* self);
RoboHubError pico_onewire_reset(RoboHubOneWire* self, rh_bool* present);
RoboHubError pico_onewire_write_byte(RoboHubOneWire* self, rh_u8 byte);
RoboHubError pico_onewire_read_byte(RoboHubOneWire* self, rh_u8* byte);
RoboHubError pico_onewire_write(RoboHubOneWire* self, const rh_u8* data, rh_size len);
RoboHubError pico_onewire_read(RoboHubOneWire* self, rh_u8* data, rh_size len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_ONEWIRE_H */
