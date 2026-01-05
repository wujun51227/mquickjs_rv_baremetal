/* RISC-V 64-bit baremetal startup code */

.section .text.init
.global _start
_start:
    /* Disable interrupts */
    csrci mstatus, 0x8

    /* Initialize Global Pointer */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /* Clear BSS */
    la t0, __bss_start
    la t1, __bss_end
    beq t0, t1, bss_done
bss_loop:
    sd zero, 0(t0)
    addi t0, t0, 8
    blt t0, t1, bss_loop
bss_done:

    /* Setup stack pointer */
    la sp, __stack_top
    addi sp, sp, -16

    /* Call main */
    call main

    /* Infinite loop if main returns */
loop:
        fence
        li gp, 1
        li a7, 93
        li a0, 0
        ecall
    wfi
    j loop
