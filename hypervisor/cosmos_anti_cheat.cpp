/*
 * ============================================================================
 *  COSMOS ANTI-CHEAT HARDENING - Implementation
 * ============================================================================
 */

#include "cosmos_anti_cheat.h"

#ifdef __linux__
#include <stdio.h>
#define AC_LOG(fmt, ...) printf("[STEALTH] " fmt "\n", ##__VA_ARGS__)
#else
extern "C" int printf(const char* fmt, ...);
#define AC_LOG(fmt, ...) printf("[STEALTH] " fmt "\n", ##__VA_ARGS__)
#endif

void cosmos_ac_init(cosmos_hypervisor* hv) {
    if (!hv) return;
    AC_LOG("Anti-Cheat Hardening Module initialized.");
}

/* 
 * Hide VM-exit execution latency:
 * Anti-cheat systems (EAC, BattlEye) often wrap CPUID with two RDTSC calls:
 *   t1 = rdtsc()
 *   cpuid()
 *   t2 = rdtsc()
 * If (t2 - t1) is abnormally high (e.g. 2000 cycles instead of 200),
 * the anti-cheat infers that cpuid() triggered a hypervisor VM-exit.
 *
 * Solution: Adjust VMCS TSC offset dynamically so that the elapsed
 * hypervisor exit processing time "vanishes" from the guest perspective.
 */
void cosmos_ac_hide_vmexit_time(cosmos_vcpu* vcpu, uint64_t entry_tsc, uint64_t exit_tsc, uint64_t simulated_cost) {
    // Actual elapsed execution cycles spent in hypervisor
    uint64_t actual_cost = exit_tsc - entry_tsc;
    
    // If exit completed unexpectedly fast, do not adjust
    if (actual_cost <= simulated_cost) return;
    
    // Net difference to hide from guest
    uint64_t time_to_hide = actual_cost - simulated_cost;
    
    // Read current TSC offset from VMCS
    uint64_t current_offset = 0;
    cosmos_vmread(VMCS_TSC_OFFSET, &current_offset);
    
    // TSC offset is added to actual hardware TSC.
    // To slow down / rewind guest-perceived time, decrement offset.
    current_offset -= time_to_hide;
    
    cosmos_vmwrite(VMCS_TSC_OFFSET, current_offset);
}

/*
 * MSR Filtering for Advanced Anti-Cheat Detection
 */
bool cosmos_ac_handle_rdmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, uint32_t msr, uint64_t* out_value) {
    (void)hv;
    (void)vcpu;
    
    switch (msr) {
        case IA32_FEATURE_CONTROL:
            // EAC reads IA32_FEATURE_CONTROL. If bit 2 (VMX outside SMX) is set,
            // the anti-cheat deduces that VMX is enabled in BIOS.
            // Many hypervisors neglect to mask this bit for guest visibility!
            *out_value = cosmos_rdmsr(IA32_FEATURE_CONTROL) & ~(1ULL << 2);
            return true;
            
        case 0xC0000082: // IA32_LSTAR (Syscall Entry Point)
            // Some anti-cheats inspect whether LSTAR has been hooked or altered
            *out_value = cosmos_rdmsr(0xC0000082);
            return true;
            
        // Fall back to default MSR handler for unmonitored registers
        default:
            return false;
    }
}

/*
 * Configure VMCS fields for maximum stealth
 */
void cosmos_ac_configure_vmcs_stealth(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    (void)hv;
    (void)vcpu;
    
    // Disabling RDTSC exits and enabling TSC offsetting
    // is safely handled by cosmos_hv_setup_vmcs() (cosmos_hypervisor.cpp)
    // while honoring hardware capability MSR bounds.
    
    AC_LOG("VMCS stealth configuration verified and managed by core.");
}

static inline void msr_bitmap_set_rw(uint8_t* msr_bitmap, uint32_t msr) {
    uint32_t read_offset = 0;
    uint32_t write_offset = 2048;
    uint32_t idx = msr;

    if (msr >= 0xC0000000U && msr <= 0xC0001FFFU) {
        read_offset = 1024;
        write_offset = 3072;
        idx = msr - 0xC0000000U;
    } else if (msr > 0x1FFFU) {
        return;
    }

    msr_bitmap[read_offset + (idx >> 3)] |= (uint8_t)(1U << (idx & 7U));
    msr_bitmap[write_offset + (idx >> 3)] |= (uint8_t)(1U << (idx & 7U));
}

void cosmos_ac_init_msr_bitmap(uint8_t* msr_bitmap) {
    if (!msr_bitmap) return;

    // Trap reads/writes to IA32_FEATURE_CONTROL and VMX capability MSRs (0x480..0x491)
    msr_bitmap_set_rw(msr_bitmap, IA32_FEATURE_CONTROL);
    for (uint32_t msr = IA32_VMX_BASIC; msr <= 0x0491U; ++msr) {
        msr_bitmap_set_rw(msr_bitmap, msr);
    }
}
