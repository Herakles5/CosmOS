/*
 * ============================================================================
 *  COSMOS ANTI-CHEAT HARDENING (STEALTH MANAGER)
 * ============================================================================
 *
 *  Phase 2.5: Anti-Cheat Hardening
 *
 *  Diese Komponente macht den Hypervisor komplett unsichtbar für
 *  Anti-Cheat Software wie Easy Anti-Cheat (EAC), BattlEye und Vanguard.
 *  
 *  Kern-Mechanismen:
 *  1. TSC-Offsetting: Versteckt die Zeit, die für einen VM-Exit benötigt wird.
 *  2. CR-Faking: Zeigt dem Gast "saubere" Control-Register.
 *  3. MSR-Filter: Blockiert verdächtige Zugriffe (z.B. IA32_FEATURE_CONTROL).
 * ============================================================================
 */

#ifndef COSMOS_ANTI_CHEAT_H
#define COSMOS_ANTI_CHEAT_H

#include <stdint.h>
#include "cosmos_hypervisor.h"

// Durchschnittliche Kosten eines VM-Exits in Taktzyklen (Hardwareabhängig)
// i7-7700 Kaby Lake: ~750-1200 Cycles. Wir nehmen einen dynamischen Wert.
#define VMEXIT_COST_CYCLES_CPUID    1050
#define VMEXIT_COST_CYCLES_RDMSR    900

// API
void cosmos_ac_init(cosmos_hypervisor* hv);

// Versteckt die Exit-Dauer durch Anpassen des TSC Offsets im VMCS
void cosmos_ac_hide_vmexit_time(cosmos_vcpu* vcpu, uint64_t entry_tsc, uint64_t exit_tsc, uint64_t simulated_cost);

// Führt erweiterte MSR-Checks für Anti-Cheat durch
bool cosmos_ac_handle_rdmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, uint32_t msr, uint64_t* out_value);

// Bereitet das VMCS für maximalen Stealth vor
void cosmos_ac_configure_vmcs_stealth(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);

#endif // COSMOS_ANTI_CHEAT_H
