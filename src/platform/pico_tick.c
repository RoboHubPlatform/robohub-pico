/*
 * pico_tick.c — System tick and delay.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_timer.h"
#include "pico_console.h"
#include "bsp.h"

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#include "hardware/timer.h"
#endif

#if defined(RH_CONSOLE_USB) && RH_CONSOLE_USB
#define ROBOHUB_CONSOLE_ON_USB 1
#else
#define ROBOHUB_CONSOLE_ON_USB 0
#endif

uint64_t pico_timer_get_millis(RoboHubTimer* self)
{
    (void)self;
#ifdef RH_PICO_SDK
    return to_ms_since_boot(get_absolute_time());
#else
    return 0;
#endif
}

uint64_t pico_timer_get_micros(RoboHubTimer* self)
{
    (void)self;
#ifdef RH_PICO_SDK
    return to_us_since_boot(get_absolute_time());
#else
    return 0;
#endif
}

void pico_timer_delay_ms(RoboHubTimer* self, uint32_t ms)
{
    (void)self;
#ifdef RH_PICO_SDK
#if ROBOHUB_CONSOLE_ON_USB
    /* The USB device stack is polled, so a delay is the one place a sleeping
     * program passes through often enough to keep it serviced. A board that
     * prints over its UART has nothing to service and sleeps as before. */
    absolute_time_t deadline = make_timeout_time_ms(ms);
    while (!time_reached(deadline)) {
        robohub_console_task();
        tight_loop_contents();
    }
#else
    sleep_ms(ms);
#endif
#endif
}

void pico_timer_delay_us(RoboHubTimer* self, uint32_t us)
{
    (void)self;
#ifdef RH_PICO_SDK
    sleep_us(us);
#endif
}
