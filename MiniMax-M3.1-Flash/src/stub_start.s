/* stub_start.s -- entry point and payload of the self extracting wrapper.
 *
 * The compressed production is inserted verbatim into the read only data of
 * the binary; the unpacker in stub.c expands it into memory and executes it.
 */
    .section .rodata
    .globl payload_start
    .globl payload_end
    .balign 8
payload_start:
    .incbin "demo4_payload.bin"
payload_end:

    .section .text
    .globl _start
    .type _start, @function
_start:
    xorl %ebp, %ebp
    movq (%rsp), %rdi            /* argc */
    leaq 8(%rsp), %rsi           /* argv */
    leaq 16(%rsp,%rdi,8), %rdx   /* envp = argv + argc + 1 */
    call cmain
    movl $60, %eax               /* exit(0), only if cmain returns */
    xorl %edi, %edi
    syscall
    .size _start, .-_start

    .section .note.GNU-stack,"",@progbits
