ENTRY(_start)
PHDRS {
    all PT_LOAD FLAGS(7);
}
SECTIONS {
    . = 0x400000 + SIZEOF_HEADERS;
    .all : {
        *(.text*)
        *(.rodata*)
        *(.data*)
    } :all
    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    } :all
    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
    }
}
