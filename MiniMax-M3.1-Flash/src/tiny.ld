/* tiny.ld -- compact ELF layout for the size constrained binary.
 *
 * One RWX load segment holds every section, so the file has no page
 * alignment holes.  Unwind tables, notes and the comment section are
 * discarded; they are optional for execution.
 */
ENTRY(_start)

PHDRS {
  interp PT_INTERP;
  load   PT_LOAD FILEHDR PHDRS FLAGS(7);
  dyn    PT_DYNAMIC FLAGS(6);
}

SECTIONS {
  . = 0x400000 + SIZEOF_HEADERS;

  .interp    : { *(.interp) }                          :load :interp
  .dynsym    : { *(.dynsym) }                          :load
  .dynstr    : { *(.dynstr) }                          :load
  .gnu.hash  : { *(.gnu.hash) }                        :load
  .rela.dyn  : { *(.rela.dyn) }                        :load
  .rela.plt  : { *(.rela.plt) }                        :load
  .plt       : { *(.plt) *(.plt.got) *(.iplt) }         :load
  .text      : { *(.text .text.*) }                    :load
  .rodata    : { *(.rodata .rodata.*) }                :load
  .got       : { *(.got) *(.got.plt) }                 :load
  .data      : { *(.data .data.*) }                    :load
  /* The dynamic table is the last thing in the file: the linker leaves a few
     empty slots at its end, and tools/exact_size.py cuts them off. */
  .dynamic   : { *(.dynamic) }                         :load :dyn
  .bss       : { *(.bss .bss.*) *(COMMON) }            :load

  /DISCARD/ : {
    *(.eh_frame) *(.eh_frame_hdr) *(.comment)
    *(.note*) *(.hash) *(.gnu.hash*)
  }
}
