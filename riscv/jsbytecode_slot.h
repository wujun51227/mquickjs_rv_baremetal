#ifndef JSBYTECODE_SLOT_H
#define JSBYTECODE_SLOT_H

#include <stdint.h>

/*
 * Fixed-address bytecode mailbox (little-endian).
 *
 * Layout at JSBYTECODE_ADDR (default 0x80100000):
 *
 *   +0   magic     JSBC_SLOT_MAGIC ('JSB1')
 *   +4   length    size of the raw mQuickJS bytecode image
 *   +8   checksum  sum of image bytes (uint8, wrapping uint32)
 *   +12  ready     write this LAST: 0 empty, 1 ready, 2 taken, 3 done
 *   +16  image     raw test_code.bin (JS_BYTECODE_MAGIC ...)
 *
 * Loader sequence: write the complete slot with ready=0, then write
 * ready=1 as the final field. Firmware only trusts the slot after ready==1
 * and the other fields check out.
 */
#define JSBC_SLOT_MAGIC  0x3142534Au  /* 'JSB1' */

#define JSBC_SLOT_EMPTY  0u
#define JSBC_SLOT_READY  1u
#define JSBC_SLOT_TAKEN  2u
#define JSBC_SLOT_DONE   3u
#define JSBC_SLOT_ERROR  4u

typedef struct {
    uint32_t magic;
    uint32_t length;
    uint32_t checksum;
    uint32_t ready;
} JsBcSlotHeader;

#define JSBC_SLOT_HEADER_SIZE 16

static inline void jsbc_slot_fence(void)
{
#if defined(__riscv)
    __asm__ __volatile__("fence rw, rw" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

static inline uint32_t jsbc_slot_checksum(const uint8_t *p, uint32_t n)
{
    uint32_t s = 0;
    while (n--)
        s += *p++;
    return s;
}

#endif /* JSBYTECODE_SLOT_H */
