/*
 * pico_console.c — Console contract for Pico boards.
 *
 * The console is the SDK's stdio. Which transport carries it — the USB CDC
 * device, the UART pins, both — is a board's decision, declared as a
 * configuration symbol and realised by the SDK drivers linked in; this file
 * only says that the platform's console writes there too, so a `printf` in a
 * program and a write through this contract reach the same place.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico_console.h"
#include "bsp.h"

#include <stdio.h>

#ifdef RH_PICO_SDK
#include "pico/stdlib.h"
#endif

RoboHubConsole robohub_console;

RoboHubConsoleError robohub_console_init(void)
{
    if (robohub_console.ready) {
        return ROBOHUB_CONSOLE_OK;
    }
#ifdef RH_PICO_SDK
    /* Brings up every transport the board enabled, including the USB one:
     * the SDK owns the descriptors, the device stack and the polling that
     * keeps it enumerated. */
    stdio_init_all();
#endif
    robohub_console.ready = 1u;
    robohub_console.written = 0u;
    return ROBOHUB_CONSOLE_OK;
}

RoboHubConsoleError robohub_console_write(const char* data, size_t len)
{
    if (data == NULL || len == 0) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    if (!robohub_console.ready) {
        return ROBOHUB_CONSOLE_ERR_STATE;
    }
#ifdef RH_PICO_SDK
    (void)fwrite(data, 1, len, stdout);
#else
    (void)fwrite(data, 1, len, stdout);
#endif
    robohub_console.written += (uint64_t)len;
    return ROBOHUB_CONSOLE_OK;
}

RoboHubConsoleError robohub_console_puts(const char* text)
{
    if (text == NULL) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    size_t len = 0;
    while (text[len] != '\0') {
        len++;
    }
    return robohub_console_write(text, len);
}

RoboHubConsoleError robohub_console_write_raw(const uint8_t* data, size_t len)
{
    if (data == NULL || len == 0) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    if (!robohub_console.ready) {
        return ROBOHUB_CONSOLE_ERR_STATE;
    }
#ifdef RH_PICO_SDK
    /* `putchar_raw`, not `fwrite`: stdio would buffer a frame and could rewrite
     * its ends, and a frame that arrives changed is a frame that fails its
     * check. */
    for (size_t i = 0; i < len; i++) {
        (void)putchar_raw((int)data[i]);
    }
#else
    (void)fwrite(data, 1, len, stdout);
#endif
    robohub_console.written += (uint64_t)len;
    return ROBOHUB_CONSOLE_OK;
}

int robohub_console_read_byte(void)
{
#ifdef RH_PICO_SDK
    int byte = getchar_timeout_us(0);
    if (byte == PICO_ERROR_TIMEOUT) {
        return -1;
    }
    return byte;
#else
    int byte = getchar();
    return byte;
#endif
}

/* The SDK's own background servicing keeps its transports alive, so there is
 * nothing for a program to drive here. The symbol exists because a program
 * that polls its console should not have to know which transport it got. */
void robohub_console_task(void)
{
}
