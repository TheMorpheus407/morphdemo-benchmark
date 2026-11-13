/*
 * Compact self-contained x86-64 ELF64 + raw LZMA1 (lc=0, lp=0, pb=0) + memfd_create + execveat stub.
 */
.intel_syntax noprefix
.code64

.section .text, "ax", @progbits
.global _start

ehdr:
    .byte 0x7f, 'E', 'L', 'F'
    .byte 2, 1, 1, 0
    .quad 0
    .word 2                     /* ET_EXEC */
    .word 62                    /* EM_X86_64 */
    .long 1
    .quad _start
    .quad phdr - ehdr
    .quad 0
    .long 0
    .word 64
    .word 56
    .word 1
    .word 0, 0, 0

phdr:
    .long 1                     /* PT_LOAD */
    .long 7                     /* PF_R | PF_W | PF_X */
    .quad 0
    .quad 0x400000
    .quad 0x400000
    .quad FILE_SIZE
    .quad FILE_SIZE + 16
    .quad 0x1000

rc_norm:
    cmp r8d, 0x01000000
    jae 1f
    shl r8d, 8
    shl r9d, 8
    lodsb
    mov r9b, al
1:  ret

rc_bit:
    call rc_norm
    movzx eax, word ptr [rbx + rdx*2]
    mov ebp, r8d
    shr ebp, 11
    imul ebp, eax
    cmp r9d, ebp
    jae 2f
    mov r8d, ebp
    neg eax
    add eax, 2048
    shr eax, 5
    add [rbx + rdx*2], ax
    add ecx, ecx
    clc
    ret
2:
    sub r8d, ebp
    sub r9d, ebp
    shr eax, 5
    sub [rbx + rdx*2], ax
    lea ecx, [rcx + rcx + 1]
    stc
    ret

rc_tree:
    mov r10d, eax
    push 1
    pop rcx
1:
    push rdx
    add edx, ecx
    call rc_bit
    pop rdx
    cmp ecx, r10d
    jb 1b
    sub ecx, r10d
    ret

rc_len:
    push 2
    pop r10
    push rdx
    call rc_bit
    pop rdx
    push 8
    pop rax
    jnc 1f
    mov r10b, 10
    push rdx
    inc edx
    call rc_bit
    pop rdx
    push 8
    pop rax
    jnc 1f
    mov r10b, 18
    shl eax, 5
1:  add edx, r10d
    push r10
    call rc_tree
    pop rax
    add ecx, eax
    ret

_start:
    push rsp
    push 0
    push 0

    xor edi, edi
    mov esi, 0x1000000
    push 3
    pop rdx
    push 0x22
    pop r10
    xor r9d, r9d
    push 9
    pop rax
    syscall

    mov rbx, rax
    mov rdi, rax
    mov eax, 1024
    mov ecx, 4096
    rep stosw
    mov r13, rdi

    lea rsi, [rip + payload]
    lodsd
    lea r12, [rdi + rax]

    lodsb
    lodsd
    bswap eax
    mov r9d, eax
    or r8d, -1

    xor r11d, r11d
    xor r15d, r15d

decode_loop:
    cmp rdi, r12
    jae decode_done

    mov edx, r11d
    call rc_bit
    jc is_match_branch

    push 1
    pop rcx
    xor r10d, r10d
    cmp r11b, 7
    jb lit_loop
    bts r10d, 8
    mov rax, rdi
    sub rax, r15
    movzx r14d, byte ptr [rax - 1]
lit_loop:
    shl r14d, 1
    mov eax, r14d
    and eax, r10d
    add eax, r10d
    lea edx, [rcx + rax + 1020]
    call rc_bit
    sbb eax, eax
    xor eax, r14d
    not eax
    and r10d, eax
    cmp ecx, 0x100
    jb lit_loop
    mov al, cl
    stosb
    cmp r11b, 4
    jae 1f
    xor r11d, r11d
    jmp decode_loop
1:
    cmp r11b, 10
    jb 2f
    sub r11b, 3
2:
    sub r11b, 3
    jmp decode_loop

is_match_branch:
    lea edx, [r11 + 12]
    call rc_bit
    jnc normal_match

    lea edx, [r11 + 24]
    call rc_bit
    jc rep_g0_1

    lea edx, [r11 + 60]
    call rc_bit
    jc rep_len_decode

    cmp r11b, 7
    mov r11b, 9
    jb 1f
    mov r11b, 11
1:
    push 1
    pop rcx
    jmp copy_match

rep_g0_1:
    lea edx, [r11 + 36]
    call rc_bit
    mov eax, r15d
    xchg eax, [rsp]
    jnc rep_swap_done

    lea edx, [r11 + 48]
    push rax
    call rc_bit
    pop rax
    xchg eax, [rsp + 4]
    jnc rep_swap_done
    xchg eax, [rsp + 8]
rep_swap_done:
    mov r15d, eax

rep_len_decode:
    mov edx, 746
    call rc_len
    cmp r11b, 7
    mov r11b, 8
    jb copy_match
    mov r11b, 11
    jmp copy_match

normal_match:
    mov eax, r15d
    xchg eax, [rsp]
    xchg eax, [rsp + 4]
    mov [rsp + 8], eax

    mov edx, 472
    call rc_len
    push rcx

    cmp r11b, 7
    mov r11b, 7
    jb 1f
    mov r11b, 10
1:
    sub ecx, 2
    cmp ecx, 3
    jbe 2f
    push 3
    pop rcx
2:
    shl ecx, 6
    lea edx, [rcx + 72]
    push 64
    pop rax
    call rc_tree
    mov r15d, ecx
    cmp ecx, 4
    jb match_slot_done

    mov r10d, ecx
    shr ecx, 1
    dec ecx
    mov eax, r10d
    and eax, 1
    or eax, 2
    shl eax, cl
    mov r15d, eax

    lea edx, [r15 + 328]
    sub edx, r10d
    cmp r10d, 14
    jb rev_tree_decode

    sub ecx, 4
direct_loop:
    call rc_norm
    shr r8d, 1
    cmp r9d, r8d
    jb 1f
    sub r9d, r8d
    lea eax, [rcx + 3]
    bts r15d, eax
1:
    loop direct_loop
    mov edx, 456
    push 4
    pop rcx

rev_tree_decode:
    mov r10d, ecx
    push 1
    pop rcx
    xor eax, eax
rev_loop:
    push rax
    push rdx
    add edx, ecx
    call rc_bit
    pop rdx
    pop rax
    jnc 1f
    bts r15d, eax
1:
    inc eax
    cmp eax, r10d
    jb rev_loop

match_slot_done:
    pop rcx

copy_match:
    mov r10, rdi
    sub r10, r15
1:
    cmp rdi, r12
    jae decode_done
    mov al, [r10 - 1]
    inc r10
    stosb
    loop 1b
    jmp decode_loop

decode_done:
    push 0
    mov rdi, rsp
    push 1
    pop rsi
    mov eax, 319
    syscall
    xchg ebx, eax

    mov rsi, r13
    mov rdx, r12
    sub rdx, r13
    mov edi, ebx
    push 1
    pop rax
    syscall

    mov rbp, [rsp + 24]
    mov rsi, rsp
    lea rdx, [rbp + 8]
    mov rax, [rbp]
    lea r10, [rdx + rax*8 + 8]
    mov r8d, 0x1000
    mov eax, 322
    syscall

payload:
