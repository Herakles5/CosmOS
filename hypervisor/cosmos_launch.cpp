/*
 * ============================================================================
 *  COSMOS HYPERVISOR - VM Launch & Guest Management
 * ============================================================================
 *
 *  Phase 1.6: First VM-Launch
 *
 *  This module launches the guest in the VM. It can:
 *  1. Subvert the current host into a guest (Host → becomes guest)
 *  2. Launch a separate guest kernel (e.g. Windows NT)
 *
 *  Mode 1 (Subvert Host):
 *    The hypervisor slides UNDER the running kernel.
 *    The host notices nothing and continues running normally, but every
 *    CPUID/RDTSC/MSR invocation now goes through our stealth handler.
 *    → This is the mode to "make the hypervisor invisible"
 *
 *  Mode 2 (Separate Guest):
 *    A completely separate guest (e.g. Windows) is started inside the VM.
 *    → This is the mode for "Windows with GPU passthrough"
 * ============================================================================
 */

#include "cosmos_hypervisor.h"
#include "cosmos_ept.h"

#ifdef __linux__
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#else
extern "C" int printf(const char* fmt, ...);
extern "C" void* memset(void*, int, unsigned long);
extern "C" void* memcpy(void*, const void*, unsigned long);
extern void* cosmos_alloc_page_aligned(uint64_t* out_phys);
#endif

#define LAUNCH_LOG(fmt, ...) printf("[LAUNCH] " fmt "\n", ##__VA_ARGS__)

/* ============================================================================
 *  GUEST TEST CODE
 *
 *  A minimal "guest" executing only CPUID and returning via VMCALL.
 *  Used to verify that the entire VMX cycle works.
 * ============================================================================ */

// This code is executed as a guest
// It is intentionally simple: CPUID → VMCALL → done
static void __attribute__((naked, aligned(4096))) guest_test_code(void) {
    __asm__ __volatile__(
        // Test 1: CPUID Leaf 0 (Vendor String)
        "mov $0, %%eax\n"
        "cpuid\n"
        
        // Test 2: CPUID Leaf 1 (Feature Info)
        // Stealth handler should clear Bit 31 of ECX!
        "mov $1, %%eax\n"
        "cpuid\n"
        // ECX should now have NO hypervisor bit
        // Store ECX in R12 for later check
        "mov %%ecx, %%r12d\n"
        
        // Test 3: CPUID Leaf 0x40000000 (Hypervisor Vendor)
        // Should return 0 (no hypervisor visible)
        "mov $0x40000000, %%eax\n"
        "cpuid\n"
        // EAX should be 0
        "mov %%eax, %%r13d\n"
        
        // Test 4: RDTSC (Timing Check)
        "rdtsc\n"
        "mov %%eax, %%r14d\n"  // Low 32 bits of TSC
        
        // Test 5: VMCALL - Hypercall to hypervisor
        // RAX = 0xC0DE = "Cosmos Test successful"
        "mov $0xC0DE, %%eax\n"
        "vmcall\n"
        
        // Should never reach here (VMCALL causes VM-Exit)
        "hlt\n"
        
        ::: "memory"
    );
}

// Size of guest test code (determined at runtime)
static void __attribute__((naked)) guest_test_code_end(void) {
    __asm__ __volatile__("nop\n" ::: "memory");
}

/* ============================================================================
 *  SUBVERT HOST MODE
 *
 *  Slides the hypervisor UNDER the currently running kernel.
 *  After VMLAUNCH, the kernel continues running as guest without noticing.
 *
 *  Workflow:
 *  1. Save current CPU state (all registers)
 *  2. Configure VMCS with current state as Guest State
 *  3. Set Guest RIP to return address (after VMLAUNCH)
 *  4. VMLAUNCH → Host becomes guest
 *  5. Code following VMLAUNCH now runs INSIDE GUEST
 * ============================================================================ */

// Flag indicating whether we are already running in guest
static volatile int subverted = 0;

// Return point function for subverted host
// Guest jumps here after successful VMLAUNCH
static void __attribute__((noinline)) subvert_return_point(void) {
    subverted = 1;
    LAUNCH_LOG("★★★ HOST SUCCESSFULLY SUBVERTED! ★★★");
    LAUNCH_LOG("The kernel is now running as VM guest.");
    LAUNCH_LOG("All CPUID calls route through the stealth handler.");
}

int cosmos_hv_subvert_host(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return -1;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    if (!vcpu->vmcs_active) {
        LAUNCH_LOG("CPU %u: VMCS not active!", cpu_id);
        return -1;
    }
    
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    LAUNCH_LOG("CPU %u: SUBVERT HOST MODE", cpu_id);
    LAUNCH_LOG("CPU %u: Kernel is becoming guest...", cpu_id);
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    
    uint64_t guest_rsp, guest_rip, guest_rflags;
    
    // Capture RFLAGS and RSP
    guest_rflags = cosmos_read_rflags();
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(guest_rsp));
    
    // Set Guest State
    cosmos_vmwrite(VMCS_GUEST_RFLAGS, guest_rflags | 0x2);
    cosmos_vmwrite(VMCS_GUEST_RSP, guest_rsp);
    cosmos_vmwrite(VMCS_GUEST_CR3, cosmos_read_cr3());
    
    // Set Guest RIP to local C label
    void* resume_ptr = &&guest_resume;
    guest_rip = (uint64_t)resume_ptr;
    cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip);
    
    // Capture callee-saved registers for clean guest context return
    uint64_t c_rbp, c_rbx, c_r12, c_r13, c_r14, c_r15;
    __asm__ __volatile__("mov %%rbp, %0" : "=r"(c_rbp));
    __asm__ __volatile__("mov %%rbx, %0" : "=r"(c_rbx));
    __asm__ __volatile__("mov %%r12, %0" : "=r"(c_r12));
    __asm__ __volatile__("mov %%r13, %0" : "=r"(c_r13));
    __asm__ __volatile__("mov %%r14, %0" : "=r"(c_r14));
    __asm__ __volatile__("mov %%r15, %0" : "=r"(c_r15));

    memset(&vcpu->guest_regs, 0, sizeof(vmx_guest_regs));
    vcpu->guest_regs.rbp = c_rbp;
    vcpu->guest_regs.rbx = c_rbx;
    vcpu->guest_regs.r12 = c_r12;
    vcpu->guest_regs.r13 = c_r13;
    vcpu->guest_regs.r14 = c_r14;
    vcpu->guest_regs.r15 = c_r15;
    
    LAUNCH_LOG("CPU %u: Starting VMLAUNCH...", cpu_id);
    
    // Force GCC to keep label and mark as reachable
    // so GCC does not optimize it away to the end of function!
    __asm__ goto("" : : "r"(resume_ptr) : "memory" : guest_resume);
    
    cosmos_vmlaunch_trampoline(&vcpu->guest_regs);
    
    // If we land here, VMLAUNCH FAILED
    {
        uint64_t vmx_error = 0;
        cosmos_vmread(0x4400, &vmx_error);
        LAUNCH_LOG("CPU %u: ★ VMLAUNCH FAILED! Error=%llu ★", cpu_id, (unsigned long long)vmx_error);
    }
    return -2;

guest_resume:
    __asm__ __volatile__("" ::: "memory");
    subverted = 1;
    LAUNCH_LOG("★★★ HOST SUCCESSFULLY SUBVERTED! ★★★");
    LAUNCH_LOG("The kernel is now running as VM guest.");
    return 0;
}

/* ============================================================================
 *  LAUNCH SEPARATE GUEST
 *
 *  Starts a separate guest (e.g. test code or later Windows).
 *  Guest receives its own address space via EPT.
 * ============================================================================ */
int cosmos_hv_launch_test_guest(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return -1;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    if (!vcpu->vmcs_active) {
        LAUNCH_LOG("CPU %u: VMCS not active!", cpu_id);
        return -1;
    }
    
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    LAUNCH_LOG("CPU %u: TEST GUEST LAUNCH", cpu_id);
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    
    // Guest Code Address
    uint64_t guest_entry = (uint64_t)&guest_test_code;
    uint64_t code_size = (uint64_t)&guest_test_code_end - (uint64_t)&guest_test_code;
    
    LAUNCH_LOG("CPU %u: Guest Code @ 0x%llX (%llu bytes)", cpu_id,
              (unsigned long long)guest_entry, (unsigned long long)code_size);
    
    // Set Guest RIP to test code
    cosmos_vmwrite(VMCS_GUEST_RIP, guest_entry);
    
    // Guest Stack (top of 4KB page, grows downward)
    // Allocate Guest Stack
    uint64_t guest_stack_phys;
#ifdef __linux__
    uint8_t* guest_stack = (uint8_t*)mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (guest_stack == MAP_FAILED) guest_stack = nullptr;
    else { memset(guest_stack, 0, 4096); guest_stack_phys = (uint64_t)guest_stack; }
#else
    uint8_t* guest_stack = (uint8_t*)cosmos_alloc_page_aligned(&guest_stack_phys);
#endif
    if (!guest_stack) {
        LAUNCH_LOG("CPU %u: Guest stack allocation failed!", cpu_id);
        return -2;
    }
    cosmos_vmwrite(VMCS_GUEST_RSP, (uint64_t)(guest_stack + 4096 - 8));
    
    // Initialize guest registers to 0
    memset(&vcpu->guest_regs, 0, sizeof(vmx_guest_regs));
    
    LAUNCH_LOG("CPU %u: Guest Entry: 0x%llX", cpu_id, (unsigned long long)guest_entry);
    LAUNCH_LOG("CPU %u: Guest Stack: 0x%llX", cpu_id, 
              (unsigned long long)(guest_stack + 4096 - 8));
    LAUNCH_LOG("CPU %u: Starting VMLAUNCH for test guest...", cpu_id);
    
    // ★ VMLAUNCH! ★
    cosmos_vmlaunch_trampoline(&vcpu->guest_regs);
    
    // If VMLAUNCH fails:
    uint64_t vmx_error = 0;
    cosmos_vmread(0x4400, &vmx_error);
    LAUNCH_LOG("CPU %u: VMLAUNCH failed! Error=%llu", cpu_id,
              (unsigned long long)vmx_error);
    
    // Error Codes:
    // 1 = VMCALL in VMX root operation
    // 2 = VMCLEAR with invalid address
    // 3 = VMCLEAR with VMXON pointer
    // 4 = VMLAUNCH with non-clear VMCS
    // 5 = VMRESUME with non-launched VMCS
    // 7 = VM entry with invalid control fields
    // 8 = VM entry with invalid host-state fields
    // 9 = VMPTRLD with invalid address
    // etc.
    
    const char* error_names[] = {
        "Unknown",                                      // 0
        "VMCALL in VMX root operation",                 // 1
        "VMCLEAR with invalid address",                 // 2
        "VMCLEAR with VMXON pointer",                   // 3
        "VMLAUNCH with non-clear VMCS",                 // 4
        "VMRESUME with non-launched VMCS",              // 5
        "VMRESUME after VMXOFF",                        // 6
        "VM entry with invalid control fields",         // 7
        "VM entry with invalid host-state fields",      // 8
        "VMPTRLD with invalid address",                 // 9
        "VMPTRLD with VMXON pointer",                   // 10
        "VMPTRLD with incorrect VMCS revision",         // 11
        "VMREAD/VMWRITE unsupported field",             // 12
        "VMWRITE to read-only field",                   // 13
    };
    
    if (vmx_error < 14) {
        LAUNCH_LOG("CPU %u: Error: %s", cpu_id, error_names[vmx_error]);
    }
    
    return -1;
}

/* ============================================================================
 *  HYPERVISOR FULL INITIALIZATION
 *
 *  Complete initialization in one call:
 *  1. VMX Detection
 *  2. VMX Enable (VMXON)
 *  3. VMCS Setup
 *  4. EPT Setup
 *  5. Ready to Launch!
 * ============================================================================ */
int cosmos_hv_init(cosmos_hypervisor* hv) {
    LAUNCH_LOG("╔══════════════════════════════════════════╗");
    LAUNCH_LOG("║   CosmOS Stealth Hypervisor v0.1         ║");
    LAUNCH_LOG("║   Type-1 Bare-Metal VMX Hypervisor       ║");
    LAUNCH_LOG("║   Target: Intel i7-7700 (Kaby Lake)      ║");
    LAUNCH_LOG("╚══════════════════════════════════════════╝");
    // The actual initialization (Detect, Enable, VMCS, EPT)
    // is now managed by the LKM (cosmos_lkm.c) to ensure clean thread pinning
    // across individual cores (smp_call_function).
    
    // Activate stealth mode
    hv->stealth_mode = 1;
    hv->initialized = 1;
    
    LAUNCH_LOG("╔══════════════════════════════════════════╗");
    LAUNCH_LOG("║  ★ HYPERVISOR READY!                     ║");
    LAUNCH_LOG("║  Stealth Mode: ACTIVE                    ║");
    LAUNCH_LOG("║  EPT: %s                             ║", hv->has_ept ? "ACTIVE  " : "INACTIVE");
    LAUNCH_LOG("║  VPID: %s                            ║", hv->has_vpid ? "ACTIVE  " : "INACTIVE");
    LAUNCH_LOG("╚══════════════════════════════════════════╝");
    
    return 0;
}

/* ============================================================================
 *  HYPERVISOR SHUTDOWN
 * ============================================================================ */
void cosmos_hv_shutdown(cosmos_hypervisor* hv) {
    if (!hv) return;
    
    LAUNCH_LOG("Shutting down hypervisor...");
    
    for (uint32_t cpu = 0; cpu < 256; cpu++) {
        if (hv->vcpus[cpu].vmx_enabled) {
            cosmos_hv_disable_vmx(hv, cpu);
        }
    }
    
    hv->initialized = 0;
    hv->stealth_mode = 0;
    
    LAUNCH_LOG("Hypervisor cleanly shut down.");
}
