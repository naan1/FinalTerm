OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    .text : {
        *(.text.init)
        *(.text)
    } > RAM

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > RAM

    .data : {
        . = ALIGN(4096);
        _data_start = .;
        *(.sdata)
        *(.data)
        *(.data.*)
        _data_end = .;
    } > RAM

    .bss : {
        . = ALIGN(4096);
        _bss_start = .;
        *(.sbss)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    } > RAM

    . = ALIGN(16);
    . += 4096; /* 4KB stack per hart */
    _stack_end = .;
}
