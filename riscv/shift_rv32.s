/* RISC-V 32-bit libgcc 64-bit shift helpers.
 *
 * ABI (ilp32, little-endian):
 *   a0 = low  32 bits of the 64-bit operand
 *   a1 = high 32 bits of the 64-bit operand
 *   a2 = shift count (int; treated as unsigned)
 *   result returned in a0:a1
 *
 * RV32 sll/srl/sra only use rs2[4:0], so a shift of 32 is a no-op.
 * That is why b==0 and b>=32 must be special-cased: the carry
 * expression (lo >> (32-b)) would become (lo >> 0) when b==0.
 *
 * Counts >= 64 follow common libgcc practice:
 *   __ashldi3 / __lshrdi3 -> 0
 *   __ashrdi3             -> sign fill
 *
 * See riscv/test_shift_rv32.c for the unit tests.
 */

    .section .text
    .p2align 2

    .globl __ashldi3
    .type  __ashldi3, @function
    .globl __lshrdi3
    .type  __lshrdi3, @function
    .globl __ashrdi3
    .type  __ashrdi3, @function

/* di_int __ashldi3(di_int a, int b)  —  a << b */
__ashldi3:
    beqz    a2, .Lashl_ret          /* b == 0: identity */
    li      t0, 64
    bgeu    a2, t0, .Lashl_zero     /* b >= 64: 0 */
    li      t0, 32
    bgeu    a2, t0, .Lashl_ge32     /* 32 <= b < 64 */
    /* 1 <= b < 32:
     *   hi = (hi << b) | (lo >> (32-b))
     *   lo =  lo << b
     */
    li      t0, 32
    sub     t0, t0, a2              /* t0 = 32 - b */
    sll     a1, a1, a2
    srl     t1, a0, t0
    or      a1, a1, t1
    sll     a0, a0, a2
    ret
.Lashl_ge32:
    /* hi = lo << (b-32); lo = 0 */
    sub     a2, a2, t0              /* a2 = b - 32, t0 is 32 */
    sll     a1, a0, a2
    li      a0, 0
    ret
.Lashl_zero:
    li      a0, 0
    li      a1, 0
.Lashl_ret:
    ret
    .size __ashldi3, . - __ashldi3

/* udi_int __lshrdi3(udi_int a, int b)  —  a >> b (logical) */
__lshrdi3:
    beqz    a2, .Llshr_ret
    li      t0, 64
    bgeu    a2, t0, .Llshr_zero
    li      t0, 32
    bgeu    a2, t0, .Llshr_ge32
    /* 1 <= b < 32:
     *   lo = (lo >> b) | (hi << (32-b))
     *   hi =  hi >> b
     */
    li      t0, 32
    sub     t0, t0, a2
    srl     a0, a0, a2
    sll     t1, a1, t0
    or      a0, a0, t1
    srl     a1, a1, a2
    ret
.Llshr_ge32:
    /* lo = hi >> (b-32); hi = 0 */
    sub     a2, a2, t0
    srl     a0, a1, a2
    li      a1, 0
    ret
.Llshr_zero:
    li      a0, 0
    li      a1, 0
.Llshr_ret:
    ret
    .size __lshrdi3, . - __lshrdi3

/* di_int __ashrdi3(di_int a, int b)  —  a >> b (arithmetic) */
__ashrdi3:
    beqz    a2, .Lashr_ret
    li      t0, 64
    bgeu    a2, t0, .Lashr_ge64
    li      t0, 32
    bgeu    a2, t0, .Lashr_ge32
    /* 1 <= b < 32:
     *   lo = (lo >> b) | (hi << (32-b))
     *   hi =  hi >>> b
     */
    li      t0, 32
    sub     t0, t0, a2
    srl     a0, a0, a2
    sll     t1, a1, t0
    or      a0, a0, t1
    sra     a1, a1, a2
    ret
.Lashr_ge32:
    /* 32 <= b < 64:
     *   lo = hi >>> (b-32)
     *   hi = hi >>> 31        (sign fill)
     */
    sub     a2, a2, t0
    sra     a0, a1, a2
    srai    a1, a1, 31
    ret
.Lashr_ge64:
    /* b >= 64: fill with sign bit */
    srai    a1, a1, 31
    mv      a0, a1
.Lashr_ret:
    ret
    .size __ashrdi3, . - __ashrdi3
