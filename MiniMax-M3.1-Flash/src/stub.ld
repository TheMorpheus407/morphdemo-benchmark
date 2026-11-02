/* stub.ld -- layout of the self extracting four kilobyte binary.
 *
 * The wrapper is a static program: it needs no interpreter, no dynamic table
 * and no library, so the file is only as large as the code and the payload.
 */
ENTRY(_start)

PHDRS {
  load PT_LOAD FLAGS(7);
  stack PT_GNU_STACK FLAGS(6);
}

SECTIONS {
  . = 0x400000 + SIZEOF_HEADERS;

  .text    : { *(.text .text.*) }            :load
  .rodata  : { *(.rodata .rodata.*) }        :load
  .data    : { *(.data .data.*) }            :load
  .bss     : { *(.bss .bss.*) *(COMMON) }    :load

  /DISCARD/ : {
    *(.eh_frame) *(.eh_frame_hdr) *(.comment)
    *(.note*) *(.hash) *(.gnu.hash*)
  }
}
