/*
 * pico_fault.c — Default fault handlers.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdint.h>

void pico_fault_handler(const char* message)
{
    while (1) {
        /* Spin: platform-specific reset or blink */
    }
}

void pico_hard_fault_handler(void)
{
    pico_fault_handler("Hard Fault");
}
