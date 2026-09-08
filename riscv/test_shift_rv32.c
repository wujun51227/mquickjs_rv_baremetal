/*
 * Unit tests for RV32 64-bit shift helpers in shift_rv32.s.
 *
 * Host:  gcc -O0 -DHOST_TEST -o test_shift_ref_host riscv/test_shift_rv32.c
 * Target: see Makefile.riscv target test-shift-rv32
 *
 * The oracle shifts one bit at a time using only 32-bit operations, so it
 * never calls __ashldi3/__lshrdi3/__ashrdi3.  HOST_TEST checks that oracle
 * against native 64-bit shifts; the RV32 build checks the assembly helpers
 * against the same oracle.
 */

#ifdef HOST_TEST
#include <stdio.h>
#endif

typedef unsigned int u32;
typedef unsigned long long u64;

typedef struct {
    u32 lo;
    u32 hi;
} u64s;

#ifdef HOST_TEST
/* Native reference used only to validate the bit-by-bit oracle. */
u64 __ashldi3(u64 a, int b)
{
    unsigned ub = (unsigned)b;
    if (ub >= 64)
        return 0;
    return a << ub;
}

u64 __lshrdi3(u64 a, int b)
{
    unsigned ub = (unsigned)b;
    if (ub >= 64)
        return 0;
    return a >> ub;
}

u64 __ashrdi3(u64 a, int b)
{
    unsigned ub = (unsigned)b;
    long long sa = (long long)a;
    if (ub >= 64)
        return (u64)(sa >> 63);
    return (u64)(sa >> ub);
}
#else
u64 __ashldi3(u64 a, int b);
u64 __lshrdi3(u64 a, int b);
u64 __ashrdi3(u64 a, int b);
#endif

static u64 pack(u32 lo, u32 hi)
{
    union {
        u64 all;
        u32 w[2];
    } u;
    u.w[0] = lo;
    u.w[1] = hi;
    return u.all;
}

static u64s unpack(u64 v)
{
    union {
        u64 all;
        u32 w[2];
    } u;
    u64s s;
    u.all = v;
    s.lo = u.w[0];
    s.hi = u.w[1];
    return s;
}

/* Bit-by-bit oracles. optimize("O0") + volatile count so GCC cannot
 * rewrite the loop as a 64-bit shift (which would call the helpers). */
#if defined(__GNUC__)
#define ORACLE __attribute__((noinline, noclone, optimize("O0")))
#else
#define ORACLE
#endif

ORACLE static u64s ref_shl(u64s a, unsigned b)
{
    if (b >= 64) {
        u64s z;
        z.lo = 0;
        z.hi = 0;
        return z;
    }
    {
        volatile unsigned n = b;
        while (n--) {
            u32 carry = a.lo >> 31;
            a.lo <<= 1;
            a.hi = (a.hi << 1) | carry;
        }
    }
    return a;
}

ORACLE static u64s ref_lshr(u64s a, unsigned b)
{
    if (b >= 64) {
        u64s z;
        z.lo = 0;
        z.hi = 0;
        return z;
    }
    {
        volatile unsigned n = b;
        while (n--) {
            u32 carry = a.hi & 1u;
            a.hi >>= 1;
            a.lo = (a.lo >> 1) | (carry << 31);
        }
    }
    return a;
}

ORACLE static u64s ref_ashr(u64s a, unsigned b)
{
    if (b >= 64) {
        u32 s = (u32)((int)a.hi >> 31);
        u64s z;
        z.lo = s;
        z.hi = s;
        return z;
    }
    {
        volatile unsigned n = b;
        while (n--) {
            u32 carry = a.hi & 1u;
            a.hi = (u32)((int)a.hi >> 1);
            a.lo = (a.lo >> 1) | (carry << 31);
        }
    }
    return a;
}

#ifdef HOST_TEST
static void tputc(char c)
{
    putchar(c);
}
#else
#ifndef UART_BASE_ADDR
#define UART_BASE_ADDR 0x10000000
#endif

static void tputc(char c)
{
    volatile char *uart = (volatile char *)UART_BASE_ADDR;
    if (c == '\n')
        *uart = '\r';
    *uart = c;
}

/* QEMU virt SiFive test finisher at 0x100000. */
static void qemu_exit(int code)
{
    volatile u32 *finisher = (volatile u32 *)0x00100000u;
    if (code == 0)
        *finisher = 0x5555u;
    else
        *finisher = 0x3333u | ((u32)code << 16);
    for (;;)
        ;
}
#endif

static void tputs(const char *s)
{
    while (*s)
        tputc(*s++);
}

static void print_hex32(u32 v)
{
    static const char hex[] = "0123456789abcdef";
    int i;
    for (i = 7; i >= 0; i--)
        tputc(hex[(v >> (i * 4)) & 0xfu]);
}

static void print_u32(u32 v)
{
    char buf[10];
    int n = 0;
    if (v == 0) {
        tputc('0');
        return;
    }
    while (v) {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n--)
        tputc(buf[n]);
}

static int failures;
static int tested;

static void check_one(const char *op, u32 lo, u32 hi, unsigned b,
                      u64 got, u64s exp)
{
    u64s g = unpack(got);
    tested++;
    if (g.lo == exp.lo && g.hi == exp.hi)
        return;
    failures++;
    if (failures > 12)
        return;
    tputs("FAIL ");
    tputs(op);
    tputs(" lo=");
    print_hex32(lo);
    tputs(" hi=");
    print_hex32(hi);
    tputs(" b=");
    print_u32(b);
    tputs(" got=");
    print_hex32(g.hi);
    tputc('_');
    print_hex32(g.lo);
    tputs(" exp=");
    print_hex32(exp.hi);
    tputc('_');
    print_hex32(exp.lo);
    tputc('\n');
}

static void test_value(u32 lo, u32 hi)
{
    static const unsigned shifts[] = {
        0, 1, 2, 3, 4, 7, 8, 15, 16, 17, 31,
        32, 33, 47, 48, 49, 63, 64, 65, 127, 255
    };
    unsigned i;
    u64 val = pack(lo, hi);
    u64s in;
    in.lo = lo;
    in.hi = hi;

    for (i = 0; i < (unsigned)(sizeof(shifts) / sizeof(shifts[0])); i++) {
        unsigned b = shifts[i];
        check_one("shl ", lo, hi, b, __ashldi3(val, (int)b), ref_shl(in, b));
        check_one("lshr", lo, hi, b, __lshrdi3(val, (int)b), ref_lshr(in, b));
        check_one("ashr", lo, hi, b, __ashrdi3(val, (int)b), ref_ashr(in, b));
    }
}

static void test_all_amounts(u32 lo, u32 hi)
{
    unsigned b;
    u64 val = pack(lo, hi);
    u64s in;
    in.lo = lo;
    in.hi = hi;
    for (b = 0; b < 80; b++) {
        check_one("shl ", lo, hi, b, __ashldi3(val, (int)b), ref_shl(in, b));
        check_one("lshr", lo, hi, b, __lshrdi3(val, (int)b), ref_lshr(in, b));
        check_one("ashr", lo, hi, b, __ashrdi3(val, (int)b), ref_ashr(in, b));
    }
}

int main(void)
{
    unsigned i;
    static const u32 vals[][2] = {
        { 0x00000000u, 0x00000000u },
        { 0x00000001u, 0x00000000u },
        { 0x00000002u, 0x00000000u },
        { 0x80000000u, 0x00000000u },
        { 0xffffffffu, 0x00000000u },
        { 0x00000000u, 0x00000001u },
        { 0x00000000u, 0x80000000u },
        { 0x00000000u, 0xffffffffu },
        { 0xffffffffu, 0xffffffffu },
        { 0xffffffffu, 0x7fffffffu },
        { 0x00000000u, 0x80000000u },
        { 0x9abcdef0u, 0x12345678u },
        { 0x12345678u, 0x9abcdef0u },
        { 0xaaaaaaaau, 0x55555555u },
        { 0x55555555u, 0xaaaaaaaau },
        { 0x0000ffffu, 0xffff0000u },
        { 0x0f0f0f0fu, 0xf0f0f0f0u },
        { 0x00000001u, 0x80000000u },
        { 0x80000000u, 0x00000001u },
        { 0x7fffffffu, 0x00000000u },
        { 0x00000000u, 0x7fffffffu },
    };

    tputs("shift_rv32 unit tests\n");

    for (i = 0; i < (unsigned)(sizeof(vals) / sizeof(vals[0])); i++)
        test_value(vals[i][0], vals[i][1]);

    /* Exhaustive shift amounts 0..79 on a few interesting patterns. */
    test_all_amounts(0x00000001u, 0x00000000u);
    test_all_amounts(0x80000000u, 0x00000000u);
    test_all_amounts(0x00000000u, 0x80000000u);
    test_all_amounts(0xffffffffu, 0xffffffffu);
    test_all_amounts(0x9abcdef0u, 0x12345678u);
    test_all_amounts(0x00000000u, 0x00000000u);

    /* Walking-one / walking-zero across the whole 64-bit word. */
    for (i = 0; i < 64; i++) {
        u32 lo = 0, hi = 0;
        if (i < 32)
            lo = 1u << i;
        else
            hi = 1u << (i - 32);
        test_value(lo, hi);
        test_value(~lo, ~hi);
    }

    tputs("tested=");
    print_u32((u32)tested);
    tputs(" failures=");
    print_u32((u32)failures);
    tputc('\n');

    if (failures) {
        tputs("FAIL\n");
#ifndef HOST_TEST
        qemu_exit(1);
#endif
        return 1;
    }
    tputs("PASS\n");
#ifndef HOST_TEST
    qemu_exit(0);
#endif
    return 0;
}
