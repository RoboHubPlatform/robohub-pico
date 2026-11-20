/**
 * @file main.c
 * @brief update_check — updates over the link on a Pico 2 W (PICO-0011).
 *
 * See module.yml. The USB serial port is the link: frames only, no text. The
 * build says which version this is (CONFIG_UPDATE_CHECK_VERSION) and whether it
 * hangs when it runs on trial (CONFIG_UPDATE_CHECK_HANG), as update_check.py
 * builds it.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/watchdog.h"

#include "platform.h"
#include "registry.h"
#include "robohub/managed.h"
#include "robohub/transport.h"
#include "robohub/update_slot.h"
#include "robohub/core/core.h"
#include "robohub/core/scheduler.h"
#include "update_key.h"

#if defined(__has_include)
#  if __has_include("autoconf.h")
#    include "autoconf.h"
#  endif
#endif
#if defined(CONFIG_UPDATE_CHECK_VERSION)
#define UPDATE_CHECK_VERSION CONFIG_UPDATE_CHECK_VERSION
#else
#define UPDATE_CHECK_VERSION 1
#endif
#if defined(CONFIG_UPDATE_CHECK_HANG) && CONFIG_UPDATE_CHECK_HANG
#define UPDATE_CHECK_HANG 1
#endif

static const rh_u8 uid[8] = { 0x55, 0x50, 0x44, 0x41, 0x54, 0x45, 0x00, 0x01 };
static RoboHubTransport link;

static int usb_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    /* One raw write per frame: no CR/LF translation, no byte at a time. */
    (void)stdio_put_string((const char*)data, (int)length, false, false);
    stdio_flush();
    return 0;
}

static RoboHubError link_task(robohub_core_task_id id, void* user)
{
    (void)id; (void)user;
    rh_u8 buffer[64];
    rh_size n = 0;
    int c;
    while (n < sizeof buffer && (c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        buffer[n++] = (rh_u8)c;
    }
    if (n > 0u) {
        robohub_transport_feed(&link, buffer, n);
    }
#if defined(UPDATE_CHECK_HANG)
    if (rh_update_pending(0)) {
        for (volatile uint32_t spin = 0;; spin++) {
        }
    }
#endif
    return ROBOHUB_OK;
}

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }
    /* Until the script opens the port (a USB forwarder may take a while to
     * bring it back after a reboot): what is said before is lost. The boot ROM's
     * trial watchdog is fed meanwhile, for at most 30 s. */
    for (int i = 0; i < 300 && !stdio_usb_connected(); i++) {
        watchdog_update();
        sleep_ms(100);
    }
    sleep_ms(200);

    const RoboHubTransportPhy phy = { .write = usb_write, .context = 0 };
    (void)robohub_transport_init(&link, &phy);
    RoboHubManagedConfig config = {
        .transport = &link,
        .uid = uid,
        .update_key = TEST_UPDATE_PUBLIC_KEY,
        .firmware_version = UPDATE_CHECK_VERSION,
        .confirm_ms = 5000u,
        .trial_ms = 60000u,
    };
    (void)robohub_managed_init(&config);
    if (robohub_core_init(NULL) != ROBOHUB_OK) {
        return 1;
    }
    /* HEALTH once, at once on this plain link: the script sees the board is up. */
    (void)robohub_transport_report_health(&link, 1u, 1u, UPDATE_CHECK_VERSION);
    robohub_core_task_id task;
    (void)robohub_core_task_add("link", link_task, 0, 5u, &task);
    (void)robohub_core_run();
    return 0;
}
