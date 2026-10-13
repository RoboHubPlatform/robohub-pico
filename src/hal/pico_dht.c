/*
 * pico_dht.c — DHT single-wire HAL implementation for Pico boards.
 *
 * The line is an output driven low to request a measurement, and an input
 * otherwise, with the pull-up returning it high. The 40 bits that follow are
 * each a low pulse and a high pulse whose length is the bit: the timings below
 * are the windows a DHT11 and DHT22 are specified against, and the sample a
 * little past the shortest high is what tells a one from a zero.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_dht.h"

#include <stddef.h>

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#endif

/* -------------------------------------------------------------------------- */
/*  Timing                                                                    */
/* -------------------------------------------------------------------------- */
/* The start signal: hold the line low long enough for the slowest part to see
 * it, then release. */
#define DHT_START_LOW_US      19000u
#define DHT_START_RELEASE_US     30u
/* Each bit is a low pulse followed by a high pulse. Sampling this far into the
 * high pulse reads a one for a ~70 us pulse and a zero for a ~26 us one, which
 * is the widest margin between the two. */
#define DHT_BIT_SAMPLE_US        40u

/* A spin bounded so a part that is absent or dead is a timeout and not a hang.
 * The bound is far longer than any window above and far shorter than a second. */
#define DHT_SPIN_LIMIT 400000u

/* -------------------------------------------------------------------------- */
/*  Wire primitives                                                           */
/* -------------------------------------------------------------------------- */

#ifdef RH_PICO_SDK

static RoboHubPicoDHT* dht(RoboHubDHT* self)
{
    return (RoboHubPicoDHT*)self->context;
}

/* Drive the line low: set the output value first, so switching the direction
 * cannot glitch a high onto a line the part is driving. */
static inline void dht_pull_low(RoboHubPicoDHT* bus)
{
    gpio_put(bus->pin, 0);
    gpio_set_dir(bus->pin, GPIO_OUT);
}

/* Release the line: it becomes an input again and the pull-up returns it high.
 * The input buffer is enabled explicitly, because flipping the direction alone
 * does not reliably make the pad read the line again on the RP2350. */
static inline void dht_release(RoboHubPicoDHT* bus)
{
    gpio_set_dir(bus->pin, GPIO_IN);
    gpio_set_input_enabled(bus->pin, true);
    gpio_pull_up(bus->pin);
}

static inline rh_u32 dht_pin(RoboHubPicoDHT* bus)
{
    return (rh_u32)gpio_get(bus->pin);
}

/* Spin until the line reads `level`, or give up. The read of the pad is the
 * only work in the loop, so a window of tens of microseconds is a handful of
 * iterations and the bound is reached only when the part is not there. */
static rh_bool dht_wait_for(RoboHubPicoDHT* bus, rh_u32 level)
{
    for (rh_u32 i = 0; i < DHT_SPIN_LIMIT; i++) {
        if (dht_pin(bus) == level) {
            return RH_TRUE;
        }
    }
    return RH_FALSE;
}

#endif /* RH_PICO_SDK */

/* -------------------------------------------------------------------------- */
/*  Instance setup                                                            */
/* -------------------------------------------------------------------------- */

void pico_dht_create(RoboHubPicoDHT* bus, rh_name name, rh_u32 pin)
{
    if (bus == NULL) {
        return;
    }
    bus->pin = pin;
    bus->interface.name = name;
    bus->interface.context = bus;
    bus->interface.init = pico_dht_init;
    bus->interface.deinit = pico_dht_deinit;
    bus->interface.read = pico_dht_read;
}

/* -------------------------------------------------------------------------- */
/*  HAL operations                                                            */
/* -------------------------------------------------------------------------- */

RoboHubError pico_dht_init(RoboHubDHT* self)
{
    (void)self;
#ifdef RH_PICO_SDK
    RoboHubPicoDHT* bus = dht(self);
    gpio_init(bus->pin);
    gpio_set_dir(bus->pin, GPIO_IN);
    gpio_set_input_enabled(bus->pin, true);
    gpio_pull_up(bus->pin);
    /* Let the pull-up settle before the first transaction. The part's own
     * power-up delay belongs to the driver that knows the part. */
    sleep_ms(2);
    return ROBOHUB_OK;
#else
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

RoboHubError pico_dht_deinit(RoboHubDHT* self)
{
    (void)self;
#ifdef RH_PICO_SDK
    RoboHubPicoDHT* bus = dht(self);
    gpio_set_dir(bus->pin, GPIO_IN);
    gpio_disable_pulls(bus->pin);
    return ROBOHUB_OK;
#else
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

RoboHubError pico_dht_read(RoboHubDHT* self, rh_u8* data, rh_size len)
{
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    RoboHubPicoDHT* bus = dht(self);

    /* The start signal: hold low, then release and let the part answer. */
    dht_pull_low(bus);
    busy_wait_us_32(DHT_START_LOW_US);
    dht_release(bus);
    busy_wait_us_32(DHT_START_RELEASE_US);

    /* The answer: the part pulls low, then high, before the first bit. A part
     * that never pulls low is not there. */
    if (!dht_wait_for(bus, 0u) || !dht_wait_for(bus, 1u)) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    /* Then the first bit's low pulse begins. */
    if (!dht_wait_for(bus, 0u)) {
        return ROBOHUB_ERR_PROTOCOL;
    }

    for (rh_size byte = 0; byte < len; byte++) {
        rh_u8 value = 0u;
        for (rh_u32 bit = 0; bit < 8u; bit++) {
            /* The low pulse ends; the high pulse whose length is the bit begins. */
            if (!dht_wait_for(bus, 1u)) {
                return ROBOHUB_ERR_PROTOCOL;
            }
            busy_wait_us_32(DHT_BIT_SAMPLE_US);
            rh_u32 level = dht_pin(bus);
            value = (rh_u8)((value << 1) | (rh_u8)(level & 0x01u));
            if (level != 0u) {
                /* Still high at the sample: wait out the rest of the pulse. A
                 * low pulse has already ended by the sample, so only a one is
                 * waited on here. */
                if (!dht_wait_for(bus, 0u)) {
                    return ROBOHUB_ERR_PROTOCOL;
                }
            }
        }
        data[byte] = value;
    }
    return ROBOHUB_OK;
#else
    (void)self;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}
