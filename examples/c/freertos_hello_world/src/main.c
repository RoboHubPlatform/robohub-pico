/**
 * @file main.c
 * @brief freertos_hello_world — hello, world, in a FreeRTOS task.
 *
 * The program creates one task through the RoboHub OSAL, writes a line from
 * it every second and toggles the board's LED. The OSAL is the same contract
 * the bare-metal examples use; `robohub-freertos` supplies the strong
 * definitions that turn the thread and the delay into FreeRTOS ones.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdint.h>

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "robohub/hal/console.h"
#include "robohub/hal/gpio.h"
#include "robohub/hal/timer.h"
#include "robohub/osal.h"

#define BSP_LED_NAME "user_led"
#define HELLO_STACK_BYTES 2048u
/* The board's USB-CDC pump runs above the ordinary task priorities. The task
 * that writes to it must not outrank it: a console write waits for the USB
 * stack to drain, and a writer that cannot be preempted by the stack it waits
 * on is a deadlock. */
#define HELLO_PRIORITY 1u

static RoboHubGPIO* led;
static RoboHubTimer* system_timer;
static rh_u32 seconds;

static void emit(const char* text)
{
    rh_size length = 0;
    while (text[length] != '\0') {
        length++;
    }
    /* Raw, not buffered: a line written from a task must reach the host even
     * when the task then blocks, and the line ends in a newline anyway. */
    (void)robohub_console_write_raw((const uint8_t*)text, length);
}

static void emit_u32(rh_u32 value)
{
    char digits[12];
    rh_u32 count = 0;

    if (value == 0u) {
        digits[count++] = '0';
    }
    while (value > 0u) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    char ordered[12];
    rh_u32 length = 0;
    while (count > 0u) {
        ordered[length++] = digits[--count];
    }
    (void)robohub_console_write_raw((const uint8_t*)ordered, length);
}

static void hello_task(void* argument)
{
    (void)argument;

    /* Let the USB stack enumerate and the board's pump task run before the
     * first write: a console opened after the board boots still has to see the
     * banner, and a write before there is anywhere to put it would block. */
    robohub_osal_sleep_ms(500u);

    emit("\nHello world, from FreeRTOS\n");
    emit("a task says something every second\n");
    emit("----------------------------------\n");

    for (;;) {
        seconds++;

        emit("[");
        emit_u32(seconds);
        emit(" s] hello from a FreeRTOS task\n");

        if (led != 0 && led->write != 0) {
            (void)led->write(led, 0, seconds % 2u);
        }

        robohub_osal_sleep_ms(1000u);
    }
}

/* Called before the scheduler is running, so the delay is the board's timer
 * and not the OSAL: the OSAL's delay is a kernel call, and there is no kernel
 * to call yet. */
static void halt_with(const char* message)
{
    for (;;) {
        emit(message);
        emit("\n");
        if (system_timer != 0 && system_timer->delay_ms != 0) {
            system_timer->delay_ms(system_timer, 2000u);
        }
    }
}

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        for (;;) {
        }
    }

    led = robohub_get_gpio(BSP_LED_NAME);
    system_timer = robohub_get_timer("system_timer");

    if (!robohub_osal_rtos_present()) {
        halt_with("no RTOS is linked; this example needs robohub-freertos");
    }

    robohub_thread_t thread = 0;
    const robohub_osal_thread_config config = {
        .name = "hello",
        .entry = hello_task,
        .arg = 0,
        .stack_size = HELLO_STACK_BYTES,
        .priority = HELLO_PRIORITY,
    };

    if (robohub_osal_thread_create(&thread, &config) != 0) {
        halt_with("could not create the hello task");
    }

    robohub_osal_start();

    /* A kernel that came back did not start. Keep the board alive rather than
     * falling out of main, which would leave nothing to talk to. */
    for (;;) {
        if (system_timer != 0 && system_timer->delay_ms != 0) {
            system_timer->delay_ms(system_timer, 1000u);
        }
    }
}
