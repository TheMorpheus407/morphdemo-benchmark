/* start.s -- program entry point for the 4 KiB binary.
 *
 * The C runtime startup files are not linked in, so the process entry point
 * is set up here.  The sequence follows the one in glibc's own Scrt1.o:
 * __libc_start_main still performs the usual C library initialisation
 * (locale, stdio, environment) before main() is called.
 */
    .globl _start
    .type _start, @function
_start:
    endbr64
    xorl %ebp, %ebp
    movq %rdx, %r9          /* rtld_fini */
    popq %rsi               /* argc */
    movq %rsp, %rdx         /* argv */
    andq $-16, %rsp
    pushq %rax
    pushq %rsp              /* stack end */
    xorl %r8d, %r8d         /* fini */
    xorl %ecx, %ecx         /* init */
    leaq main(%rip), %rdi   /* main */
    call __libc_start_main@PLT
    hlt
    .size _start, .-_start
    .section .note.GNU-stack,"",@progbits
