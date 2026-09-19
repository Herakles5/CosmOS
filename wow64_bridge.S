.global jump_to_32bit
.global wow64_api_thunk
.global wow64_saved_rsp

.data
wow64_saved_rsp: .quad 0
thunk_msg: .ascii "THUNK!\n"
thunk_msg2: .ascii "THUNK2!\n"
dbg_lretq: .ascii "LRETQ!\n"
dbg_sig: .ascii "SIG!\n"
dbg_wrap: .ascii "WRAP_DONE!\n"

.text
.code64

# void jump_to_32bit(uint32_t entry_point, uint32_t stack32_ptr, uint32_t ldt_selector)
jump_to_32bit:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    # Save 64-bit stack pointer to r8 BEFORE switching
    mov %rsp, %r8

    # Switch to 32-bit stack
    mov %esi, %esp

    # Set DS and ES to 32-bit data segment (0x2B)
    mov $0x2B, %ax
    mov %ax, %ds
    mov %ax, %es
    
    # Set FS to LDT entry for current thread (from %edx)
    mov %dx, %ax
    mov %ax, %fs

    # Save 64-bit stack pointer into TEB + 0x1FF0
    mov %r8, %fs:0x1FF0

    # Setup lretq frame (8 bytes CS, 8 bytes RIP)
    pushq $0x23
    pushq %rdi
    
    # Zero out all registers to simulate a fresh process start!
    xor %eax, %eax
    xor %ebx, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %ebp, %ebp
    xor %esi, %esi
    xor %edi, %edi
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15

    lretq

# wow64_api_thunk (called via 32-bit far jump trampoline)
wow64_api_thunk:
    # We are in 64-bit mode now.

    # DEBUG: Print "THUNK!"
    push %rax
    push %rdi
    push %rsi
    push %rdx
    push %r11
    push %rcx
    mov $1, %eax
    mov $1, %edi
    lea thunk_msg(%rip), %rsi
    mov $7, %edx
    syscall
    pop %rcx
    pop %r11
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rax

    # Restore FS, DS, and ES selectors to 0 FIRST

    # Read glibc_fs_base from TEB + 0x1FF8 BEFORE clearing FS
    mov %fs:0x1FF8, %r14
    
    # Read 64-bit stack from TEB + 0x1FF0 BEFORE clearing FS
    mov %fs:0x1FF0, %r15

    # NOTE: Loading 0 into FS clears the hidden FS base!
    xor %r11d, %r11d
    mov %r11w, %fs
    mov %r11w, %ds
    mov %r11w, %es

    # Now restore the FS base using arch_prctl (syscall 158, ARCH_SET_FS = 0x1002)
    # Since we don't want to clobber RAX (API ID), we save it
    push %rax
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %r11
    
    mov $158, %eax
    mov $0x1002, %edi
    mov %r14, %rsi
    syscall
    
    pop %r11
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rax

    # Save 32-bit ESP to R12
    mov %esp, %r12d

    # Restore 64-bit stack from R15 (which we read from TEB before clearing FS)
    mov %r15, %rsp

    # Save 32-bit registers that might be trashed by 64-bit C++ ABI
    push %rdi
    push %rsi
    push %rbx
    push %rbp
    push %rcx
    push %rdx

    # Setup arguments for wow64_dispatch(uint32_t api_id, uint32_t* esp, uint32_t* out_pop_bytes, wow64_context* ctx)
    mov %eax, %edi
    mov %r12d, %esi

    # The context (edi, esi, ebx, ebp, ecx, edx) is currently at %rsp.
    # We will pass %rsp to %rcx.
    mov %rsp, %rcx

    # Align stack to 16 bytes for System V ABI (RSP was 16n+8, sub 24 makes it 16n)
    sub $24, %rsp
    mov %rsp, %rdx      # arg 3: out_pop_bytes
    lea 8(%rsp), %r8    # arg 5: out_ldt_selector
    call wow64_dispatch
    mov (%rsp), %r13d
    mov 8(%rsp), %r14d  # Store LDT selector in r14d
    add $24, %rsp

    # DEBUG: Print "THUNK2!"
    push %rax
    push %rdi
    push %rsi
    push %rdx
    push %r11
    push %rcx
    mov $1, %eax
    mov $1, %edi
    lea thunk_msg2(%rip), %rsi
    mov $8, %edx
    syscall
    pop %rcx
    pop %r11
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rax

    # Restore 32-bit registers
    pop %rdx
    pop %rcx
    pop %rbp
    pop %rbx
    pop %rsi
    pop %rdi

    # Restore 32-bit ESP
    mov %r12d, %esp

    # Pop the 32-bit return address from the 32-bit stack manually (4 bytes)
    mov (%esp), %r11d
    add $4, %esp

    # Pop stdcall arguments based on returned pop_bytes (now in %r13d)
    add %r13d, %esp

    # Far return to 32-bit mode
    pushq $0x23
    pushq %r11

    # Set DS, ES, and FS back to 32-bit segments before jumping
    # We must preserve EAX (API return value)
    push %rax

    mov $0x2B, %ax
    mov %ax, %ds
    mov %ax, %es

    # Set FS to LDT entry for current thread (from r14d)
    mov %r14w, %ax
    mov %ax, %fs

    # Restore EAX!
    pop %rax

    # DEBUG: Print "LRETQ!" right before the far return
    push %rax
    push %rdi
    push %rsi
    push %rdx
    push %r11
    push %rcx
    mov $1, %eax
    mov $1, %edi
    lea dbg_lretq(%rip), %rsi
    mov $7, %edx
    syscall
    pop %rcx
    pop %r11
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rax

    lretq

.global my_sig_wrapper
my_sig_wrapper:
    # DEBUG: Print "SIG!" immediately to confirm handler entry
    push %rax
    push %rdi
    push %rsi
    push %rdx
    push %r11
    push %rcx
    mov $1, %eax
    mov $1, %edi
    lea dbg_sig(%rip), %rsi
    mov $5, %edx
    syscall
    pop %rcx
    pop %r11
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rax

    # Save signal handler arguments (sig, siginfo, ucontext)
    push %rdi
    push %rsi
    push %rdx

    # Step 1: Check if glibc_fs_table is initialized
    mov glibc_fs_table(%rip), %r8
    test %r8, %r8
    jz .Lsig_skip          # If null, skip (too early)

    # Step 2: Get current thread ID (SYS_gettid = 186)
    mov $186, %eax
    syscall                # rax = tid, RCX/R11 clobbered!

    # Step 3: Check bounds and read glibc_fs_table[tid]
    cmp $4194304, %rax
    jae .Lsig_skip
    mov (%r8, %rax, 8), %rsi
    test %rsi, %rsi
    jz .Lsig_skip          # If 0, no glibc_fs saved for this thread

    # Step 4: Clear segment selectors and set FS_BASE
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %ds
    mov %ax, %es

    mov $158, %eax         # SYS_arch_prctl
    mov $0x1002, %edi      # ARCH_SET_FS
    syscall

.Lsig_skip:
    # Restore signal handler arguments
    pop %rdx
    pop %rsi
    pop %rdi

    # DEBUG: Print WRAP_DONE!
    push %rax
    push %rdi
    push %rsi
    push %rdx
    push %r11
    push %rcx
    mov $1, %eax
    mov $1, %edi
    lea dbg_wrap(%rip), %rsi
    mov $11, %edx
    syscall
    pop %rcx
    pop %r11
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rax

    jmp segfault_handler@PLT

