/*
 * pico_dht.h — DHT single-wire HAL implementation for Pico boards.
 *
 * A DHT is one GPIO driven open-drain: the line is pulled low to signal and
 * released to let the pull-up return it high. The RP2350 has no DHT peripheral,
 * so the timing is produced by the core, in a critical section, at the clock the
 * board runs at — the same way the 1-Wire bus here is.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_DHT_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_DHT_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/dht.h"

/* -------------------------------------------------------------------------- */
/*  Instance                                                                  */
/* -------------------------------------------------------------------------- */

/* A Pico DHT line is the interface plus the pin it is wired to. The interface is
 * first, so a pointer to the interface a driver holds is a pointer to the line
 * the platform knows, the way a platform's other instances are. */
typedef struct RoboHubPicoDHT {
    RoboHubDHT interface;
    rh_u32     pin;
} RoboHubPicoDHT;

/* Fill in an instance for a pin. The part needs a pull-up on the data line —
 * usually 4.7 kOhm to 3V3 — and the internal pull-up is enabled as a fallback so
 * a bare line still reads high between transactions. */
void pico_dht_create(RoboHubPicoDHT* bus, rh_name name, rh_u32 pin);

/* -------------------------------------------------------------------------- */
/*  HAL operations                                                            */
/* -------------------------------------------------------------------------- */

RoboHubError pico_dht_init(RoboHubDHT* self);
RoboHubError pico_dht_deinit(RoboHubDHT* self);
RoboHubError pico_dht_read(RoboHubDHT* self, rh_u8* data, rh_size len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_DHT_H */
