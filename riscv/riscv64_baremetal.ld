/* RISC-V 64-bit baremetal linker script */
OUTPUT_ARCH("riscv")
ENTRY(_start)

/*
 * Compact layout in a single RAM region:
 *
 *   ORIGIN(RAM)  .text .rodata .data .bss
 *                __heap_start            heap grows up
 *                __heap_end
 *                (free)
 *                __stack_limit
 *   RAM end      __stack_top             C stack grows down
 *
 * Override with:
 *   -Wl,--defsym,__heap_size=<n>
 *   -Wl,--defsym,__stack_size=<n>
 */
PROVIDE(__heap_size = 16K);
PROVIDE(__jsbytecode_addr = 0x80100000);
PROVIDE(__jsbytecode_size = 0);
PROVIDE(__stack_size = 8K);

MEMORY
{
    RAM (rwxai) : ORIGIN = 0x80000000, LENGTH = 4M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        *(.text.init)
        *(.text*)
        . = ALIGN(8);
    } > RAM

    .rodata : {
        *(.rodata*)
        . = ALIGN(8);
    } > RAM

    .data : {
        *(.data*)
        . = ALIGN(8);
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata*)
        . = ALIGN(8);
    } > RAM

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM

    /* Heap immediately follows BSS */
    .heap (NOLOAD) : {
        . = ALIGN(16);
        PROVIDE(__heap_start = .);
        . = . + __heap_size;
        PROVIDE(__heap_end = .);
    } > RAM

    /* Bytecode / mailbox slot.
       Embedded CONFIG_BYTECODE: KEEP the .incbin image.
       CONFIG_BYTECODE_WAIT: no image, __jsbytecode_size reserves a NOBITS slot
       at --section-start=.jsbytecode=<addr> for a later loader. */
    .jsbytecode : ALIGN(8) {
        PROVIDE(__jsbytecode_start = .);
        KEEP(*(.jsbytecode))
        . = ALIGN(8);
        . = . + __jsbytecode_size;
        PROVIDE(__jsbytecode_end = .);
    } > RAM

    PROVIDE(__ram_end = ORIGIN(RAM) + LENGTH(RAM));
    PROVIDE(__stack_top = __ram_end);
    PROVIDE(__stack_limit = __stack_top - __stack_size);

    _end = .;

    ASSERT(__heap_end <= __stack_limit,
           "Error: heap overlaps reserved stack; increase RAM LENGTH or reduce __heap_size/__stack_size")
    ASSERT(SIZEOF(.jsbytecode) == 0 || __heap_end <= __jsbytecode_addr,
           "Error: heap overlaps jsbytecode; increase JSBYTECODE_ADDR or reduce HEAP_SIZE")
    ASSERT(SIZEOF(.jsbytecode) == 0 || (__jsbytecode_addr + SIZEOF(.jsbytecode)) <= __stack_limit,
           "Error: jsbytecode overlaps reserved stack")
}
