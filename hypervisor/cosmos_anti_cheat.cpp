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
    AC_LOG("Anti-Cheat Hardening Module initialisiert.");
}

/* 
 * VM-Exit Zeit verstecken: 
 * Anti-Cheats (EAC, BattlEye) benutzen oft die Instruktion CPUID umarmt
 * von zwei RDTSC Aufrufen:
 *   t1 = rdtsc()
 *   cpuid()
 *   t2 = rdtsc()
 * Wenn (t2 - t1) ungewöhnlich groß ist (z.B. 2000 Cycles statt 200),
 * weiß das AC, dass cpuid() abgefangen wurde (VM-Exit).
 *
 * Lösung: Wir manipulieren den TSC-Offset im VMCS so, dass die gemessene
 * Zeit für den VM-Exit "verschwindet".
 */
void cosmos_ac_hide_vmexit_time(cosmos_vcpu* vcpu, uint64_t entry_tsc, uint64_t exit_tsc, uint64_t simulated_cost) {
    // Die echte Zeit, die im Hypervisor verbracht wurde
    uint64_t actual_cost = exit_tsc - entry_tsc;
    
    // Wenn es unerwartet schnell ging, nichts anpassen
    if (actual_cost <= simulated_cost) return;
    
    // Die Differenz, die wir verstecken müssen
    uint64_t time_to_hide = actual_cost - simulated_cost;
    
    // TSC Offset lesen, anpassen und schreiben
    uint64_t current_offset = 0;
    cosmos_vmread(VMCS_TSC_OFFSET, &current_offset);
    
    // Der TSC Offset wird auf den echten TSC addiert.
    // Um die Zeit für den Gast "anzuhalten" (zu verringern),
    // müssen wir den Offset reduzieren (negativ machen).
    current_offset -= time_to_hide;
    
    cosmos_vmwrite(VMCS_TSC_OFFSET, current_offset);
}

/*
 * MSR-Filter für fortgeschrittene Anti-Cheats
 */
bool cosmos_ac_handle_rdmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, uint32_t msr, uint64_t* out_value) {
    (void)hv;
    (void)vcpu;
    
    switch (msr) {
        case IA32_FEATURE_CONTROL:
            // EAC liest IA32_FEATURE_CONTROL. Wenn Bit 2 (VMX outside SMX) gesetzt ist,
            // weiß das AC, dass VMX im BIOS aktiviert wurde.
            // Viele Hypervisoren vergessen, dieses Bit für den Gast zu verstecken!
            *out_value = cosmos_rdmsr(IA32_FEATURE_CONTROL) & ~(1ULL << 2);
            return true;
            
        case 0xC0000082: // IA32_LSTAR (Syscall Entry Point)
            // Manche ACs prüfen ob LSTAR manipuliert wurde
            *out_value = cosmos_rdmsr(0xC0000082);
            return true;
            
        // Wenn es kein spezielles MSR ist, lassen wir es den normalen Handler machen
        default:
            return false;
    }
}

/*
 * Konfiguriert das VMCS für maximalen Stealth
 */
void cosmos_ac_configure_vmcs_stealth(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    (void)hv;
    (void)vcpu;
    
    // RDTSC-Exits ausschalten und TSC-Offsetting anschalten
    // wird bereits sicher und unter Berücksichtigung der Hardware-MSRs
    // in cosmos_hv_setup_vmcs() (cosmos_hypervisor.cpp) erledigt!
    // Manuelles Überschreiben hier führte zu VM-Entry Fails (System Freeze),
    // weil adjust_vmx_control() umgangen wurde und ungültige Bits gesetzt wurden.
    
    AC_LOG("VMCS Stealth ist bereits sicher durch den Core konfiguriert.");
}
