/* Baremetal system call stubs for RISC-V RV32 */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>


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

/* Simple putchar with 16550 LSR THRE check and CRLF translation */
int putchar(int c) {
    volatile uint8_t *uart = (volatile uint8_t *)UART_BASE_ADDR;
    if (c == '\n') {
        while ((uart[5] & 0x20) == 0);
        uart[0] = '\r';
    }
    while ((uart[5] & 0x20) == 0);
    uart[0] = (uint8_t)c;
    return c;
}

/* Simple puts */
int puts(const char *s) {
    while (*s) {
        putchar(*s++);
    }
    putchar('\n');
    return 0;
}

/* Minimal printf implementation */
static void print_char(char c) {
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
        unum = (unsigned long)-num;
    } else {
        unum = (unsigned long)num;
    }

    if (unum == 0) {
        buf[i++] = '0';
    } else {
        while (unum > 0) {
            buf[i++] = digits[unum % base];
            unum /= base;
        }
    }

    if (negative && pad_char == '0') {
        print_char('-');
        negative = 0;
        if (width > 0) width--;
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
    volatile uint32_t *test_dev = (volatile uint32_t *)0x100000;
    if (status == 0) {
        *test_dev = 0x5555;
    } else {
        *test_dev = 0x3333 | ((uint32_t)status << 16);
    }
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

/* Forward declarations */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

/* Memory stubs for RV32 */
typedef struct {
    size_t size;
    size_t dummy; /* Pad to 8 bytes so (hdr + 1) is 8-byte aligned */
} alloc_header_t;

static char *heap_ptr;

void *malloc(size_t size) {
    extern char __heap_start[];
    extern char __heap_end[];

    if (heap_ptr == NULL) {
        heap_ptr = __heap_start;
    }

    /* Align to 8 bytes for RV32 */
    uintptr_t cur = ((uintptr_t)heap_ptr + 7) & ~7u;
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
    extern char __heap_end[];
    alloc_header_t *old_hdr;
    uintptr_t old_end, new_end;
    void *new_ptr;
    size_t copy_size;

    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    old_hdr = (alloc_header_t *)ptr - 1;
    old_end = (uintptr_t)old_hdr + sizeof(*old_hdr) + old_hdr->size;
    new_end = (uintptr_t)old_hdr + sizeof(*old_hdr) + size;
    if ((char *)old_end == heap_ptr && new_end >= (uintptr_t)old_hdr &&
        new_end <= (uintptr_t)__heap_end) {
        old_hdr->size = size;
        heap_ptr = (char *)new_end;
        return ptr;
    }

    new_ptr = malloc(size);
    if (!new_ptr)
        return NULL;
    copy_size = (old_hdr->size < size) ? old_hdr->size : size;
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);
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
    union {
        double d;
        uint64_t i;
    } ux, uy;
    ux.d = x;
    uy.d = y;
    ux.i = (ux.i & 0x7FFFFFFFFFFFFFFFull) | (uy.i & 0x8000000000000000ull);
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
