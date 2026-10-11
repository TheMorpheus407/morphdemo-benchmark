/*
 * tiny.ld — minimal ELF layout for the dynamically linked demos:
 * a single RWX PT_LOAD (no page-alignment padding between segments),
 * plus PT_INTERP, PT_DYNAMIC and a non-executable PT_GNU_STACK.
 * Section headers are stripped afterwards (tools/sstrip.py); the kernel
 * only needs the program headers.
 */
ENTRY(_start)

PHDRS
{
  load   PT_LOAD FILEHDR PHDRS FLAGS(7);
  interp PT_INTERP  FLAGS(4);
  dyn    PT_DYNAMIC FLAGS(6);
  stack  PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
  . = 0x400000 + SIZEOF_HEADERS;
  .interp   : { *(.interp) }                 :load :interp
  .hash     : { *(.hash) }                   :load
  .dynsym   : { *(.dynsym) }                 :load
  .dynstr   : { *(.dynstr) }                 :load
  .rela.plt : { *(.rela.plt) *(.rela.dyn) }  :load
  .gnu.version   : { *(.gnu.version) }       :load
  .gnu.version_d : { *(.gnu.version_d) }     :load
  .gnu.version_r : { *(.gnu.version_r) }     :load
  .plt      : { *(.plt) }                    :load
  .text     : { *(.text .text.*) }           :load
  .rodata   : { *(.rodata .rodata.*) }       :load
  .dynamic  : { *(.dynamic) }                :load :dyn
  .got      : { *(.got .got.plt) }           :load
  .data     : { *(.data .data.*) }           :load
  .bss      : { *(.bss .bss.*) *(COMMON) }   :load
  /DISCARD/ : { *(.note*) *(.eh_frame*) *(.comment) *(.gnu*) }
}
