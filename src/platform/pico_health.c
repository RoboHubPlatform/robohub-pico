/**
 * @file pico_health.c
 * @brief robohub-common's health contracts on the RP2350 (PICO-0010).
 *
 * - **Watchdog**: the RP2350's watchdog, through the SDK (`watchdog_enable`,
 *   paused while a debugger halts the core). Its counter runs at 1 µs per tick
 *   for at most 0xFFFFFF ticks, so the longest period is 16 777 ms; a longer one
 *   is given that. It can be stopped (`watchdog_disable`), and it has no early
 *   warning interrupt: a hang leaves no record, only the reason.
 * - **Reset reason**: the SDK's own distinction first — a reboot the enabled
 *   watchdog caused is `watchdog`; a reboot through `watchdog_reboot` (a program,
 *   picotool, or this file's fault handler) is `software` — then POWMAN's
 *   CHIP_RESET flags: power-on, brown-out, the RUN pin.
 * - **Retained memory**: 32 bytes of uninitialised RAM (`.uninitialized_data`),
 *   which the boot ROM and the SDK's startup leave alone across a watchdog or
 *   software reboot and which power loss turns to noise.
 * - **Faults**: the HardFault handler records the faulting PC from the stacked
 *   exception frame and reboots through the watchdog, so the next boot reads a
 *   software reset with a record — a fault.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/health.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive over robohub-common's
 * weak defaults. */
// @req PICO-0010
const char rh_health_platform[] = "pico";

#ifdef RH_PICO_SDK

#include "pico.h"
#include "hardware/watchdog.h"
#include "hardware/structs/powman.h"

#define PICO_WATCHDOG_MAX_MS 16777u

static uint32_t __uninitialized_ram(pico_health_retained)[RH_HEALTH_RETAINED_SIZE / 4u] __attribute__((aligned(4)));

// @req PICO-0010
void rh_watchdog_caps_get(rh_watchdog_caps *caps)
{
    if (caps) {
        caps->present = true;
        caps->stoppable = true;
        caps->early_warning = false;
        caps->min_ms = 1u;
        caps->max_ms = PICO_WATCHDOG_MAX_MS;
    }
}

// @req PICO-0010
RoboHubError rh_watchdog_start(uint32_t period_ms, uint32_t *chosen_ms)
{
    if (period_ms == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    uint32_t chosen = period_ms > PICO_WATCHDOG_MAX_MS ? PICO_WATCHDOG_MAX_MS : period_ms;
    watchdog_enable(chosen, true);
    if (chosen_ms) {
        *chosen_ms = chosen;
    }
    return ROBOHUB_OK;
}

// @req PICO-0010
void rh_watchdog_feed(void)
{
    watchdog_update();
}

// @req PICO-0010
RoboHubError rh_watchdog_stop(void)
{
    watchdog_disable();
    return ROBOHUB_OK;
}

// @req PICO-0010
rh_reset_reason rh_platform_reset_reason(void)
{
    if (watchdog_enable_caused_reboot()) {
        return RH_RESET_WATCHDOG;
    }
    if (watchdog_caused_reboot()) {
        return RH_RESET_SOFTWARE;
    }
    uint32_t flags = powman_hw->chip_reset;
    if (flags & POWMAN_CHIP_RESET_HAD_POR_BITS) {
        return RH_RESET_POWER_ON;
    }
    if (flags & POWMAN_CHIP_RESET_HAD_BOR_BITS) {
        return RH_RESET_BROWN_OUT;
    }
    if (flags & POWMAN_CHIP_RESET_HAD_RUN_LOW_BITS) {
        return RH_RESET_PIN;
    }
    if (flags & (POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_SWCORE_BITS |
                 POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_BITS |
                 POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_ASYNC_BITS |
                 POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_PSM_BITS)) {
        return RH_RESET_WATCHDOG;
    }
    if (flags & POWMAN_CHIP_RESET_HAD_DP_RESET_REQ_BITS) {
        return RH_RESET_SOFTWARE; /* a debugger asked */
    }
    return RH_RESET_UNKNOWN;
}

// @req PICO-0010
void *rh_platform_retained(void)
{
    return pico_health_retained;
}

/* Called from the HardFault entry with the exception frame the core stacked:
 * r0 r1 r2 r3 r12 lr pc xpsr. Writes the record and reboots; nothing here can
 * fault again. */
void pico_health_hardfault(const uint32_t *frame)
{
    rh_health_fault(RH_FAULT_HARD, frame != 0 ? frame[6] : 0u);
    watchdog_reboot(0, 0, 0);
    for (;;) {
    }
}

/* The SDK's vector table names isr_hardfault weakly; this is the strong one.
 * EXC_RETURN bit 2 says which stack the frame is on. */
// @req PICO-0010
void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile(
        "movs r0, #4          \n"
        "mov  r1, lr          \n"
        "tst  r0, r1          \n"
        "beq  1f              \n"
        "mrs  r0, psp         \n"
        "b    2f              \n"
        "1:                   \n"
        "mrs  r0, msp         \n"
        "2:                   \n"
        "ldr  r1, =pico_health_hardfault \n"
        "bx   r1              \n");
}

#endif /* RH_PICO_SDK */
