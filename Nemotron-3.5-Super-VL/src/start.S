.section .text
.globl _start
.type _start, @function
_start:
    /* align stack to 16 bytes before calling main (ABI entry state) */
    andq    $-16, %rsp
    call    main
    movl    %eax, %edi
    movb    $60, %al          /* sys_exit */
    syscall