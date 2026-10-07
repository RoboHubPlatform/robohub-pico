/*
 * Startup for RP2350 (Pico 2W).
 *
 * The symbols this uses are the generated linker script's: `_estack`,
 * `_sdata`/`_edata`/`_sidata`, and `__bss_start__`/`__bss_end__`. They are
 * named here exactly as it names them, because a startup that read a second
 * script's symbols would copy the right bytes to the wrong place — which on a
 * board with no console looks like a program that never starts.
 *
 * @project RoboHub Platform
 * @license MIT
 */

.syntax unified
.cpu cortex-m33
.thumb

.text
.thumb

/* The program's entry: the stack, then .bss cleared, then .data copied down
 * from the flash it was linked against, then main. */
.section .text._entry_point, "ax"
.global _entry_point
.type _entry_point,%function
.thumb_func
_entry_point:
    ldr r0, =_estack
    mov sp, r0

    /* Zero .bss */
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    movs r2, #0
    b .Lbss_test
.Lbss_loop:
    str r2, [r0], #4
.Lbss_test:
    cmp r0, r1
    blo .Lbss_loop

    /* Copy .data from its load address to where it runs */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    b .Ldata_test
.Ldata_loop:
    ldr r3, [r2], #4
    str r3, [r0], #4
.Ldata_test:
    cmp r0, r1
    blo .Ldata_loop

    /* C++ or not, the toolchain's own initialisers run here. */
    bl __libc_init_array

    bl main

.Lhang:
    wfi
    b .Lhang

/*
 * Vector table and PICOBIN image-definition block, in the `isr_vector` output
 * section the generated script places first.
 *
 * The bootrom does not read the first bytes of flash as a bare vector table the
 * way the RP2040 did: it scans for the block marker and reads an IMAGE_DEF out
 * of what follows, which says the part, the CPU and the security state. A flash
 * without it is a flash the bootrom will not boot, which is why the block is
 * here beside the table rather than in a section of its own.
 */
.section .isr_vector, "a"
.p2align 2
.global __vectors
__vectors:
    .word _estack
    .word _entry_point
    .word _nmi_handler
    .word _hard_fault_handler
    .word 0 /* MemManage */
    .word 0 /* BusFault */
    .word 0 /* UsageFault */
    .word 0 /* SecureFault */
    .word 0 /* Reserved */
    .word 0 /* Reserved */
    .word 0 /* Reserved */
    .word 0 /* SVCall */
    .word 0 /* DebugMonitor */
    .word 0 /* Reserved */
    .word 0 /* PendSV */
    .word _systick_handler

.global __embedded_block
__embedded_block:
    .word 0xffffded3           /* PICOBIN_BLOCK_MARKER_START */

    .byte 0x42                 /* IMAGE_TYPE item */
    .byte 0x1                  /* one word */
    .hword 0x1021              /* EXE | secure | ARM | RP2350 */

    .byte 0xff                 /* last item (0x80 | 0x7f) */
    .hword (__embedded_block_end - __embedded_block - 16) / 4
    .byte 0

    .word 0                    /* offset to the next block: none */
    .word 0xab123579           /* PICOBIN_BLOCK_MARKER_END */
__embedded_block_end:

/* The default handlers are code, so they live with the code. */
.section .text, "ax"
.weak _nmi_handler
.thumb_func
_nmi_handler:
    b .

.weak _hard_fault_handler
.thumb_func
_hard_fault_handler:
    b .

.weak _systick_handler
.thumb_func
_systick_handler:
    bx lr
