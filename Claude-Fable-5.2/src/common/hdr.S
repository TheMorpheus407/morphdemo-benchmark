/* Hand-written ELF64 container: header, program headers, interpreter and the
 * minimal dynamic section needed to get dlopen/dlsym resolved by ld.so.
 * Everything else is imported at runtime by name through those two.
 * Linked first by link.ld; the final file is the flat binary image.          */
        .section .hdr,"awx",@progbits
        .globl  ehdr, imp_dlopen, imp_dlsym
ehdr:   .byte   0x7f,'E','L','F',2,1,1,0
        .fill   8,1,0
        .word   2                       /* ET_EXEC   */
        .word   62                      /* x86-64    */
        .long   1
        .quad   _start
        .quad   phdr-ehdr
        .quad   0
        .long   0
        .word   64,56
        .word   (phdr_end-phdr)/56
        .word   64,0,0
phdr:   .long   3,4                     /* PT_INTERP */
        .quad   interp-ehdr,interp,interp,interp_end-interp,interp_end-interp,1
        .long   1,7                     /* PT_LOAD rwx */
        .quad   0,ehdr,ehdr,_file_end-ehdr,_mem_end-ehdr,0x1000
        .long   2,6                     /* PT_DYNAMIC */
        .quad   dynamic-ehdr,dynamic,dynamic,dynamic_end-dynamic,dynamic_end-dynamic,8
#ifndef NO_GNU_STACK
        .long   0x6474e551,6            /* PT_GNU_STACK rw */
        .quad   0,0,0,0,0,16
#endif
phdr_end:
interp: .asciz  "/lib64/ld-linux-x86-64.so.2"
interp_end:
        .balign 8
dynamic:.quad   1,str_libc-dynstr       /* DT_NEEDED  */
        .quad   5,dynstr                /* DT_STRTAB  */
        .quad   6,dynsym                /* DT_SYMTAB  */
        .quad   10,dynstr_end-dynstr    /* DT_STRSZ   */
        .quad   11,24                   /* DT_SYMENT  */
        .quad   7,rela                  /* DT_RELA    */
        .quad   8,rela_end-rela         /* DT_RELASZ  */
        .quad   9,24                    /* DT_RELAENT */
        .quad   0x6ffffffb,1            /* DT_FLAGS_1 = NOW */
        .quad   0,0
dynamic_end:
dynsym: .fill   24,1,0
        .long   str_dlopen-dynstr
        .byte   0x12,0
        .word   0
        .quad   0,0
        .long   str_dlsym-dynstr
        .byte   0x12,0
        .word   0
        .quad   0,0
rela:   .quad   imp_dlopen,(1<<32)|6,0  /* R_X86_64_GLOB_DAT */
        .quad   imp_dlsym,(2<<32)|6,0
rela_end:
imp_dlopen: .quad 0
imp_dlsym:  .quad 0
dynstr: .byte   0
str_libc:   .asciz "libc.so.6"
str_dlopen: .asciz "dlopen"
str_dlsym:  .asciz "dlsym"
dynstr_end:
