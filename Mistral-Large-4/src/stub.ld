/*
 * stub.ld — layout for the morphdemo-4k self-decompressing stub:
 * a fully static ELF (no interpreter, no dynamic section), one RWX
 * PT_LOAD.  There is no PT_GNU_STACK (the stack defaults to executable;
 * saves one 56-byte program header).
 */
ENTRY(_start)

PHDRS
{
  load  PT_LOAD FILEHDR PHDRS FLAGS(7);
}

SECTIONS
{
  . = 0x400000 + SIZEOF_HEADERS;   /* ehdr(64) + 1 phdr(56) = 0x78, contiguous */
  .text   : { *(.text .text.*) }                     :load
  .rodata : { *(.rodata .rodata.*) }                 :load
  .data   : { *(.data .data.*) }                     :load
  .bss    : { *(.bss .bss.*) *(COMMON) }             :load
  /DISCARD/ : { *(.note*) *(.eh_frame*) *(.comment) *(.gnu*) *(.interp) }
}
