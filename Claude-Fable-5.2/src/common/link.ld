/* One RWX segment: [ELF container][code][rodata][data] | [bss] */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)
SECTIONS {
  . = 0x400000;
  .all : {
    KEEP(*(.hdr))
    *(.text.start)
    *(.text .text.*)
    *(.rodata .rodata.* .rodata.cst*)
    *(.data .data.*)
    KEEP(*(.impnames))
  }
  _file_end = .;
  .bss (NOLOAD) : {
    KEEP(*(.imp))
    *(.bss .bss.* COMMON)
  }
  _mem_end = .;
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.gnu*) *(.interp) *(.dynamic) *(.dynsym) *(.dynstr) *(.hash) *(.rela*) *(.got*) *(.plt*) }
}
