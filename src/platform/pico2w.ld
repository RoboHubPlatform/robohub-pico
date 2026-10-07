/* Linker script for Raspberry Pi Pico 2W (RP2350)
 *
 * Flash: 4 MB at 0x10000000 (XIP)
 * SRAM:  520 KB at 0x20000000
 */

MEMORY
{
    FLASH(rx)  : ORIGIN = 0x10000000, LENGTH = 4M
    RAM(rwx)   : ORIGIN = 0x20000000, LENGTH = 520K
}

ENTRY(_entry_point)

SECTIONS
{
    /* Vector table at flash base, then the PICOBIN image-definition block.
     *
     * The order is the SDK's: the bootrom reads the stack pointer and reset
     * vector from the image start, and finds the block by scanning for its
     * marker, so the table has to be first and the block after it.
     */
    .text : ALIGN(4)
    {
        __text_start__ = .;
        __vectors_start__ = .;
        KEEP(*(.vectors))
        __vectors_end__ = .;
        KEEP(*(.embedded_block))
        *(.text._entry_point)
        *(.text*)
        *(.rodata*)
        *(.glue*)
        *(.eh_frame)
        . = ALIGN(4);
        __text_end__ = .;
    } > FLASH

    /* Initialized data (copied from flash to RAM at startup) */
    .data : ALIGN(4)
    {
        __data_start__ = .;
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT > FLASH
    __data_start__source = LOADADDR(.data);

    /* BSS (zeroed at startup) */
    .bss : ALIGN(4)
    {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    /* Stack */
    .stack : ALIGN(8)
    {
        . = ALIGN(8);
        . += 4K;
        __stack_top = .;
    } > RAM

    /* Heap */
    .heap : ALIGN(8)
    {
        __heap_start__ = .;
        . += 16K;
        __heap_end__ = .;
    } > RAM

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.attributes*)
        *(.comment*)
        *(.debug*)
    }
}
