/*
 * ============================================================================
 *  COSMOS HYPERVISOR - VM Launch & Guest Management
 * ============================================================================
 *
 *  Phase 1.6: Erster VM-Launch
 *
 *  Dieses Modul startet den Gast in der VM. Es kann:
 *  1. Den aktuellen Host als Gast "unterklappen" (Host → wird zum Gast)
 *  2. Einen separaten Gast-Kernel starten (z.B. Windows NT)
 *
 *  Modus 1 (Subvert Host):
 *    Der Hypervisor schiebt sich UNTER den laufenden Kernel.
 *    Der Host merkt nichts und läuft normal weiter, aber jeder
 *    CPUID/RDTSC/MSR-Aufruf geht jetzt durch unseren Stealth-Handler.
 *    → Das ist der Modus für "den Hypervisor unsichtbar machen"
 *
 *  Modus 2 (Separate Guest):
 *    Ein komplett separater Gast (z.B. Windows) wird in der VM gestartet.
 *    → Das ist der Modus für "Windows mit GPU-Passthrough"
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
 *  Ein minimaler "Gast", der nur CPUID ausführt und dann via VMCALL
 *  zurückkehrt. Damit testen wir, ob der gesamte VMX-Kreislauf funktioniert.
 * ============================================================================ */

// Dieser Code wird als Gast ausgeführt
// Er ist absichtlich simpel: CPUID → VMCALL → fertig
static void __attribute__((naked, aligned(4096))) guest_test_code(void) {
    __asm__ __volatile__(
        // Test 1: CPUID Leaf 0 (Vendor String)
        "mov $0, %%eax\n"
        "cpuid\n"
        
        // Test 2: CPUID Leaf 1 (Feature Info)
        // Der Stealth-Handler sollte Bit 31 von ECX löschen!
        "mov $1, %%eax\n"
        "cpuid\n"
        // ECX sollte jetzt KEIN Hypervisor-Bit haben
        // Speichere ECX in R12 für spätere Prüfung
        "mov %%ecx, %%r12d\n"
        
        // Test 3: CPUID Leaf 0x40000000 (Hypervisor Vendor)
        // Sollte 0 zurückgeben (kein Hypervisor sichtbar)
        "mov $0x40000000, %%eax\n"
        "cpuid\n"
        // EAX sollte 0 sein
        "mov %%eax, %%r13d\n"
        
        // Test 4: RDTSC (Timing Check)
        "rdtsc\n"
        "mov %%eax, %%r14d\n"  // Low 32 bits des TSC
        
        // Test 5: VMCALL - Hypercall an den Hypervisor
        // RAX = 0xC0DE = "Cosmos Test erfolgreich"
        "mov $0xC0DE, %%eax\n"
        "vmcall\n"
        
        // Sollte nie hierher kommen (VMCALL löst VM-Exit aus)
        "hlt\n"
        
        ::: "memory"
    );
}

// Größe des Guest-Test-Codes (wird zur Laufzeit bestimmt)
static void __attribute__((naked)) guest_test_code_end(void) {
    __asm__ __volatile__("nop\n" ::: "memory");
}

/* ============================================================================
 *  SUBVERT HOST MODE
 *
 *  Schiebt den Hypervisor UNTER den aktuell laufenden Kernel.
 *  Nach dem VMLAUNCH läuft der Kernel als Gast weiter, ohne es zu merken.
 *
 *  Ablauf:
 *  1. Speichere den aktuellen CPU-Zustand (alle Register)
 *  2. Konfiguriere VMCS mit dem aktuellen Zustand als Guest State
 *  3. Setze Guest RIP auf die Rückkehradresse (nach VMLAUNCH)
 *  4. VMLAUNCH → Host wird zum Gast
 *  5. Der Code nach VMLAUNCH läuft jetzt IM GAST weiter
 * ============================================================================ */

// Flag das anzeigt ob wir bereits im Gast laufen
static volatile int subverted = 0;

// Return-Point Funktion für den subvertierten Host
// Hierhin springt der Gast nach erfolgreicher VMLAUNCH
static void __attribute__((noinline)) subvert_return_point(void) {
    subverted = 1;
    LAUNCH_LOG("★★★ HOST ERFOLGREICH SUBVERTIERT! ★★★");
    LAUNCH_LOG("Der Kernel läuft jetzt als VM-Gast.");
    LAUNCH_LOG("Alle CPUID-Aufrufe gehen durch den Stealth-Handler.");
}

int cosmos_hv_subvert_host(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return -1;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    if (!vcpu->vmcs_active) {
        LAUNCH_LOG("CPU %u: VMCS nicht aktiv!", cpu_id);
        return -1;
    }
    
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    LAUNCH_LOG("CPU %u: SUBVERT HOST MODE", cpu_id);
    LAUNCH_LOG("CPU %u: Der Kernel wird zum Gast...", cpu_id);
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    
    uint64_t guest_rsp, guest_rip, guest_rflags;
    
    // Erfasse RFLAGS und RSP
    guest_rflags = cosmos_read_rflags();
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(guest_rsp));
    
    // Setze Guest State
    cosmos_vmwrite(VMCS_GUEST_RFLAGS, guest_rflags | 0x2);
    cosmos_vmwrite(VMCS_GUEST_RSP, guest_rsp);
    cosmos_vmwrite(VMCS_GUEST_CR3, cosmos_read_cr3());
    
    // Guest RIP auf lokales C-Label setzen
    void* resume_ptr = &&guest_resume;
    guest_rip = (uint64_t)resume_ptr;
    cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip);
    
    // Callee-saved Register erfassen, damit der Gast-Context sauber zurückkehren kann
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
    
    LAUNCH_LOG("CPU %u: Starte VMLAUNCH...", cpu_id);
    
    // Wir zwingen GCC, das Label zu behalten und markieren es als
    // erreichbar, damit GCC es nicht ans Ende der Funktion wegoptimiert!
    __asm__ goto("" : : "r"(resume_ptr) : "memory" : guest_resume);
    
    cosmos_vmlaunch_trampoline(&vcpu->guest_regs);
    
    // Wenn wir hier landen, ist VMLAUNCH FEHLGESCHLAGEN
    {
        uint64_t vmx_error = 0;
        cosmos_vmread(0x4400, &vmx_error);
        LAUNCH_LOG("CPU %u: ★ VMLAUNCH FEHLGESCHLAGEN! Error=%llu ★", cpu_id, (unsigned long long)vmx_error);
    }
    return -2;

guest_resume:
    __asm__ __volatile__("" ::: "memory");
    subverted = 1;
    LAUNCH_LOG("★★★ HOST ERFOLGREICH SUBVERTIERT! ★★★");
    LAUNCH_LOG("Der Kernel läuft jetzt als VM-Gast.");
    return 0;
}

/* ============================================================================
 *  LAUNCH SEPARATE GUEST
 *
 *  Startet einen separaten Gast (z.B. den Test-Code oder später Windows).
 *  Der Gast bekommt einen eigenen Adressraum via EPT.
 * ============================================================================ */
int cosmos_hv_launch_test_guest(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return -1;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    if (!vcpu->vmcs_active) {
        LAUNCH_LOG("CPU %u: VMCS nicht aktiv!", cpu_id);
        return -1;
    }
    
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    LAUNCH_LOG("CPU %u: TEST GUEST LAUNCH", cpu_id);
    LAUNCH_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    
    // Guest Code Adresse
    uint64_t guest_entry = (uint64_t)&guest_test_code;
    uint64_t code_size = (uint64_t)&guest_test_code_end - (uint64_t)&guest_test_code;
    
    LAUNCH_LOG("CPU %u: Guest Code @ 0x%llX (%llu bytes)", cpu_id,
              (unsigned long long)guest_entry, (unsigned long long)code_size);
    
    // Guest RIP auf Test-Code setzen
    cosmos_vmwrite(VMCS_GUEST_RIP, guest_entry);
    
    // Guest Stack (Top of a 4KB page, wächst nach unten)
    // Guest Stack allozieren
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
        LAUNCH_LOG("CPU %u: Guest Stack Allokation fehlgeschlagen!", cpu_id);
        return -2;
    }
    cosmos_vmwrite(VMCS_GUEST_RSP, (uint64_t)(guest_stack + 4096 - 8));
    
    // Guest Register auf 0 initialisieren
    memset(&vcpu->guest_regs, 0, sizeof(vmx_guest_regs));
    
    LAUNCH_LOG("CPU %u: Guest Entry: 0x%llX", cpu_id, (unsigned long long)guest_entry);
    LAUNCH_LOG("CPU %u: Guest Stack: 0x%llX", cpu_id, 
              (unsigned long long)(guest_stack + 4096 - 8));
    LAUNCH_LOG("CPU %u: Starte VMLAUNCH für Test-Gast...", cpu_id);
    
    // ★ VMLAUNCH! ★
    cosmos_vmlaunch_trampoline(&vcpu->guest_regs);
    
    // Wenn VMLAUNCH fehlschlägt:
    uint64_t vmx_error = 0;
    cosmos_vmread(0x4400, &vmx_error);
    LAUNCH_LOG("CPU %u: VMLAUNCH fehlgeschlagen! Error=%llu", cpu_id,
              (unsigned long long)vmx_error);
    
    // Error Codes:
    // 1 = VMCALL in VMX root
    // 2 = VMCLEAR mit ungültiger Adresse
    // 3 = VMCLEAR mit VMXON Pointer
    // 4 = VMLAUNCH mit non-clear VMCS
    // 5 = VMRESUME mit non-launched VMCS
    // 7 = VM entry mit ungültigen Control-Feldern
    // 8 = VM entry mit ungültigen Host-State-Feldern
    // 9 = VMPTRLD mit ungültiger Adresse
    // usw.
    
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
 *  Komplette Initialisierung in einem Aufruf:
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
    // Die eigentliche Initialisierung (Detect, Enable, VMCS, EPT)
    // wird jetzt vom LKM (cosmos_lkm.c) gesteuert, um sauberes Thread-Pinning
    // auf den einzelnen Kernen (smp_call_function) zu garantieren.
    
    // Stealth Mode aktivieren
    hv->stealth_mode = 1;
    hv->initialized = 1;
    
    LAUNCH_LOG("╔══════════════════════════════════════════╗");
    LAUNCH_LOG("║  ★ HYPERVISOR BEREIT!                    ║");
    LAUNCH_LOG("║  Stealth Mode: AKTIV                     ║");
    LAUNCH_LOG("║  EPT: %s                             ║", hv->has_ept ? "AKTIV  " : "INAKTIV");
    LAUNCH_LOG("║  VPID: %s                            ║", hv->has_vpid ? "AKTIV  " : "INAKTIV");
    LAUNCH_LOG("╚══════════════════════════════════════════╝");
    
    return 0;
}

/* ============================================================================
 *  HYPERVISOR SHUTDOWN
 * ============================================================================ */
void cosmos_hv_shutdown(cosmos_hypervisor* hv) {
    if (!hv) return;
    
    LAUNCH_LOG("Hypervisor wird heruntergefahren...");
    
    for (uint32_t cpu = 0; cpu < 256; cpu++) {
        if (hv->vcpus[cpu].vmx_enabled) {
            cosmos_hv_disable_vmx(hv, cpu);
        }
    }
    
    hv->initialized = 0;
    hv->stealth_mode = 0;
    
    LAUNCH_LOG("Hypervisor sauber heruntergefahren.");
}

