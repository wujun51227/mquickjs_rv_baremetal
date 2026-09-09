/* Baremetal system call stubs for RISC-V RV64 */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>


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

/* Memory stubs for RV64 */
typedef struct {
    size_t size;
} alloc_header_t;

static char *heap_ptr;

void *malloc(size_t size) {
    extern char __heap_start[];
    extern char __heap_end[];

    if (heap_ptr == NULL) {
        heap_ptr = __heap_start;
    }

    // Align to 8 bytes for RV64
    uintptr_t cur = ((uintptr_t)heap_ptr + 7) & ~7ul;
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

    if (!ptr) {
        return malloc(size);
    }
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

/* Time from the RISC-V cycle counter, relative to the counter origin. */
#ifndef MCOUNTER_FREQ_HZ
#define MCOUNTER_FREQ_HZ 10000000ULL
#endif

static uint64_t read_mcycle(void)
{
    uint64_t value;

    __asm__ volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
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
