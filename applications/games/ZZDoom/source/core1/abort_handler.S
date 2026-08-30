/*
 * abort_handler.S - speaking exception handlers for ZZDoom Core1
 * Installs a vector table; Data/Prefetch/Undef aborts dump CP15 fault
 * registers to shared struct then WFE. No silent hangs.
 *
 * shared ARM 0x04300000:
 *   [5]  = 0xBADD0DAB (data abort) / 0xBADB00AF (prefetch) / 0xBADB00DD (undef)
 *   [6]  = DFAR (fault address)
 *   [7]  = DFSR (fault status)
 *   [8]  = abort LR (faulting PC + offset)
 *   [9]  = SPSR
 *
 * ASCII only.
 */
    .section .text.vectors, "ax"
    .align 5
    .global zz_vector_table
zz_vector_table:
    ldr pc, =zz_reset_h        /* 0x00 reset */
    ldr pc, =zz_undef_h        /* 0x04 undef */
    ldr pc, =zz_svc_h          /* 0x08 svc */
    ldr pc, =zz_pabt_h         /* 0x0C prefetch abort */
    ldr pc, =zz_dabt_h         /* 0x10 data abort */
    nop                        /* 0x14 reserved */
    ldr pc, =zz_irq_h          /* 0x18 irq */
    ldr pc, =zz_fiq_h          /* 0x1C fiq */

#define SHARED 0x04300000

zz_dabt_h:
    /* r0 = shared base */
    ldr     r0, =SHARED
    /* marker */
    ldr     r1, =0xBADD0DAB
    str     r1, [r0, #20]          /* shared[5] */
    /* DFAR (data fault address) c6,c0,0 */
    mrc     p15, 0, r1, c6, c0, 0
    str     r1, [r0, #24]          /* shared[6] */
    /* DFSR (data fault status) c5,c0,0 */
    mrc     p15, 0, r1, c5, c0, 0
    str     r1, [r0, #28]          /* shared[7] */
    /* faulting addr approx = lr - 8 */
    sub     r1, lr, #8
    str     r1, [r0, #32]          /* shared[8] */
    mrs     r1, spsr
    str     r1, [r0, #36]          /* shared[9] */
    dsb
1:  wfe
    b       1b

zz_pabt_h:
    ldr     r0, =SHARED
    ldr     r1, =0xBADB00AF
    str     r1, [r0, #20]
    mrc     p15, 0, r1, c6, c0, 2     /* IFAR */
    str     r1, [r0, #24]
    mrc     p15, 0, r1, c5, c0, 1     /* IFSR */
    str     r1, [r0, #28]
    sub     r1, lr, #4
    str     r1, [r0, #32]
    dsb
2:  wfe
    b       2b

zz_undef_h:
    ldr     r0, =SHARED
    ldr     r1, =0xBADB00DD
    str     r1, [r0, #20]
    sub     r1, lr, #4
    str     r1, [r0, #32]
    dsb
3:  wfe
    b       3b

zz_reset_h:
zz_svc_h:
zz_irq_h:
zz_fiq_h:
4:  wfe
    b       4b

    .global zz_install_vectors
    .type zz_install_vectors, %function
zz_install_vectors:
    /* Set VBAR to our table */
    ldr     r0, =zz_vector_table
    mcr     p15, 0, r0, c12, c0, 0    /* write VBAR */
    isb
    bx      lr
    .size zz_install_vectors, . - zz_install_vectors
