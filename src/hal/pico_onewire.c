/*
 * pico_onewire.c — 1-Wire HAL implementation for Pico boards.
 *
 * The RP2350 has no 1-Wire controller, so the wire is produced in software on
 * one GPIO: the line is an output driven low to signal, and an input otherwise,
 * with the pull-up returning it high. The timings below are the canonical
 * standard-speed sequences a DS18B20 is specified against.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_onewire.h"

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#endif

/* -------------------------------------------------------------------------- */
/*  Timing                                                                    */
/* -------------------------------------------------------------------------- */
#define OW_RESET_LOW_US      480u   /* hold low at least 480 */
#define OW_PRESENCE_WAIT_US   70u   /* the part answers within 60 us of release */
#define OW_PRESENCE_TAIL_US  410u   /* then the reset cycle completes at 960 */
#define OW_WRITE1_LOW_US      10u   /* a one: brief low, then release */
#define OW_WRITE1_TAIL_US     55u
#define OW_WRITE0_LOW_US      65u   /* a zero: long low, brief release */
#define OW_WRITE0_TAIL_US      5u
#define OW_READ_LOW_US         3u   /* start a read slot and let the part drive */
#define OW_READ_SAMPLE_US     10u
#define OW_READ_TAIL_US       53u

/* -------------------------------------------------------------------------- */
/*  Wire primitives                                                           */
/* -------------------------------------------------------------------------- */

#ifdef RH_PICO_SDK

static RoboHubPicoOneWire* ow(RoboHubOneWire* self)
{
    return (RoboHubPicoOneWire*)self->context;
}

/* Drive the line low: set the output value first, so switching the direction
 * cannot glitch a high onto a line another device is holding. */
static inline void ow_pull_low(RoboHubPicoOneWire* bus)
{
    gpio_put(bus->pin, 0);
    gpio_set_dir(bus->pin, GPIO_OUT);
}

/* Release the line: it becomes an input again and the pull-up returns it high.
 *
 * The input buffer is enabled explicitly. Flipping the direction from output to
 * input alone does not reliably make the pad read the line again on the RP2350,
 * so a released line reads as the zero it was last driven as — a reset that
 * never sees a presence pulse. */
static inline void ow_release(RoboHubPicoOneWire* bus)
{
    gpio_set_dir(bus->pin, GPIO_IN);
    gpio_set_input_enabled(bus->pin, true);
    gpio_pull_up(bus->pin);
}

static inline rh_u32 ow_read_pin(RoboHubPicoOneWire* bus)
{
    return (rh_u32)gpio_get(bus->pin);
}

/* Busy-wait rather than sleep: a slot is a few microseconds and must not yield,
 * and the device's windows are measured from the edge that started the slot. */
static inline void ow_delay(rh_u32 us)
{
    busy_wait_us_32(us);
}

static void ow_write_bit(RoboHubPicoOneWire* bus, rh_u32 bit)
{
    if (bit != 0u) {
        ow_pull_low(bus);
        ow_delay(OW_WRITE1_LOW_US);
        ow_release(bus);
        ow_delay(OW_WRITE1_TAIL_US);
    } else {
        ow_pull_low(bus);
        ow_delay(OW_WRITE0_LOW_US);
        ow_release(bus);
        ow_delay(OW_WRITE0_TAIL_US);
    }
}

static rh_u32 ow_read_bit(RoboHubPicoOneWire* bus)
{
    ow_pull_low(bus);
    ow_delay(OW_READ_LOW_US);
    ow_release(bus);
    ow_delay(OW_READ_SAMPLE_US);
    rh_u32 bit = ow_read_pin(bus);
    ow_delay(OW_READ_TAIL_US);
    return bit & 0x01u;
}

#endif /* RH_PICO_SDK */

/* -------------------------------------------------------------------------- */
/*  Instance setup                                                            */
/* -------------------------------------------------------------------------- */

void pico_onewire_create(RoboHubPicoOneWire* bus, rh_name name, rh_u32 pin)
{
    if (bus == NULL) {
        return;
    }
    bus->pin = pin;
    bus->interface.name = name;
    bus->interface.context = bus;
    bus->interface.init = pico_onewire_init;
    bus->interface.deinit = pico_onewire_deinit;
    bus->interface.reset = pico_onewire_reset;
    bus->interface.write_byte = pico_onewire_write_byte;
    bus->interface.read_byte = pico_onewire_read_byte;
    bus->interface.write = pico_onewire_write;
    bus->interface.read = pico_onewire_read;
}

/* -------------------------------------------------------------------------- */
/*  HAL operations                                                            */
/* -------------------------------------------------------------------------- */

RoboHubError pico_onewire_init(RoboHubOneWire* self)
{
    (void)self;
#ifdef RH_PICO_SDK
    RoboHubPicoOneWire* bus = ow(self);
    gpio_init(bus->pin);
    gpio_set_dir(bus->pin, GPIO_IN);
    gpio_set_input_enabled(bus->pin, true);
    gpio_pull_up(bus->pin);
    /* Leave the line idle before the first transaction, so the pull-up has
     * settled and a part that powered up with the board is ready. */
    sleep_ms(2);
    return ROBOHUB_OK;
#else
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

RoboHubError pico_onewire_deinit(RoboHubOneWire* self)
{
    (void)self;
#ifdef RH_PICO_SDK
    RoboHubPicoOneWire* bus = ow(self);
    gpio_set_dir(bus->pin, GPIO_IN);
    gpio_disable_pulls(bus->pin);
    return ROBOHUB_OK;
#else
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

RoboHubError pico_onewire_reset(RoboHubOneWire* self, rh_bool* present)
{
    if (present == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    *present = RH_FALSE;
#ifdef RH_PICO_SDK
    RoboHubPicoOneWire* bus = ow(self);
    ow_pull_low(bus);
    ow_delay(OW_RESET_LOW_US);
    ow_release(bus);
    ow_delay(OW_PRESENCE_WAIT_US);
    *present = (ow_read_pin(bus) == 0u) ? RH_TRUE : RH_FALSE;
    ow_delay(OW_PRESENCE_TAIL_US);
    return ROBOHUB_OK;
#else
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

RoboHubError pico_onewire_write_byte(RoboHubOneWire* self, rh_u8 byte)
{
#ifdef RH_PICO_SDK
    RoboHubPicoOneWire* bus = ow(self);
    for (rh_u32 i = 0; i < 8u; i++) {
        ow_write_bit(bus, ((rh_u32)byte >> i) & 0x01u);
    }
    return ROBOHUB_OK;
#else
    (void)self; (void)byte;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

RoboHubError pico_onewire_read_byte(RoboHubOneWire* self, rh_u8* byte)
{
    if (byte == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_PICO_SDK
    RoboHubPicoOneWire* bus = ow(self);
    rh_u32 value = 0;
    for (rh_u32 i = 0; i < 8u; i++) {
        value |= ow_read_bit(bus) << i;
    }
    *byte = (rh_u8)value;
    return ROBOHUB_OK;
#else
    (void)self;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

RoboHubError pico_onewire_write(RoboHubOneWire* self, const rh_u8* data, rh_size len)
{
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    for (rh_size i = 0; i < len; i++) {
        RoboHubError err = pico_onewire_write_byte(self, data[i]);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
    }
    return ROBOHUB_OK;
}

RoboHubError pico_onewire_read(RoboHubOneWire* self, rh_u8* data, rh_size len)
{
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    for (rh_size i = 0; i < len; i++) {
        RoboHubError err = pico_onewire_read_byte(self, &data[i]);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
    }
    return ROBOHUB_OK;
}
