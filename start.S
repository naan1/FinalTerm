.section .text.init
.global _start

_start:
    # Setup stack pointer for each hart
    csrr t0, mhartid
    slli t0, t0, 10
    la   sp, _stack_end
    sub  sp, sp, t0

    # Clear BSS
    la t0, _bss_start
    la t1, _bss_end
bss_clear:
    sw zero, 0(t0)
    addi t0, t0, 4
    bltu t0, t1, bss_clear

    # Jump to C code
    call main

    # Infinite loop if main returns
1:  wfi
    j 1b
