PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
  dynamic PT_DYNAMIC;
}
SECTIONS
{
  . = 0xb0;
  .interp            : { *(.interp) }            :text
  .hash              : { *(.hash) }              :text
  .gnu.hash          : { *(.gnu.hash) }          :text
  .dynsym            : { *(.dynsym) }            :text
  .dynstr            : { *(.dynstr) }            :text
  .rela.plt          : { *(.rela.plt) }          :text
  .plt               : { *(.plt) }               :text
  .text              : { *(.text) }              :text
  .rodata            : { *(.rodata) }            :text
  .eh_frame          : { *(.eh_frame) }          :text
  .note.gnu.property : { *(.note.gnu.property) } :text
  .dynamic           : { *(.dynamic) }           :text
  . = ALIGN(0x1000);
  .got.plt           : { *(.got.plt) }           :data
  .data              : { *(.data) }              :data
  .bss               : { *(.bss) }               :data
  /DISCARD/ : { *(.comment) *(.note.GNU-stack) *(.eh_frame_hdr) }
}