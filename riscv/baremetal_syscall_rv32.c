/* Baremetal system call stubs for RISC-V RV32 */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#include "../mquickjs.h"

/* Define FILE structure for stdout */
typedef struct {
    int dummy;
} FILE;

FILE *stdout = (FILE *)1;

/* Define missing types for RV32 */
#ifndef ssize_t
#define ssize_t int
#endif

#ifndef off_t
#define off_t int
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)0xFFFFFFFFu)
#endif

/* Define time structures */
struct timeval {
    long tv_sec;
    long tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

/* UART16550 base address, overridable via CFLAGS (QEMU virt default) */
#ifndef UART_BASE_ADDR
#define UART_BASE_ADDR 0x10000000
#endif

/* Simple putchar */
void putchar(char c) {
    volatile char *uart = (volatile char *)UART_BASE_ADDR;
    *uart = c;
}

/* Simple puts */
void puts(const char *s) {
    while (*s) {
        putchar(*s++);
    }
}

/* Minimal printf implementation */
static char print_buf[256];
static int print_idx = 0;

static void print_char(char c) {
    if (c == '\n') {
        putchar('\r');
    }
    putchar(c);
}

static void print_string(const char *s) {
    while (*s) {
        print_char(*s++);
    }
}

static void print_number(long num, int base, int is_signed, int width, char pad_char) {
    char buf[32];
    char digits[] = "0123456789abcdef";
    int i = 0;
    unsigned long unum;
    int negative = 0;

    if (is_signed && num < 0) {
        negative = 1;
        unum = -num;
    } else {
        unum = num;
    }

    if (unum == 0) {
        buf[i++] = '0';
    } else {
        while (unum > 0) {
            buf[i++] = digits[unum % base];
            unum /= base;
        }
    }

    if (negative) {
        buf[i++] = '-';
    }

    /* Pad with spaces/zeros */
    while (i < width) {
        print_char(pad_char);
        width--;
    }

    /* Print the number in reverse */
    while (i > 0) {
        print_char(buf[--i]);
    }
}

int printf(const char *fmt, ...) {
    va_list args;
    int width;
    char pad_char;

    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            print_char(*fmt++);
            continue;
        }

        fmt++; /* skip '%' */

        /* Handle width specifier (simple version) */
        width = 0;
        pad_char = ' ';
        if (*fmt == '0') {
            pad_char = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt++) {
            case 'd':
                print_number(va_arg(args, int), 10, 1, width, pad_char);
                break;
            case 'u':
                print_number(va_arg(args, unsigned int), 10, 0, width, pad_char);
                break;
            case 'x':
                print_number(va_arg(args, unsigned int), 16, 0, width, pad_char);
                break;
            case 'p':
                print_string("0x");
                print_number((unsigned int)va_arg(args, void *), 16, 0, 8, '0');
                break;
            case 's':
                print_string(va_arg(args, const char *));
                break;
            case 'c':
                print_char(va_arg(args, int));
                break;
            case '%':
                print_char('%');
                break;
            default:
                print_char('%');
                print_char(fmt[-1]);
                break;
        }
    }

    va_end(args);
    return 0;
}

/* Minimal exit */
void exit(int status) {
    while (1) {
        asm volatile("wfi");
    }
}

/* Stub functions */
int errno = 0;
char **environ = NULL;

/* File I/O stubs */
int open(const char *pathname, int flags, ...) { return -1; }
int close(int fd) { return -1; }
ssize_t read(int fd, void *buf, size_t count) { return -1; }
ssize_t write(int fd, const void *buf, size_t count) {
    const char *p = (const char *)buf;
    size_t i;
    for (i = 0; i < count; i++) {
        putchar(p[i]);
    }
    return count;
}
off_t lseek(int fd, off_t offset, int whence) { return -1; }

/* Function definitions for the baremetal version */
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int i;
    JSValue v;
    
    for(i = 0; i < argc; i++) {
        if (i != 0) {
            putchar(' ');
        }
        v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v, &buf);
            if (!str)
                return JS_EXCEPTION;
            // Write each character individually using putchar (assumes putchar is available in your baremetal environment)
            for(size_t j = 0; j < len; j++) {
                putchar(str[j]);
            }
        } else {
            // For now, we'll just return undefined for non-strings, since JS_PrintValueF might not work in baremetal
            // You might need to implement a simple JS value to string function for baremetal
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JS_GC(ctx);
    return JS_UNDEFINED;
}

JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // Not implemented for baremetal
    return JS_ThrowTypeError(ctx, "load() not implemented");
}

JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // Not implemented for baremetal
    return JS_ThrowTypeError(ctx, "setTimeout() not implemented");
}

JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // Not implemented for baremetal
    return JS_ThrowTypeError(ctx, "clearTimeout() not implemented");
}

/* console.log implementation */
JSValue js_console_log(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int i;
    JSValue v;
    
    for(i = 0; i < argc; i++) {
        if (i != 0)
            print_char(' ');
        v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v, &buf);
            if (str) {
                print_string(str);
            }
        } else {
            // For non-string values, just print a placeholder
            print_string("[object]");
        }
    }
    print_char('\n');
    return JS_UNDEFINED;
}

/* Forward declarations */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

/* Memory stubs for RV32 */
typedef struct {
    size_t size;
} alloc_header_t;

void *malloc(size_t size) {
    extern char __heap_start[];
    extern char __heap_end[];
    static char *heap_ptr = NULL;

    if (heap_ptr == NULL) {
        heap_ptr = __heap_start;
    }

    /* Align to 4 bytes for RV32 */
    uintptr_t cur = ((uintptr_t)heap_ptr + 3) & ~3u;
    size_t total = sizeof(alloc_header_t) + size;

    if (cur + total > (uintptr_t)__heap_end || cur + total < cur) {
        return NULL; /* out of memory or overflow */
    }

    alloc_header_t *hdr = (alloc_header_t *)cur;
    hdr->size = size;

    heap_ptr = (char *)(cur + total);
    return (void *)(hdr + 1);
}

void free(void *ptr) {
    /* Simple allocator - no free */
    (void)ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    alloc_header_t *old_hdr = (alloc_header_t *)ptr - 1;
    void *new_ptr = malloc(size);
    if (new_ptr) {
        size_t copy_size = (old_hdr->size < size) ? old_hdr->size : size;
        memcpy(new_ptr, ptr, copy_size);
        free(ptr);
    }
    return new_ptr;
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/* Math functions */
double fabs(double x) {
    if (x < 0)
        return -x;
    return x;
}

void __assert_func(const char *file, int line, const char *func, const char *expr) {
    printf("Assertion failed: %s at %s:%d in %s\n", expr, file, line, func);
    while (1) {
        asm volatile("wfi");
    }
}

double copysign(double x, double y) {
    unsigned int xbits = *(unsigned int *)&x;
    unsigned int ybits = *(unsigned int *)&y;
    xbits = (xbits & 0x7FFFFFFF) | (ybits & 0x80000000);
    return *(double *)&xbits;
}

int abs(int x) {
    if (x < 0)
        return -x;
    return x;
}

void abort(void) {
    printf("Aborting...\n");
    while (1) {
        asm volatile("wfi");
    }
}

/* setjmp/longjmp implementation for RV32 */
typedef struct {
    unsigned int ra;
    unsigned int sp;
    unsigned int gp;
    unsigned int tp;
    unsigned int t0;
    unsigned int t1;
    unsigned int t2;
    unsigned int t3;
    unsigned int t4;
    unsigned int t5;
    unsigned int t6;
    unsigned int s0;
    unsigned int s1;
    unsigned int a0;
    unsigned int a1;
    unsigned int a2;
    unsigned int a3;
    unsigned int a4;
    unsigned int a5;
    unsigned int a6;
    unsigned int a7;
    unsigned int s2;
    unsigned int s3;
    unsigned int s4;
    unsigned int s5;
    unsigned int s6;
    unsigned int s7;
    unsigned int s8;
    unsigned int s9;
    unsigned int s10;
    unsigned int s11;
} jmp_buf[1];

int setjmp(jmp_buf env) {
    /* Save essential registers for RV32 */
    asm volatile(
        "sw ra, 0(%0)\n"
        "sw sp, 4(%0)\n"
        "sw gp, 8(%0)\n"
        "sw tp, 12(%0)\n"
        "sw t0, 16(%0)\n"
        "sw t1, 20(%0)\n"
        "sw t2, 24(%0)\n"
        "sw t3, 28(%0)\n"
        "sw t4, 32(%0)\n"
        "sw t5, 36(%0)\n"
        "sw t6, 40(%0)\n"
        "sw s0, 44(%0)\n"
        "sw s1, 48(%0)\n"
        "sw s2, 52(%0)\n"
        "sw s3, 56(%0)\n"
        "sw s4, 60(%0)\n"
        "sw s5, 64(%0)\n"
        "sw s6, 68(%0)\n"
        "sw s7, 72(%0)\n"
        "sw s8, 76(%0)\n"
        "sw s9, 80(%0)\n"
        "sw s10, 84(%0)\n"
        "sw s11, 88(%0)\n"
        : : "r"(env) : "memory"
    );
    return 0;
}

void longjmp(jmp_buf env, int val) {
    if (val == 0)
        val = 1;
    asm volatile(
        "mv a0, %1\n"
        "lw ra, 0(%0)\n"
        "lw sp, 4(%0)\n"
        "lw gp, 8(%0)\n"
        "lw tp, 12(%0)\n"
        "lw t0, 16(%0)\n"
        "lw t1, 20(%0)\n"
        "lw t2, 24(%0)\n"
        "lw t3, 28(%0)\n"
        "lw t4, 32(%0)\n"
        "lw t5, 36(%0)\n"
        "lw t6, 40(%0)\n"
        "lw s0, 44(%0)\n"
        "lw s1, 48(%0)\n"
        "lw s2, 52(%0)\n"
        "lw s3, 56(%0)\n"
        "lw s4, 60(%0)\n"
        "lw s5, 64(%0)\n"
        "lw s6, 68(%0)\n"
        "lw s7, 72(%0)\n"
        "lw s8, 76(%0)\n"
        "lw s9, 80(%0)\n"
        "lw s10, 84(%0)\n"
        "lw s11, 88(%0)\n"
        "ret\n"
        : : "r"(env), "r"(val) : "memory"
    );
}

/* String functions */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n-- && (*d++ = *src++));
    while (n--) *d++ = '\0';
    return dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return n == SIZE_MAX ? 0 : *(unsigned char *)s1 - *(unsigned char *)s2;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

/* GCC built-in functions for 64-bit arithmetic on RV32 */
typedef int64_t di_int;
typedef uint64_t udi_int;

/* __ashldi3, __ashrdi3, and __lshrdi3 are implemented in shift_rv32.s */

udi_int __udivdi3(udi_int x, udi_int n) {
    udi_int q = 0, r = 0;
    int i;
    if (n == 0) return 0;
    
    /* Generic long division using restorative algorithm */
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((x >> i) & 1);
        if (r >= n) {
            r -= n;
            q |= ((udi_int)1 << i);
        }
    }
    return q;
}

di_int __divdi3(di_int x, di_int n) {
    int neg = 0;
    udi_int xu = (udi_int)x;
    udi_int nu = (udi_int)n;
    if (x < 0) { neg = 1; xu = -xu; }
    if (n < 0) { neg ^= 1; nu = -nu; }
    udi_int result = __udivdi3(xu, nu);
    return neg ? -(di_int)result : (di_int)result;
}

udi_int __umoddi3(udi_int x, udi_int n) {
    udi_int r = 0;
    int i;
    if (n == 0) return 0;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((x >> i) & 1);
        if (r >= n) {
            r -= n;
        }
    }
    return r;
}

int __clzsi2(uint32_t x) {
    if (x == 0) return 32;
    int count = 0;
    while ((x & 0x80000000) == 0) { count++; x <<= 1; }
    return count;
}

int __ctzsi2(uint32_t x) {
    if (x == 0) return 32;
    int count = 0;
    while ((x & 1) == 0) { count++; x >>= 1; }
    return count;
}

int __clzdi2(uint64_t x) {
    int n = 0;
    if ((x >> 32) == 0) { n += 32; x <<= 32; }
    if ((x >> 48) == 0) { n += 16; x <<= 16; }
    if ((x >> 56) == 0) { n += 8; x <<= 8; }
    if ((x >> 60) == 0) { n += 4; x <<= 4; }
    if ((x >> 62) == 0) { n += 2; x <<= 2; }
    if ((x >> 63) == 0) { n += 1; }
    return n;
}

int __ctzdi2(uint64_t x) {
    int n = 0;
    if ((x & 0xFFFFFFFF) == 0) { n += 32; x >>= 32; }
    if ((x & 0xFFFF) == 0) { n += 16; x >>= 16; }
    if ((x & 0xFF) == 0) { n += 8; x >>= 8; }
    if ((x & 0xF) == 0) { n += 4; x >>= 4; }
    if ((x & 0x3) == 0) { n += 2; x >>= 2; }
    if ((x & 0x1) == 0) { n += 1; }
    return n;
}

/* Time from the RISC-V cycle counter, relative to the counter origin. */
#ifndef MCOUNTER_FREQ_HZ
#define MCOUNTER_FREQ_HZ 10000000ULL
#endif

static uint64_t read_mcycle(void)
{
    uint32_t hi, lo, hi_again;

    do {
        __asm__ volatile ("csrr %0, mcycleh" : "=r"(hi));
        __asm__ volatile ("csrr %0, mcycle" : "=r"(lo));
        __asm__ volatile ("csrr %0, mcycleh" : "=r"(hi_again));
    } while (hi != hi_again);
    return ((uint64_t)hi << 32) | lo;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    uint64_t cycles, sec, rem;

    (void)tz;
    if (!tv)
        return 0;
    cycles = read_mcycle();
    sec = cycles / MCOUNTER_FREQ_HZ;
    rem = cycles % MCOUNTER_FREQ_HZ;
    tv->tv_sec = (long)sec;
    tv->tv_usec = (long)(rem * 1000000ULL / MCOUNTER_FREQ_HZ);
    return 0;
}

/* Assert stub */
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    printf("Assertion failed: %s, file %s, line %u, function %s\n",
           assertion, file, line, function);
    exit(1);
}
