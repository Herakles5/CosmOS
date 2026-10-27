/*
 * ============================================================================
 *  COSMOS ANTI-CHEAT HARDENING (STEALTH MANAGER)
 * ============================================================================
 *
 *  Phase 2.5: Anti-Cheat Hardening
 *
 *  This component makes the hypervisor completely invisible to
 *  anti-cheat software such as Easy Anti-Cheat (EAC), BattlEye, and Vanguard.
 *  
 *  Core Mechanisms:
 *  1. TSC Offsetting: Hides the execution time consumed by VM-exits.
 *  2. CR Spoofing: Presents "clean" control registers to the guest.
 *  3. MSR Filtering: Traps and spoofs sensitive MSR accesses (e.g. IA32_FEATURE_CONTROL).
 * ============================================================================
 */

#ifndef COSMOS_ANTI_CHEAT_H
#define COSMOS_ANTI_CHEAT_H

#include <stdint.h>
#include "cosmos_hypervisor.h"

// Average cost of a VM-exit in CPU clock cycles (hardware-dependent)
// i7-7700 Kaby Lake: ~750-1200 cycles. We use a dynamic profile.
#define VMEXIT_COST_CYCLES_CPUID    1050
#define VMEXIT_COST_CYCLES_RDMSR    900

// API
void cosmos_ac_init(cosmos_hypervisor* hv);

// Hides exit latency by adjusting the TSC offset in the VMCS
void cosmos_ac_hide_vmexit_time(cosmos_vcpu* vcpu, uint64_t entry_tsc, uint64_t exit_tsc, uint64_t simulated_cost);

// Performs advanced MSR filtering and spoofing for anti-cheat detection routines
bool cosmos_ac_handle_rdmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, uint32_t msr, uint64_t* out_value);

// Prepares VMCS fields for maximum stealth operation
void cosmos_ac_configure_vmcs_stealth(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);

#endif // COSMOS_ANTI_CHEAT_H
