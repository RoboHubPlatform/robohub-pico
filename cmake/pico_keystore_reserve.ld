/*
 * pico_keystore_reserve.ld — keep the device keystore's two flash sectors out of
 * the image.
 *
 * Passed to the linker as an implicit script beside the SDK's own, so it adds a
 * check and changes no layout. RH_KEYSTORE_RESERVED_START is defined on the link
 * line (--defsym) as the address of the first keystore sector:
 * XIP_BASE + PICO_FLASH_SIZE_BYTES - 2 * 4096. An image whose end reaches it would
 * have its tail erased by a key rotation, so it does not link.
 *
 * SPDX-License-Identifier: MIT
 */
ASSERT(__flash_binary_end <= RH_KEYSTORE_RESERVED_START,
       "robohub-pico: the image overlaps the device keystore (the last two 4 KiB flash sectors)")
