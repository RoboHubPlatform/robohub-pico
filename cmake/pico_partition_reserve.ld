/*
 * pico_partition_reserve.ld — keep an image within one A/B partition (PICO-0012).
 *
 * Passed to the linker beside the SDK's script, as the keystore's check is.
 * RH_PARTITION_END is defined on the link line (--defsym) as XIP_BASE plus the
 * partition size of partitions/ab.json: the boot ROM maps a partition to
 * 0x10000000, and an image longer than the partition does not fit it.
 *
 * SPDX-License-Identifier: MIT
 */
ASSERT(__flash_binary_end <= RH_PARTITION_END,
       "robohub-pico: the image is larger than an A/B partition (2036 KiB, partitions/ab.json)")
