ENTRY(core1_entry)

SECTIONS {
    . = 0x04200000;

    .text : {
        KEEP(*(.text.core1_entry))
        *(.text*)
        *(.rodata*)
    }

    .data : { *(.data*) }

    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    }
}
