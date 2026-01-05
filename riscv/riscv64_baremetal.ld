/* RISC-V 64-bit baremetal linker script */
OUTPUT_ARCH("riscv")
ENTRY(_start)

PROVIDE(__heap_size = 16M);

MEMORY
{
    RAM (rwxai) : ORIGIN = 0x80000000, LENGTH = 4M
}

SECTIONS
{
    . = 0x80000000;

    .text : {
        *(.text.init)
        *(.text*)
        . = ALIGN(8);
    } > RAM

    PROVIDE(__global_pointer$ = . + 0x800);

    .rodata : {
        *(.rodata*)
        . = ALIGN(8);
    } > RAM

    .data : {
        *(.data*)
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

    /* Export heap symbols */
    PROVIDE(__heap_start = .);

    .heap : {
        . = . + __heap_size;
        PROVIDE(__heap_end = .);
    } > RAM

    /* Set stack top to end of RAM */
    PROVIDE(__stack_top = 0x80000000 + 512K);
    
    _end = .;
}
