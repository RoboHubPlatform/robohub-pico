/*
 * pico_platform.c — Platform initialization.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_platform.h"
#include "registry.h"
#include "platform.h"

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#endif

void pico_system_clock_init(void)
{
#ifdef RH_PICO_SDK
#endif
}

RoboHubError pico_platform_init(void)
{
#ifdef RH_PICO_SDK
    setup_default_uart();
#endif
    return ROBOHUB_OK;
}
