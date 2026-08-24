/* ZZRastan Core1 linker, XX19 / XACP v1.5. ASCII only. */
ENTRY(core1_entry)
SECTIONS
{
    . = 0x04500000;
    .text : { KEEP(*(.text.core1_entry)) *(.text*) *(.rodata*) . = ALIGN(4); }
    /* C++ static constructors (ymfm objects). Without these the globals are never constructed
       and Core1 faults as soon as they are used. */
    .init_array : {
        . = ALIGN(4);
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
        . = ALIGN(4);
    }
    .data : { *(.data*) . = ALIGN(4); }
    _blob_end = .;
    ASSERT(_blob_end <= 0x046F0000,
        "ZZRastan blob reaches shared at 0x04700000.")

    .bss 0x22000000 (NOLOAD) : {
        _bss_start = .;
        *(.bss*) *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    }
    ASSERT(_bss_end <= 0x22400000,
        "ZZRastan BSS exceeds 0x22000000..0x22400000.")

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.note*) *(.comment*) }
}
