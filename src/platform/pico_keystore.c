/**
 * @file pico_keystore.c
 * @brief The device keystore in the last two 4 KiB sectors of the Pico's flash.
 *
 * Slot A is the second-to-last sector, slot B the last one. The A/B logic, the
 * CRC32 and the sequence numbers are robohub-common's (robohub/keystore_core.h);
 * this file only reads, erases and programs one sector.
 *
 * Erasing or programming flash stops execute-in-place, so nothing may run from
 * flash meanwhile: no interrupt handler on this core, and no code on the other
 * one. The SDK's flash_safe_execute arranges both — it disables interrupts and,
 * when the other core is running, locks it out (which needs that core to have
 * called flash_safe_execute_core_init, or to be under an RTOS the SDK knows; if
 * neither holds it refuses and the store reports failure rather than crash).
 *
 * The two sectors are kept out of the image by a link-time check
 * (cmake/pico_keystore_reserve.ld): a program that grows into them fails to link
 * instead of having its tail erased by a key rotation.
 *
 * Reads go through the uncached, untranslated XIP window, so they see what the
 * flash holds right after a program, and see the physical sector even when the
 * running image sits in a translated partition.
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/keystore.h"
#include "robohub/keystore_core.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive; see keystore_core.h. */
// @req PICO-KEY-0005
const char rh_keystore_platform[] = "pico";

#ifdef RH_PICO_SDK

#include <string.h>

#include "pico.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"

#ifndef RH_KEYSTORE_FLASH_SIZE
#define RH_KEYSTORE_FLASH_SIZE PICO_FLASH_SIZE_BYTES
#endif

// @req PICO-KEY-0003
#if RH_KEYSTORE_FLASH_SIZE != PICO_FLASH_SIZE_BYTES
#error "the keystore's flash size disagrees with the board's PICO_FLASH_SIZE_BYTES; the linker reservation would be in the wrong place"
#endif

/** Offset of slot A from the start of flash; slot B is the sector after it. */
#define RH_KEYSTORE_OFFSET (RH_KEYSTORE_FLASH_SIZE - 2u * FLASH_SECTOR_SIZE)

#ifdef XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
#define RH_KEYSTORE_READ_BASE XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
#elif defined(XIP_NOCACHE_NOALLOC_BASE)
#define RH_KEYSTORE_READ_BASE XIP_NOCACHE_NOALLOC_BASE
#else
#define RH_KEYSTORE_READ_BASE XIP_BASE
#endif

/** How long to wait for the other core to be locked out, in milliseconds. */
#define RH_KEYSTORE_LOCKOUT_MS 100u

// @req PICO-KEY-0001
static uint32_t slot_offset(unsigned slot)
{
    return RH_KEYSTORE_OFFSET + (uint32_t)slot * FLASH_SECTOR_SIZE;
}

// @req PICO-KEY-0001
static int pico_read(void *ctx, unsigned slot, uint8_t *buf, size_t len)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS || len > FLASH_SECTOR_SIZE) {
        return -1;
    }
    memcpy(buf, (const void *)(uintptr_t)(RH_KEYSTORE_READ_BASE + slot_offset(slot)), len);
    return 0;
}

typedef struct {
    uint32_t offset;
    const uint8_t *page; /* NULL to erase */
} pico_flash_op_t;

static void pico_flash_op(void *param)
{
    const pico_flash_op_t *op = (const pico_flash_op_t *)param;
    if (op->page == 0) {
        flash_range_erase(op->offset, FLASH_SECTOR_SIZE);
    } else {
        flash_range_program(op->offset, op->page, FLASH_PAGE_SIZE);
    }
}

// @req PICO-KEY-0004
static int pico_erase(void *ctx, unsigned slot)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS) {
        return -1;
    }
    pico_flash_op_t op = { slot_offset(slot), 0 };
    return flash_safe_execute(pico_flash_op, &op, RH_KEYSTORE_LOCKOUT_MS) == PICO_OK ? 0 : -1;
}

// @req PICO-KEY-0004
static int pico_write(void *ctx, unsigned slot, const uint8_t *buf, size_t len)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS || len > FLASH_PAGE_SIZE) {
        return -1;
    }
    /* Flash is programmed a whole 256-byte page at a time; the rest stays erased. */
    static uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, buf, len);
    pico_flash_op_t op = { slot_offset(slot), page };
    int rc = flash_safe_execute(pico_flash_op, &op, RH_KEYSTORE_LOCKOUT_MS) == PICO_OK ? 0 : -1;
    memset(page, 0, sizeof(page));
    return rc;
}

static const rh_keystore_backend_t pico_backend = { 0, pico_read, pico_erase, pico_write };

// @req PICO-0004
int rh_keystore_load(rh_key_t *out)
{
    return rh_keystore_core_load(&pico_backend, out);
}

// @req PICO-0004
int rh_keystore_store(const rh_key_t *key)
{
    return rh_keystore_core_store(&pico_backend, key);
}

#endif /* RH_PICO_SDK */
