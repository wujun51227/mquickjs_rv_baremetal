/* Baremetal system call stubs for RISC-V RV64 */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#include "../mquickjs.h"

/* Define FILE structure for stdout */
typedef struct {
    int dummy;
} FILE;

FILE *stdout = (FILE *)1;

/* Define missing types for RV64 */
#ifndef ssize_t
typedef long ssize_t;
#endif

#ifndef off_t
typedef long off_t;
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)0xFFFFFFFFFFFFFFFFul)
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

/* Simple putchar for QEMU virt machine */
void putchar(char c) {
    /* Use UART16550 at 0x10000000 for QEMU virt machine */
    volatile char *uart = (volatile char *)0x10000000;
    *uart = c;
}

/* Simple puts */
void puts(const char *s) {
    while (*s) {
        putchar(*s++);
    }
}

/* Minimal printf implementation */
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
                print_number((unsigned long)va_arg(args, void *), 16, 0, 16, '0');
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
            // Write each character individually using putchar
            for(size_t j = 0; j < len; j++) {
                putchar(str[j]);
            }
        } else {
            // For now, we'll just return undefined for non-strings
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

JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // For baremetal, return a simple timestamp
    return JS_NewInt32(ctx, 0);
}

JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // For baremetal, return a simple timestamp
    return JS_NewInt32(ctx, 0);
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

/* Memory stubs for RV64 */
void *malloc(size_t size) {
    extern char __heap_start[];
    extern char __heap_end[];
    static char *heap_ptr = NULL;
    void *ptr;

    if (heap_ptr == NULL) {
        heap_ptr = __heap_start;
    }

    ptr = heap_ptr;
    // Align to 8 bytes for RV64
    heap_ptr = (char *)(((uintptr_t)heap_ptr + 7) & ~7ul);
    heap_ptr += size;

    if (heap_ptr > __heap_end) {
        return NULL; /* out of memory */
    }

    return ptr;
}

void free(void *ptr) {
    /* Simple allocator - no free */
    (void)ptr;
}

void *realloc(void *ptr, size_t size) {
    void *new_ptr = malloc(size);
    if (new_ptr && ptr) {
        /* Copy old data - we don't know the size, so this is imperfect */
    }
    return new_ptr;
}

void *calloc(size_t nmemb, size_t size) {
    void *ptr = malloc(nmemb * size);
    if (ptr) {
        char *p = (char *)ptr;
        size_t i;
        for (i = 0; i < nmemb * size; i++) {
            p[i] = 0;
        }
    }
    return ptr;
}

/* Math functions */
double fabs(double x) {
    if (x < 0)
        return -x;
    return x;
}

int __clzsi2(uint32_t x) {
    if (x == 0) return 32;
    int count = 0;
    while ((x & 0x80000000) == 0) {
        count++;
        x <<= 1;
    }
    return count;
}

/* Count trailing zeros for 32-bit integers */
int __ctzsi2(uint32_t x) {
    if (x == 0) return 32;
    int count = 0;
    while ((x & 1) == 0) {
        count++;
        x >>= 1;
    }
    return count;
}

void __assert_func(const char *file, int line, const char *func, const char *expr) {
    printf("Assertion failed: %s at %s:%d in %s\n", expr, file, line, func);
    while (1) {
        asm volatile("wfi");
    }
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

double copysign(double x, double y) {
    union {
        double d;
        uint64_t i;
    } ux, uy;
    ux.d = x;
    uy.d = y;
    ux.i = (ux.i & 0x7FFFFFFFFFFFFFFFul) | (uy.i & 0x8000000000000000ul);
    return ux.d;
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

/* setjmp/longjmp implementation for RV64 */
typedef struct {
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
} jmp_buf[1];

int setjmp(jmp_buf env) {
    /* Save essential registers for RV64 */
    asm volatile(
        "sd ra, 0(%0)\n"
        "sd sp, 8(%0)\n"
        "sd gp, 16(%0)\n"
        "sd tp, 24(%0)\n"
        "sd t0, 32(%0)\n"
        "sd t1, 40(%0)\n"
        "sd t2, 48(%0)\n"
        "sd t3, 56(%0)\n"
        "sd t4, 64(%0)\n"
        "sd t5, 72(%0)\n"
        "sd t6, 80(%0)\n"
        "sd s0, 88(%0)\n"
        "sd s1, 96(%0)\n"
        "sd s2, 104(%0)\n"
        "sd s3, 112(%0)\n"
        "sd s4, 120(%0)\n"
        "sd s5, 128(%0)\n"
        "sd s6, 136(%0)\n"
        "sd s7, 144(%0)\n"
        "sd s8, 152(%0)\n"
        "sd s9, 160(%0)\n"
        "sd s10, 168(%0)\n"
        "sd s11, 176(%0)\n"
        : : "r"(env) : "memory"
    );
    return 0;
}

void longjmp(jmp_buf env, int val) {
    if (val == 0)
        val = 1;
    asm volatile(
        "mv a0, %1\n"
        "ld ra, 0(%0)\n"
        "ld sp, 8(%0)\n"
        "ld gp, 16(%0)\n"
        "ld tp, 24(%0)\n"
        "ld t0, 32(%0)\n"
        "ld t1, 40(%0)\n"
        "ld t2, 48(%0)\n"
        "ld t3, 56(%0)\n"
        "ld t4, 64(%0)\n"
        "ld t5, 72(%0)\n"
        "ld t6, 80(%0)\n"
        "ld s0, 88(%0)\n"
        "ld s1, 96(%0)\n"
        "ld s2, 104(%0)\n"
        "ld s3, 112(%0)\n"
        "ld s4, 120(%0)\n"
        "ld s5, 128(%0)\n"
        "ld s6, 136(%0)\n"
        "ld s7, 144(%0)\n"
        "ld s8, 152(%0)\n"
        "ld s9, 160(%0)\n"
        "ld s10, 168(%0)\n"
        "ld s11, 176(%0)\n"
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

/* Time stubs */
int gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tv) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
    return 0;
}

/* Built-in functions for 64-bit integers (needed for RV64) */
int __clzdi2(unsigned long x) {
    int n = 0;
    if (x == 0)
        return 64;
    if ((x >> 32) == 0) {
        n += 32;
        x <<= 32;
    }
    if ((x >> 48) == 0) {
        n += 16;
        x <<= 16;
    }
    if ((x >> 56) == 0) {
        n += 8;
        x <<= 8;
    }
    if ((x >> 60) == 0) {
        n += 4;
        x <<= 4;
    }
    if ((x >> 62) == 0) {
        n += 2;
        x <<= 2;
    }
    if ((x >> 63) == 0) {
        n += 1;
    }
    return n;
}

int __ctzdi2(unsigned long x) {
    int n = 0;
    if ((x & 0xFFFFFFFFul) == 0) {
        n += 32;
        x >>= 32;
    }
    if ((x & 0xFFFFul) == 0) {
        n += 16;
        x >>= 16;
    }
    if ((x & 0xFFul) == 0) {
        n += 8;
        x >>= 8;
    }
    if ((x & 0xFul) == 0) {
        n += 4;
        x >>= 4;
    }
    if ((x & 0x3ul) == 0) {
        n += 2;
        x >>= 2;
    }
    if ((x & 0x1ul) == 0) {
        n += 1;
    }
    return n;
}

/* Assert stub */
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    printf("Assertion failed: %s, file %s, line %u, function %s\n",
           assertion, file, line, function);
    exit(1);
}
