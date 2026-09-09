/* RISC-V 64-bit baremetal startup code */

.section .text.init
.global _start
_start:
    /* Disable interrupts (clear MIE bit 3) */
    csrci mstatus, 0x8

    /* Set default trap handler */
    la t0, _trap_handler
    csrw mtvec, t0

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

    /* Call main(0, NULL) */
    li a0, 0
    li a1, 0
    call main

    /* Terminate execution */
    /* Try QEMU virt test finisher at 0x100000 */
    li t0, 0x100000
    beqz a0, 1f
    slliw a0, a0, 16
    li t1, 0x3333
    or a0, a0, t1
    sw a0, 0(t0)
    j loop
1:
    li t1, 0x5555
    sw t1, 0(t0)
loop:
    wfi
    j loop

.global _trap_handler
    .p2align 2
_trap_handler:
    wfi
    j _trap_handler
