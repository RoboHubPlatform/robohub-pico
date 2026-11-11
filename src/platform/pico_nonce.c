/**
 * @file pico_nonce.c
 * @brief The platform's nonce source: the SDK's random number generator.
 *
 * pico_rand mixes the ring oscillator's random bit, the bus performance counters
 * and the time since boot; it is the SDK's answer for a value that must not
 * repeat. See robohub_platform_nonce in platform.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "platform.h"

#include "pico/rand.h"

int robohub_platform_nonce(rh_u8* out, rh_size length)
{
    rh_size done = 0;
    while (done < length) {
        uint64_t value = get_rand_64();
        for (int i = 0; i < 8 && done < length; i++) {
            out[done++] = (rh_u8)(value >> (8 * i));
        }
    }
    return 0;
}
