/*
 * ============================================================================
 *  COSMOS VMX ASSEMBLY - Low-Level VMX Instruction Wrappers
 * ============================================================================
 *
 *  This file contains low-level assembly routines for VMX instructions
 *  that cannot be executed directly from C/C++. All functions conform
 *  to System V AMD64 ABI (Linux/CosmOS calling convention).
 *
 *  Calling Convention:
 *    RDI = 1st argument
 *    RSI = 2nd argument
 *    RDX = 3rd argument
 *    RCX = 4th argument
 *    RAX = Return Value
 *
 *  VMX Instruction Status:
 *    CF=1: VMfailInvalid (no active VMCS)
 *    ZF=1: VMfailValid   (error code in VM-instruction error field)
 *    Both 0: Success
 * ============================================================================
 */

.intel_syntax noprefix
.text

/* ============================================================================
 *  cosmos_vmxon - Enable VMX Root Operation
 *
 *  uint8_t cosmos_vmxon(uint64_t* vmxon_region_phys_ptr)
 *
 *  RDI = Pointer to 64-bit physical address of VMXON region
 *  Returns: 0 = Success, 1 = Error
 * ============================================================================ */
.global cosmos_vmxon
cosmos_vmxon:
    vmxon [rdi]
    jbe .vmxon_fail          # Jump if CF=1 OR ZF=1
    xor eax, eax             # Return 0 (Success)
    ret
.vmxon_fail:
    mov eax, 1               # Return 1 (Error)
    ret

/* ============================================================================
 *  cosmos_vmxoff - Disable VMX Root Operation
 *
 *  uint8_t cosmos_vmxoff(void)
 *  Returns: 0 = Success, 1 = Error
 * ============================================================================ */
.global cosmos_vmxoff
cosmos_vmxoff:
    vmxoff
    jbe .vmxoff_fail
    xor eax, eax
    ret
.vmxoff_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmptrld - Load VMCS as active
 *
 *  uint8_t cosmos_vmptrld(uint64_t* vmcs_phys_ptr)
 *  RDI = Pointer to 64-bit physical address of VMCS
 *  Returns: 0 = Success, 1 = Error
 * ============================================================================ */
.global cosmos_vmptrld
cosmos_vmptrld:
    vmptrld [rdi]
    jbe .vmptrld_fail
    xor eax, eax
    ret
.vmptrld_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmclear - Clear VMCS and mark as inactive
 *
 *  uint8_t cosmos_vmclear(uint64_t* vmcs_phys_ptr)
 *  RDI = Pointer to 64-bit physical address of VMCS
 *  Returns: 0 = Success, 1 = Error
 * ============================================================================ */
.global cosmos_vmclear
cosmos_vmclear:
    vmclear [rdi]
    jbe .vmclear_fail
    xor eax, eax
    ret
.vmclear_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmread - Read a field from active VMCS
 *
 *  uint8_t cosmos_vmread(uint64_t field, uint64_t* value)
 *  RDI = VMCS Field Encoding
 *  RSI = Pointer to 64-bit destination for read value
 *  Returns: 0 = Success, 1 = Error
 * ============================================================================ */
.global cosmos_vmread
cosmos_vmread:
    vmread [rsi], rdi
    jbe .vmread_fail
    xor eax, eax
    ret
.vmread_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmwrite - Write a value to a VMCS field
 *
 *  uint8_t cosmos_vmwrite(uint64_t field, uint64_t value)
 *  RDI = VMCS Field Encoding
 *  RSI = Value to write
 *  Returns: 0 = Success, 1 = Error
 * ============================================================================ */
.global cosmos_vmwrite
cosmos_vmwrite:
    vmwrite rdi, rsi
    jbe .vmwrite_fail
    xor eax, eax
    ret
.vmwrite_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmlaunch_trampoline - Launch guest for the first time
 *
 *  void cosmos_vmlaunch_trampoline(vmx_guest_regs* guest_regs)
 *
 *  RDI = Pointer to vmx_guest_regs structure
 *
 *  This function:
 *  1. Saves all host registers on the stack
 *  2. Loads all guest registers from the structure
 *  3. Executes VMLAUNCH
 *  4. On VM-Exit execution automatically resumes at cosmos_vmexit_stub
 *
 *  IMPORTANT: Host RSP is stored in VMCS pointing to
 *  cosmos_vmexit_stub. On every VM-Exit the CPU jumps there.
 * ============================================================================ */
.global cosmos_vmlaunch_trampoline
cosmos_vmlaunch_trampoline:
    # Save host state (callee-saved per System V ABI)
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    # Save RDI (guest_regs pointer) in R15 (callee-saved)
    mov r15, rdi

    # Load all guest GPRs from structure
    # Order: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8-r15
    mov rax, [r15 + 0x00]    # guest_regs->rax
    mov rbx, [r15 + 0x08]    # guest_regs->rbx
    mov rcx, [r15 + 0x10]    # guest_regs->rcx
    mov rdx, [r15 + 0x18]    # guest_regs->rdx
    mov rsi, [r15 + 0x20]    # guest_regs->rsi
    mov rdi, [r15 + 0x28]    # guest_regs->rdi
    mov rbp, [r15 + 0x30]    # guest_regs->rbp
    mov r8,  [r15 + 0x38]    # guest_regs->r8
    mov r9,  [r15 + 0x40]    # guest_regs->r9
    mov r10, [r15 + 0x48]    # guest_regs->r10
    mov r11, [r15 + 0x50]    # guest_regs->r11
    mov r12, [r15 + 0x58]    # guest_regs->r12
    mov r13, [r15 + 0x60]    # guest_regs->r13
    mov r14, [r15 + 0x68]    # guest_regs->r14
    # Load R15 LAST because it is used as pointer
    mov r15, [r15 + 0x70]    # guest_regs->r15

    # VMLAUNCH! From here on the guest executes.
    # On success this instruction does NOT return.
    # On failure it falls through to next instruction.
    vmlaunch

    # If we land here, VMLAUNCH failed!
    # Error handling: restore host state and return
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

/* ============================================================================
 *  cosmos_vmresume_trampoline - Resume guest (after VM-Exit)
 *
 *  void cosmos_vmresume_trampoline(vmx_guest_regs* guest_regs)
 *  Identical to vmlaunch_trampoline, but uses VMRESUME instead of VMLAUNCH.
 * ============================================================================ */
.global cosmos_vmresume_trampoline
cosmos_vmresume_trampoline:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r15, rdi

    mov rax, [r15 + 0x00]
    mov rbx, [r15 + 0x08]
    mov rcx, [r15 + 0x10]
    mov rdx, [r15 + 0x18]
    mov rsi, [r15 + 0x20]
    mov rdi, [r15 + 0x28]
    mov rbp, [r15 + 0x30]
    mov r8,  [r15 + 0x38]
    mov r9,  [r15 + 0x40]
    mov r10, [r15 + 0x48]
    mov r11, [r15 + 0x50]
    mov r12, [r15 + 0x58]
    mov r13, [r15 + 0x60]
    mov r14, [r15 + 0x68]
    mov r15, [r15 + 0x70]

    vmresume

    # VMRESUME failed
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

/* ============================================================================
 *  cosmos_vmexit_stub - VM-Exit Entry Point
 *
 *  CPU lands HERE on EVERY VM-Exit!
 *
 *  VMCS Host-RIP points to this address.
 *  VMCS Host-RSP points to a prepared stack.
 *
 *  Workflow:
 *  1. Save all guest registers to stack (CPU restores only RSP/RIP)
 *  2. Invoke C++ handler: cosmos_vmexit_handler(vmx_guest_regs*)
 *  3. Restore guest registers
 *  4. Execute VMRESUME
 * ============================================================================ */
.global cosmos_vmexit_stub
cosmos_vmexit_stub:
    # Save guest registers on stack (in reverse order!)
    # Build vmx_guest_regs structure on stack.
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    # RSP now points to vmx_guest_regs structure
    mov rdi, rsp              # 1st argument for cosmos_vmexit_handler

    # Align stack to 16 bytes (System V ABI)
    # RSP pushed 15 * 8 = 120 bytes → RSP is 8-byte aligned
    # We need 16-byte alignment for CALL
    sub rsp, 8

    # Call C++ VM-Exit Handler
    call cosmos_vmexit_handler

    # Revert stack alignment
    add rsp, 8

    # RAX contains return value from cosmos_vmexit_handler (0 = resume, 1 = abort)
    test eax, eax
    jz .do_resume
    
    # Abort requested! Trigger kernel panic (Invalid Opcode).
    # Halts silent freeze and prints a trace to dmesg!
    ud2

.do_resume:
    # Restore guest registers
    # Note: guest_regs on stack may have been overwritten by C++
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    # Return to guest! VMRESUME continues execution.
    vmresume

    # If VMRESUME fails, we land here.
    # In this case: HLT loop (emergency stop)
    cli
.vmexit_halt_loop:
    hlt
    jmp .vmexit_halt_loop

/* ============================================================================
 *  cosmos_invept - Invalidate EPT Translations
 *
 *  void cosmos_invept(uint64_t type, uint64_t eptp)
 *  RDI = Invalidation Type (1=single-context, 2=global)
 *  RSI = EPTP (Extended Page Table Pointer)
 * ============================================================================ */
.global cosmos_invept
cosmos_invept:
    # INVEPT expects: type in register, 128-bit descriptor in memory
    # Descriptor: [EPTP (64-bit)] [Reserved (64-bit)]
    sub rsp, 16
    mov [rsp], rsi            # EPTP
    mov qword ptr [rsp + 8], 0  # Reserved
    invept rdi, [rsp]
    add rsp, 16
    ret

/* ============================================================================
 *  cosmos_invvpid - Invalidate VPID Translations
 *
 *  void cosmos_invvpid(uint64_t type, uint64_t vpid, uint64_t linear_addr)
 *  RDI = Invalidation Type
 *  RSI = VPID
 *  RDX = Linear Address
 * ============================================================================ */
.global cosmos_invvpid
cosmos_invvpid:
    sub rsp, 16
    mov [rsp], rsi            # VPID
    mov [rsp + 8], rdx        # Linear Address
    invvpid rdi, [rsp]
    add rsp, 16
    ret
