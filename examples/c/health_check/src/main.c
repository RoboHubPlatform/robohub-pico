/**
 * @file main.c
 * @brief health_check — the health contracts on a Pico 2 W (PICO-0010).
 *
 * See module.yml: three boots, a watchdog reset, a recorded fault, a fed
 * watchdog. The watchdog is the core's (CONFIG_RH_CORE_WATCHDOG_MS=3000).
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "robohub/hal/console.h"
#include "robohub/hal/timer.h"
#include "robohub/health.h"
#include "robohub/core/core.h"
#include "robohub/core/health.h"
#include "robohub/core/scheduler.h"

static void put(const char* text)
{
    (void)robohub_console_puts(text);
}

static void put_u32(uint32_t value)
{
    char digits[12];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value > 0u);
    char out[12];
    uint32_t at = 0;
    while (count > 0u) {
        out[at++] = digits[--count];
    }
    out[at] = '\0';
    put(out);
}

static void put_hex(uint32_t value)
{
    static const char hex[] = "0123456789abcdef";
    char out[11] = "0x";
    for (int i = 0; i < 8; i++) {
        out[2 + i] = hex[(value >> (28 - 4 * i)) & 0xFu];
    }
    out[10] = '\0';
    put(out);
}

static robohub_core_health health;
static robohub_core_task_id hang_task, fault_task, beat_task;

/* Step 1: after a second, a task stops returning. The loop stops, nothing feeds
 * the watchdog, and the board resets one period later. */
static RoboHubError hang(robohub_core_task_id id, void* user)
{
    (void)id; (void)user;
    if (robohub_core_uptime_ms() < 1000u) {
        return ROBOHUB_OK;
    }
    put("health-check: step 1 — task ");
    put_u32(id);
    put(" hangs now; the watchdog should reset the board within 3 s\n");
    for (volatile uint32_t spin = 0;; spin++) {
    }
    return ROBOHUB_OK;
}

/* Step 2: after a second, a task writes where nothing is. */
static RoboHubError fault(robohub_core_task_id id, void* user)
{
    (void)id; (void)user;
    if (robohub_core_uptime_ms() < 1000u) {
        return ROBOHUB_OK;
    }
    put("health-check: step 2 — task ");
    put_u32(id);
    put(" faults now; the board should record it and restart\n");
    *(volatile uint32_t*)0xF0000000u = 1u;
    return ROBOHUB_OK;
}

/* Step 3: tasks keep their time and the loop keeps the watchdog fed. */
static RoboHubError beat(robohub_core_task_id id, void* user)
{
    (void)id; (void)user;
    robohub_core_health now;
    robohub_core_health_get(&now);
    uint32_t seconds = robohub_core_uptime_ms() / 1000u;
    /* The whole chain again, for a console opened late: boot 3 means both
     * resets kept the retained record. */
    put("health-check: boot ");
    put_u32(health.boot_count);
    put(", reset reason ");
    put(rh_reset_reason_name(health.reset_reason));
    put(", fault code ");
    put_u32(health.fault.code);
    put(" in task ");
    put_u32(health.fault.task);
    put(" at ");
    put_hex(health.fault.pc);
    put("\n");
    put("health-check: alive ");
    put_u32(seconds);
    put(" s, watchdog fed ");
    put_u32(now.watchdog_feeds);
    put(" times\n");
    if (seconds >= 20u) {
        put(now.watchdog_ms != 0u && now.watchdog_feeds > 100u
                ? "health-check: passed\n"
                : "health-check: FAILED — the watchdog was not running or not fed\n");
    }
    return ROBOHUB_OK;
}

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }
    RoboHubTimer* timer = robohub_get_timer("system_timer");
    /* Time for the USB console to be opened after a reset. */
    for (int i = 0; i < 30 && timer != NULL; i++) {
        timer->delay_ms(timer, 100u);
    }

    if (robohub_core_init(NULL) != ROBOHUB_OK) {
        put("health-check: FAILED — the core did not start\n");
        return 1;
    }
    robohub_core_health_get(&health);
    put("\nhealth-check: boot ");
    put_u32(health.boot_count);
    put(", reset reason ");
    put(rh_reset_reason_name(health.reset_reason));
    put("\n");
    if (health.has_fault) {
        put("health-check: fault record — code ");
        put_u32(health.fault.code);
        put(", task ");
        put_u32(health.fault.task);
        put(", pc ");
        put_hex(health.fault.pc);
        put(", uptime ");
        put_u32(health.fault.uptime_ms);
        put(" ms\n");
    } else {
        put("health-check: no fault record\n");
    }

    if (health.reset_reason == RH_RESET_FAULT) {
        if (!health.has_fault || health.fault.code != RH_FAULT_HARD || health.fault.pc == 0u ||
            health.fault.task == RH_HEALTH_NO_TASK) {
            put("health-check: FAILED — a fault reset without the record it should have left\n");
        } else {
            put("health-check: step 3 — the fault was recorded; running with the watchdog fed\n");
        }
        (void)robohub_core_task_add("beat", beat, 0, 2000u, &beat_task);
    } else if (health.reset_reason == RH_RESET_WATCHDOG) {
        (void)robohub_core_task_add("fault", fault, 0, 100u, &fault_task);
    } else {
        (void)robohub_core_task_add("hang", hang, 0, 100u, &hang_task);
    }
    (void)robohub_core_run();
    return 0;
}
