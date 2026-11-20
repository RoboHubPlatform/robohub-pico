/**
 * @file pico_update.c
 * @brief robohub-common's update slot on the RP2350: the boot ROM's A/B
 *        partitions, flash update boot and try-before-you-buy (PICO-0011,
 *        robohub ADR-0023).
 *
 * The flash, as the partition table this platform ships lays it out
 * (partitions/ab.json, PICO-0012):
 *
 *   0x000000  8 kB     the partition table
 *   0x002000  2036 kB  partition A
 *   0x1FF000  2036 kB  partition B
 *   0x3FC000  4 kB     spare
 *   0x3FD000  4 kB     the update state sector (this file)
 *   0x3FE000  8 kB     the keystore (pico_keystore.c)
 *
 * The partitions' places are read from the boot ROM's partition table, not
 * assumed; a board without one — flashed before Phase 2c — is not updatable.
 *
 * - An image is written to the partition not booted, a page at a time.
 * - rh_update_boot_once() records the update in the state sector and reboots
 *   with a flash update boot to that partition's start. The image is flagged
 *   try-before-you-buy (an update image, PICO-0012), so the boot ROM enters it
 *   under its 16.7 s watchdog, which the core takes over and feeds only while
 *   every task keeps its time (datasheet §5.1.17: the timeout "can also be
 *   extended by reloading the watchdog counter").
 * - Pending: this boot is that image's trial (the boot ROM's BUY_PENDING flag).
 * - rh_update_confirm(): explicit_buy() — the image becomes the preferred one and
 *   the other partition's first sector is erased.
 * - rh_update_reject(), a hang, a crash, a power cut: any boot that is not the
 *   trial boots the other, untried image (a non-TBYB image is chosen over a TBYB
 *   one), and finds the state sector still naming the update: rolled back.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/update_slot.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive. */
// @req PICO-0011
const char rh_update_platform[] = "pico";

#ifdef RH_PICO_SDK

#include <string.h>

#include "pico.h"
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/regs/addressmap.h"
#include "boot/bootrom_constants.h"
#include "boot/picoboot_constants.h"
#include "boot/picobin.h"

#ifdef XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
#define PICO_UPDATE_READ_BASE XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
#else
#define PICO_UPDATE_READ_BASE XIP_NOCACHE_NOALLOC_BASE
#endif

/** The update state sector: the third sector from the end, before the keystore's two. */
#define PICO_UPDATE_STATE_OFFSET (PICO_FLASH_SIZE_BYTES - 3u * FLASH_SECTOR_SIZE)
#define PICO_UPDATE_LOCKOUT_MS 100u
#define PICO_UPDATE_MAGIC 0x52485550u /* "RHUP" */

typedef struct {
    uint32_t magic;
    uint32_t pending;           /* 1: an update was booted once and not settled */
    uint32_t pending_partition;
    rh_update_info info;
    uint32_t outcome;           /* rh_update_outcome, until taken */
    rh_update_info outcome_info;
    uint32_t security;
    uint32_t check;             /* a sum over the words above, inverted */
} pico_update_state;

static pico_update_state state;
static bool state_loaded;
static bool settled;            /* this boot's trial or rollback was worked out */

/* The 4 kB work area the boot ROM asks for (explicit_buy, load_partition_table). */
static uint8_t __attribute__((aligned(4))) rom_work[4096];

static uint32_t state_check(const pico_update_state *s)
{
    const uint32_t *w = (const uint32_t *)s;
    uint32_t sum = 0;
    for (size_t i = 0; i < offsetof(pico_update_state, check) / 4u; i++) {
        sum += w[i];
    }
    return ~sum;
}

typedef struct {
    uint32_t offset;
    const uint8_t *data;   /* NULL to erase a sector */
} pico_flash_op_t;

static void pico_flash_op(void *param)
{
    const pico_flash_op_t *op = (const pico_flash_op_t *)param;
    if (op->data == 0) {
        flash_range_erase(op->offset, FLASH_SECTOR_SIZE);
    } else {
        flash_range_program(op->offset, op->data, FLASH_PAGE_SIZE);
    }
}

static int flash_erase(uint32_t offset)
{
    pico_flash_op_t op = { offset, 0 };
    return flash_safe_execute(pico_flash_op, &op, PICO_UPDATE_LOCKOUT_MS) == PICO_OK ? 0 : -1;
}

static int flash_page(uint32_t offset, const uint8_t page[FLASH_PAGE_SIZE])
{
    pico_flash_op_t op = { offset, page };
    return flash_safe_execute(pico_flash_op, &op, PICO_UPDATE_LOCKOUT_MS) == PICO_OK ? 0 : -1;
}

static int state_save(void)
{
    static uint8_t page[FLASH_PAGE_SIZE];
    state.magic = PICO_UPDATE_MAGIC;
    state.check = state_check(&state);
    memset(page, 0xFF, sizeof page);
    memcpy(page, &state, sizeof state);
    if (flash_erase(PICO_UPDATE_STATE_OFFSET) != 0 || flash_page(PICO_UPDATE_STATE_OFFSET, page) != 0) {
        return -1;
    }
    return 0;
}

static void state_load(void)
{
    if (state_loaded) {
        return;
    }
    state_loaded = true;
    memcpy(&state, (const void *)(uintptr_t)(PICO_UPDATE_READ_BASE + PICO_UPDATE_STATE_OFFSET), sizeof state);
    if (state.magic != PICO_UPDATE_MAGIC || state.check != state_check(&state)) {
        memset(&state, 0, sizeof state); /* erased, or never written */
    }
}

/* -------------------------------------------------------------------------- */
/*  Partitions                                                                */
/* -------------------------------------------------------------------------- */

static int booted_partition(bool *trial)
{
    boot_info_t info;
    if (!rom_get_boot_info(&info)) {
        return -1;
    }
    if (trial) {
        *trial = (info.tbyb_and_update_info & BOOT_TBYB_AND_UPDATE_FLAG_BUY_PENDING) != 0;
    }
    return info.partition;
}

/* A partition's place in flash, from the boot ROM's partition table. */
static bool partition_place(int partition, uint32_t *offset, uint32_t *size)
{
    if (partition < 0 || partition > 1) {
        return false;
    }
    uint32_t words[3];
    int n = rom_get_partition_table_info(words, 3, PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION |
                                                     ((uint32_t)partition << 24));
    if (n < 0) {
        /* After a watchdog boot the table is not resident
         * (BOOTROM_ERROR_PRECONDITION_NOT_MET): load it, and ask again. */
        (void)rom_load_partition_table(rom_work, sizeof rom_work, false);
        n = rom_get_partition_table_info(words, 3, PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION |
                                                     ((uint32_t)partition << 24));
    }
    if (n < 3 || (words[0] & PT_INFO_PARTITION_LOCATION_AND_FLAGS) == 0) {
        return false;
    }
    uint32_t first = (words[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
    uint32_t last = (words[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
    if (last < first) {
        return false;
    }
    *offset = first * FLASH_SECTOR_SIZE;
    *size = (last - first + 1u) * FLASH_SECTOR_SIZE;
    return true;
}

static int inactive_partition(void)
{
    int booted = booted_partition(0);
    return (booted == 0 || booted == 1) ? 1 - booted : -1;
}

/* Once per boot: is this the trial of the update recorded, or a boot after one
 * that did not settle — rolled back? */
// @req PICO-0011
static void settle(void)
{
    if (settled) {
        return;
    }
    settled = true;
    state_load();
    if (!state.pending) {
        return;
    }
    bool trial = false;
    int booted = booted_partition(&trial);
    if (trial && booted == (int)state.pending_partition) {
        return; /* on trial: rh_update_pending */
    }
    state.outcome = RH_UPDATE_ROLLED_BACK;
    state.outcome_info = state.info;
    state.pending = 0;
    (void)state_save();
}

/* -------------------------------------------------------------------------- */
/*  The contract                                                              */
/* -------------------------------------------------------------------------- */

static int write_partition = -1;
static uint32_t write_offset, write_size, written;
static uint8_t page[FLASH_PAGE_SIZE];
static uint32_t page_fill;

// @req PICO-0011
void rh_update_caps_get(rh_update_caps *caps)
{
    if (caps == 0) {
        return;
    }
    uint32_t offset = 0, size = 0;
    caps->updatable = partition_place(inactive_partition(), &offset, &size);
    caps->slot_size = caps->updatable ? size : 0u;
}

// @req PICO-0011
RoboHubError rh_update_begin(uint32_t size)
{
    settle();
    int partition = inactive_partition();
    uint32_t offset = 0, room = 0;
    if (!partition_place(partition, &offset, &room)) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }
    if (size == 0u || size > room) {
        return ROBOHUB_ERR_NO_MEM;
    }
    write_partition = partition;
    write_offset = offset;
    write_size = size;
    written = 0u;
    page_fill = 0u;
    return ROBOHUB_OK;
}

/* A whole page to flash; the sector it starts is erased first. */
static RoboHubError flush_page(void)
{
    uint32_t at = write_offset + written - page_fill;
    if ((at % FLASH_SECTOR_SIZE) == 0u && flash_erase(at) != 0) {
        return ROBOHUB_ERR_IO;
    }
    if (page_fill < FLASH_PAGE_SIZE) {
        memset(page + page_fill, 0xFF, FLASH_PAGE_SIZE - page_fill);
    }
    if (flash_page(at, page) != 0) {
        return ROBOHUB_ERR_IO;
    }
    page_fill = 0u;
    return ROBOHUB_OK;
}

// @req PICO-0011
RoboHubError rh_update_write(uint32_t offset, const uint8_t *data, uint32_t length)
{
    if (write_partition < 0 || offset != written) {
        return ROBOHUB_ERR_STATE;
    }
    if (data == 0 || length > write_size - written) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    while (length > 0u) {
        uint32_t n = FLASH_PAGE_SIZE - page_fill;
        if (n > length) {
            n = length;
        }
        memcpy(page + page_fill, data, n);
        page_fill += n;
        written += n;
        data += n;
        length -= n;
        if (page_fill == FLASH_PAGE_SIZE || written == write_size) {
            RoboHubError err = flush_page();
            if (err != ROBOHUB_OK) {
                return err;
            }
        }
    }
    return ROBOHUB_OK;
}

// @req PICO-0011
RoboHubError rh_update_read(uint32_t offset, uint8_t *out, uint32_t length)
{
    if (write_partition < 0) {
        return ROBOHUB_ERR_STATE;
    }
    /* What is still in the page buffer is not in flash yet. */
    uint32_t in_flash = written - page_fill;
    if (out == 0 || offset > in_flash || length > in_flash - offset) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    memcpy(out, (const void *)(uintptr_t)(PICO_UPDATE_READ_BASE + write_offset + offset), length);
    return ROBOHUB_OK;
}

// @req PICO-0011
RoboHubError rh_update_boot_once(const rh_update_info *info)
{
    if (write_partition < 0 || written != write_size || page_fill != 0u || info == 0) {
        return ROBOHUB_ERR_STATE;
    }
    state_load();
    state.pending = 1;
    state.pending_partition = (uint32_t)write_partition;
    state.info = *info;
    if (state_save() != 0) {
        return ROBOHUB_ERR_IO;
    }
    /* A flash update boot, naming the partition just written: the boot ROM
     * prefers it and enters it on trial. A quarter of a second first, so the
     * `verified` just sent leaves the board before its USB goes away. */
    rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE | REBOOT2_FLAG_NO_RETURN_ON_SUCCESS, 250u,
               XIP_BASE + write_offset, 0u);
    return ROBOHUB_ERR_STATE; /* the reboot did not start */
}

// @req PICO-0011
bool rh_update_pending(rh_update_info *out)
{
    settle();
    if (!state.pending) {
        return false;
    }
    if (out != 0) {
        *out = state.info;
    }
    return true;
}

// @req PICO-0011
RoboHubError rh_update_confirm(void)
{
    if (!rh_update_pending(0)) {
        return ROBOHUB_ERR_STATE;
    }
    int rc = rom_explicit_buy(rom_work, sizeof rom_work);
    if (rc != BOOTROM_OK) {
        return ROBOHUB_ERR_IO;
    }
    if (state.info.security_version > state.security) {
        state.security = state.info.security_version;
    }
    state.outcome = RH_UPDATE_KEPT;
    state.outcome_info = state.info;
    state.pending = 0;
    return state_save() == 0 ? ROBOHUB_OK : ROBOHUB_ERR_IO;
}

// @req PICO-0011
RoboHubError rh_update_reject(void)
{
    if (!rh_update_pending(0)) {
        return ROBOHUB_ERR_STATE;
    }
    /* A normal boot chooses the untried image; the next boot finds the state
     * sector still naming the update and calls it rolled back. */
    watchdog_reboot(0, 0, 0);
    for (;;) {
    }
}

// @req PICO-0011
rh_update_outcome rh_update_take_outcome(rh_update_info *out)
{
    settle();
    rh_update_outcome outcome = (rh_update_outcome)state.outcome;
    if (outcome == RH_UPDATE_NONE) {
        return RH_UPDATE_NONE;
    }
    if (out != 0) {
        *out = state.outcome_info;
    }
    state.outcome = RH_UPDATE_NONE;
    (void)state_save();
    return outcome;
}

// @req PICO-0011
uint32_t rh_update_security_version(void)
{
    state_load();
    return state.security;
}

// @req PICO-0011
int rh_update_running_slot(void)
{
    int booted = booted_partition(0);
    return (booted == 0 || booted == 1) ? booted : -1;
}

#endif /* RH_PICO_SDK */
