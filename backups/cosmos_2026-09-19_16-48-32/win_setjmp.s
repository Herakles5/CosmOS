.global my_setjmp
.global my_longjmp

my_setjmp:
    mov %rbx, 0x00(%rcx)
    mov %rbp, 0x08(%rcx)
    mov %rdi, 0x10(%rcx)
    mov %rsi, 0x18(%rcx)
    mov %r12, 0x20(%rcx)
    mov %r13, 0x28(%rcx)
    mov %r14, 0x30(%rcx)
    mov %r15, 0x38(%rcx)
    lea 8(%rsp), %rax
    mov %rax, 0x40(%rcx)
    mov (%rsp), %rax
    mov %rax, 0x48(%rcx)
    
    # Save XMM non-volatile registers (XMM6-XMM15)
    # They are 128-bit (16 bytes) each.
    movdqu %xmm6,  0x50(%rcx)
    movdqu %xmm7,  0x60(%rcx)
    movdqu %xmm8,  0x70(%rcx)
    movdqu %xmm9,  0x80(%rcx)
    movdqu %xmm10, 0x90(%rcx)
    movdqu %xmm11, 0xA0(%rcx)
    movdqu %xmm12, 0xB0(%rcx)
    movdqu %xmm13, 0xC0(%rcx)
    movdqu %xmm14, 0xD0(%rcx)
    movdqu %xmm15, 0xE0(%rcx)

    xor %eax, %eax
    ret

my_longjmp:
    mov 0x00(%rcx), %rbx
    mov 0x08(%rcx), %rbp
    mov 0x10(%rcx), %rdi
    mov 0x18(%rcx), %rsi
    mov 0x20(%rcx), %r12
    mov 0x28(%rcx), %r13
    mov 0x30(%rcx), %r14
    mov 0x38(%rcx), %r15
    mov 0x40(%rcx), %rsp
    mov 0x48(%rcx), %r8
    
    movdqu 0x50(%rcx), %xmm6
    movdqu 0x60(%rcx), %xmm7
    movdqu 0x70(%rcx), %xmm8
    movdqu 0x80(%rcx), %xmm9
    movdqu 0x90(%rcx), %xmm10
    movdqu 0xA0(%rcx), %xmm11
    movdqu 0xB0(%rcx), %xmm12
    movdqu 0xC0(%rcx), %xmm13
    movdqu 0xD0(%rcx), %xmm14
    movdqu 0xE0(%rcx), %xmm15

    mov %edx, %eax
    test %eax, %eax
    jnz .Ldone
    mov $1, %eax
.Ldone:
    jmp *%r8
