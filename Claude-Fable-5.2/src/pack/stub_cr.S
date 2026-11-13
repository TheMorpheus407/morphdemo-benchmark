/* stub_cr.S - self-extracting stub, "CR" codec (count-ratio context mixing).
 *
 * File layout:   [ELF header][program header][decoder code][model list][compressed stream][zero padding]
 * Memory layout (one RWX PT_LOAD, everything except the file image is zero-fill BSS):
 *   VBASE              file image (FILESIZE bytes), followed by one untouched zero page
 *   0x80000000         hash table, 2^(31-HS) bytes of 2-byte slots (n0, n1)
 *   0x100000000-LEN    output buffer (the decompressed payload ELF), ends exactly at 4 GiB
 *   0x100000000        one zero page: empty string for memfd_create/execveat, slack for write()
 *
 * The decoder mirrors cmpack.c exactly (see the description there).  After decoding it does
 *   fd = memfd_create("", MFD_EXEC); write(fd, payload, ...); execveat(fd, "", argv, envp, AT_EMPTY_PATH)
 * with the original argv/envp taken from the initial stack.  Only raw syscalls, no libraries.
 *
 * Build parameters (cpp -D): FILESIZE LEN NMOD CSTART BASEP HS UPD TINYHDR HARNESS
 *   TINYHDR=1  put the first bytes of code into the unused p_paddr / p_align program header fields
 *              and use the 16-bit syscall-number load (needs LEN+4096 < 65536)       [4k variant]
 *   HARNESS=1  test build: instead of memfd+exec, write the decoded payload to stdout and exit(0)
 * Registers on entry (Linux x86-64 ELF_PLAT_INIT): all general registers are zero except rsp.
 */
        .intel_syntax noprefix
        .code64
        .text

#define VBASE   (0x80000000 - 0x1000 - FILESIZE)
#define OUTEND  0x100000000
#define OUT     (OUTEND - LEN)
#define CTOP    (NMOD - 1 + CSTART)

base:
ehdr:   .byte   0x7f, 'E', 'L', 'F', 2, 1, 1, 0         /* ELFCLASS64, little endian, SYSV */
        .quad   0
        .word   2                                       /* ET_EXEC */
        .word   0x3e                                    /* EM_X86_64 */
        .long   1
        .quad   _start
        .quad   phdr - base
        .quad   0                                       /* no section headers */
        .long   0
        .word   0x40, 0x38, 1, 0x40, 0, 0
phdr:   .long   1                                       /* PT_LOAD */
        .long   7                                       /* RWX */
        .quad   0
        .quad   base
#if TINYHDR
_start: mov     edi, OUT & 0xffffffff                   /* p_paddr (ignored by the kernel): 8 bytes of code */
        push    rdi
        jmp     short cont1
#else
        .quad   base
#endif
        .quad   filesize
        .quad   OUTEND + 0x1000 - VBASE                 /* p_memsz */
#if TINYHDR
cont1:                                                  /* p_align (ignored for ET_EXEC): 8 bytes of code */
#else
        .quad   0x1000
_start: mov     edi, OUT & 0xffffffff
        push    rdi
#endif
        inc     ebx                                     /* partial = 1 */
        inc     r8d                                     /* range = 1 (value r9 = 0, bit position r10 = 0) */

/* ---- per bit ------------------------------------------------------------------------------ */
bitloop:
        mov     esi, offset models
        push    CTOP
        pop     rdx                                     /* model counter c */
        push    BASEP
        pop     rbp                                     /* n0 accumulator */
        mov     r11d, ebp                               /* n1 accumulator */
model:  lodsb                                           /* context mask */
        mov     bh, al
        mov     eax, [rsi-4]                            /* hash seed: dword ending at the mask byte */
        push    rdi
hl:     dec     edi
        shr     bh, 1
        jnc     hn
        crc32   eax, byte ptr [rdi]                     /* (flags untouched: ZF still from shr) */
hn:     jnz     hl
        crc32   eax, bl                                 /* partial byte */
        shr     eax, HS
        and     al, 0xfc
        stc
        rcr     eax, 1                                  /* slot address = 0x80000000 + 2*index */
        mov     ecx, edx
        shr     ecx, 1                                  /* weight shift = c >> 1 */
        movzx   edi, byte ptr [rax]
        shl     edi, cl
        add     ebp, edi
        movzx   edi, byte ptr [rax+1]
        shl     edi, cl
        add     r11d, edi
        pop     rdi
        push    rax                                     /* remember the slot for the update */
        dec     edx
#if CSTART
        jnz     model
#else
        jns     model
#endif
/* ---- normalise: shift compressed bits in until range >= 2^31 (rsi = start of the stream) ---- */
norm:   test    r8d, r8d
        js      nd
        bt      [rsi], r10d
        inc     r10d
        adc     r9d, r9d
        add     r8d, r8d
        jmp     norm
hop:    jmp     bitloop                                 /* relay for the short backward jumps below */
/* ---- decode one bit: threshold = range * n0 / (n0 + n1) -------------------------------------- */
nd:     add     r11d, ebp
        mov     eax, r8d
        mul     ebp
        div     r11d
        sub     r8d, eax
        sub     r9d, eax
        jnc     one
        add     r9d, eax                                /* bit 0: value unchanged, range = threshold (leaves CF=1) */
        mov     r8d, eax
one:    sbb     edx, edx
        inc     edx                                     /* edx = bit */
        cmc
        adc     ebx, ebx                                /* partial = partial*2 + bit */
/* ---- update the counters of all models -------------------------------------------------------- */
        push    NMOD
        pop     rcx
upd:    pop     rax
        lea     eax, [rax+rdx]
        inc     byte ptr [rax]                          /* n[bit]++ */
        xor     al, 1
#if UPD == 2
        inc     byte ptr [rax]                          /* n[!bit] = (n+1) >> 1 */
        shr     byte ptr [rax], 1
#else
        cmp     byte ptr [rax], 2                       /* n[!bit] = n > 1 ? n >> 1 : n */
        jb      1f
        shr     byte ptr [rax], 1
1:
#endif
        loop    upd
        shr     bh, 1                                   /* 8 bits done? */
        jnc     hop
        mov     al, bl
        stosb
        mov     bl, 1
        test    edi, edi                                /* output ends exactly at 4 GiB */
        jnz     hop

/* ---- done: hand the payload to the kernel ------------------------------------------------------ */
#if HARNESS
        pop     rsi                                     /* OUT */
        mov     edx, LEN
        push    1
        pop     rdi
        mov     eax, edi
        syscall                                         /* write(1, OUT, LEN) */
        xor     edi, edi
        push    60
        pop     rax
        syscall                                         /* exit(0) */
        .byte   0x00, 0x0f, 0x05                        /* same 3 bytes before the model list as the real stub */
#else
        push    rdi                                     /* rdi = 4 GiB: address of a zero byte, and a "huge" count */
        push    0x10
        pop     rsi                                     /* MFD_EXEC */
        mov     eax, 319
        syscall                                         /* memfd_create("", MFD_EXEC) */
        xchg    eax, edi                                /* edi = fd, eax = 0 */
        pop     rdx
        pop     rsi                                     /* OUT */
        inc     eax
        syscall                                         /* write(fd, OUT, 4 GiB) -> short write up to the end of the mapping */
        push    rdx
        pop     rsi                                     /* "" */
#if TINYHDR
        mov     ax, 322                                 /* (rax = bytes written < 65536) */
#else
        mov     eax, 322
#endif
        pop     rcx                                     /* argc */
        mov     rdx, rsp                                /* argv */
        lea     r10, [rsp+rcx*8+8]                      /* envp */
        mov     r8d, 0x1000                             /* AT_EMPTY_PATH */
        syscall                                         /* execveat(fd, "", argv, envp, AT_EMPTY_PATH) */
#if !TINYHDR
        push    60
        pop     rax
        syscall                                         /* exit(fd) if anything failed */
        .byte   0x00, 0x0f, 0x05
#endif
#endif
models: .incbin MODELS_BIN
stream: .incbin STREAM_BIN
stream_end:
        .org    FILESIZE                                /* zero padding up to the exact size */
filesize = . - base
