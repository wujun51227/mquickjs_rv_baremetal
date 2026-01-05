/* RISC-V 32-bit shift functions */

.section .text
.global __ashldi3
.global __lshrdi3
.global __ashrdi3

/* di_int __ashldi3(di_int a, int b) */
__ashldi3:
    /* a0-a1 = input (64-bit), a2 = shift count */
    /* Check if shift >= 64 */
    li t0, 64
    bltu a2, t0, 1f
    /* Shift >= 64, return 0 */
    li a0, 0
    li a1, 0
    ret
1:
    /* Check if shift >= 32 */
    li t0, 32
    bltu a2, t0, 2f
    /* Shift >= 32: shift low to high, clear low */
    sub a2, a2, t0
    mv a1, a0
    sll a1, a1, a2
    li a0, 0
    ret
2:
    /* Shift < 32: shift both, move bits from low to high */
    mv t0, a0
    sll a1, a1, a2
    sub t1, t0, a2
    srl t1, t1, 0
    sll t0, t0, a2
    or a1, a1, t1
    mv a0, t0
    ret

/* udi_int __lshrdi3(udi_int a, int b) */
__lshrdi3:
    /* a0-a1 = input (64-bit), a2 = shift count */
    /* Check if shift >= 64 */
    li t0, 64
    bltu a2, t0, 1f
    /* Shift >= 64, return 0 */
    li a0, 0
    li a1, 0
    ret
1:
    /* Check if shift >= 32 */
    li t0, 32
    bltu a2, t0, 2f
    /* Shift >= 32: shift high to low, clear high */
    sub a2, a2, t0
    mv a0, a1
    srl a0, a0, a2
    li a1, 0
    ret
2:
    /* Shift < 32: shift both, move bits from high to low */
    mv t0, a1
    srl a0, a0, a2
    sub t1, t0, a2
    sll t1, t1, 0
    srl t1, t1, 0
    srl t0, t0, a2
    sll t1, t1, 0
    or a0, a0, t1
    mv a1, t0
    ret

/* di_int __ashrdi3(di_int a, int b) */
__ashrdi3:
    /* a0-a1 = input (64-bit), a2 = shift count */
    /* Check if shift >= 64 */
    li t0, 64
    bltu a2, t0, 1f
    /* Shift >= 64: arithmetic shift */
    li t0, 0
    blt a1, t0, 2f
    li a0, 0
    li a1, 0
    ret
2:
    li a0, -1
    li a1, -1
    ret
1:
    /* Check if shift >= 32 */
    li t0, 32
    bltu a2, t0, 3f
    /* Shift >= 32: shift high to low, sign extend high */
    sub a2, a2, t0
    mv a0, a1
    sra a0, a0, a2
    li t0, 0
    blt a1, t0, 4f
    li a1, 0
    ret
4:
    li a1, -1
    ret
3:
    /* Shift < 32: shift both, move bits from high to low, sign extend */
    mv t0, a1
    sra a0, a0, a2
    sub t1, t0, a2
    sll t1, t1, 0
    srl t1, t1, 0
    sra t0, t0, a2
    or a0, a0, t1
    mv a1, t0
    ret
