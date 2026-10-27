/*
 * ============================================================================
 *  COSMOS HYPERVISOR - Implementation
 * ============================================================================
 *
 *  Phase 1.1: VMX Detection & VMXON
 *  Phase 1.3: Stealth VM-Exit Handlers (CPUID, RDTSC, MSR)
 *
 *  Dieses Modul implementiert den Kern des CosmOS Type-1 Hypervisors.
 *  Es kann sowohl bare-metal (CosmOS) als auch unter Linux (zum Testen)
 *  kompiliert werden.
 * ============================================================================
 */

#include "cosmos_hypervisor.h"
#include "cosmos_anti_cheat.h"

/* ============================================================================
 *  MEMORY ALLOCATION ABSTRACTION
 *
 *  Unter Linux: mmap() für page-aligned Speicher
 *  Bare Metal:  Eigener Page Allocator aus CosmOS
 * ============================================================================ */

#ifdef __linux__
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Unter Linux können wir physische Adressen nicht direkt verwenden.
// Für Tests benutzen wir virtuelle = physische Annahme (funktioniert nur als Root mit KVM).
static void* hv_alloc_page(uint64_t* out_phys) {
    void* p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    memset(p, 0, 4096);
    *out_phys = (uint64_t)p;
    return p;
}

static void* hv_alloc_pages(uint64_t size, uint64_t* out_phys) {
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    memset(p, 0, size);
    *out_phys = (uint64_t)p;
    return p;
}

static void hv_free_page(void* ptr) {
    if (ptr) munmap(ptr, 4096);
}

#define HV_LOG(fmt, ...) printf("[COSMOS-HV] " fmt "\n", ##__VA_ARGS__)

#else
// Bare Metal CosmOS - eigener Allocator
extern void* cosmos_alloc_page_aligned(uint64_t* out_phys);
extern void* cosmos_alloc_pages_aligned(uint64_t size, uint64_t* out_phys);
extern void  cosmos_free_page(void* ptr);
extern "C" void* cosmos_get_current_pml4(void);

static void* hv_alloc_page(uint64_t* out_phys) {
    return cosmos_alloc_page_aligned(out_phys);
}

static void* hv_alloc_pages(uint64_t size, uint64_t* out_phys) {
    return cosmos_alloc_pages_aligned(size, out_phys);
}

static void hv_free_page(void* ptr) {
    cosmos_free_page(ptr);
}

// Bare metal printf und memset (bereits in CosmOS kernel vorhanden)
extern "C" int printf(const char* fmt, ...);
extern "C" void* memset(void*, int, unsigned long);
extern "C" void* memcpy(void*, const void*, unsigned long);
#define HV_LOG(fmt, ...) printf("[COSMOS-HV] " fmt "\n", ##__VA_ARGS__)

#endif

/* ============================================================================
 *  HELPER: Adjust VMX control value
 *
 *  Intel VMX erfordert, dass bestimmte Bits in den Control-Feldern
 *  gesetzt oder gelöscht sein MÜSSEN. Die MSRs IA32_VMX_*_CTLS geben
 *  an, welche Bits mandatory 0 und mandatory 1 sind.
 *
 *  allowed0 (Low 32 bits):  Bit=1 → MUSS gesetzt sein
 *  allowed1 (High 32 bits): Bit=0 → MUSS gelöscht sein
 * ============================================================================ */
static uint32_t adjust_vmx_control(uint32_t desired, uint64_t msr_value) {
    uint32_t allowed0 = (uint32_t)(msr_value & 0xFFFFFFFF);       // Must-be-1
    uint32_t allowed1 = (uint32_t)((msr_value >> 32) & 0xFFFFFFFF); // Can-be-1
    
    desired |= allowed0;    // Setze alle mandatory-1 Bits
    desired &= allowed1;    // Lösche alle mandatory-0 Bits
    
    return desired;
}

/* ============================================================================
 *  Phase 1.1: VMX DETECTION
 *
 *  Prüft ob die CPU VMX unterstützt und liest alle Capability-MSRs aus.
 * ============================================================================ */
int cosmos_hv_detect_vmx(cosmos_hypervisor* hv) {
    if (!hv) return -1;
    
    // WICHTIG: Nur die Capabilities und VCPUs nullen, NICHT stealth_mode/initialized
    // (die wurden bereits von cosmos_hv_init gesetzt)
    uint8_t saved_stealth = hv->stealth_mode;
    uint8_t saved_init = hv->initialized;
    memset(hv, 0, sizeof(cosmos_hypervisor));
    hv->stealth_mode = saved_stealth;
    hv->initialized = saved_init;
    
    HV_LOG("========================================");
    HV_LOG("  CosmOS Stealth Hypervisor v0.1");
    HV_LOG("  Phase 1.1: VMX Detection");
    HV_LOG("========================================");
    
    // --- Schritt 1: CPUID Leaf 1 prüfen ---
    uint32_t eax, ebx, ecx, edx;
    cosmos_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    if (!(ecx & CPUID_1_ECX_VMX)) {
        HV_LOG("FEHLER: CPU unterstützt KEIN VMX (VT-x)!");
        HV_LOG("Bitte VT-x im BIOS aktivieren.");
        return -1;
    }
    
    hv->has_vmx = 1;
    HV_LOG("[OK] VMX (VT-x) wird unterstützt");
    
    // Warnung falls wir bereits in einem Hypervisor laufen
    if (ecx & CPUID_1_ECX_HYPERVISOR) {
        HV_LOG("[WARN] Ein Hypervisor ist bereits aktiv!");
        HV_LOG("[WARN] Nested Virtualization erforderlich.");
    }
    
    // --- Schritt 2: IA32_FEATURE_CONTROL MSR prüfen ---
    uint64_t feature_control = cosmos_rdmsr(IA32_FEATURE_CONTROL);
    HV_LOG("IA32_FEATURE_CONTROL = 0x%llX", (unsigned long long)feature_control);
    
    if (feature_control & FEATURE_CONTROL_LOCKED) {
        if (!(feature_control & FEATURE_CONTROL_VMXON_OUTSIDE)) {
            HV_LOG("FEHLER: VMXON ist im BIOS gesperrt!");
            HV_LOG("Bitte 'Intel Virtualization Technology' im BIOS aktivieren.");
            return -2;
        }
        HV_LOG("[OK] VMXON ist erlaubt (MSR locked, VMXON enabled)");
    } else {
        // MSR ist nicht gesperrt - wir können es selbst konfigurieren
        HV_LOG("[INFO] IA32_FEATURE_CONTROL ist unlocked, konfiguriere...");
        feature_control |= FEATURE_CONTROL_VMXON_OUTSIDE | FEATURE_CONTROL_LOCKED;
        cosmos_wrmsr(IA32_FEATURE_CONTROL, feature_control);
        HV_LOG("[OK] VMXON aktiviert und MSR gesperrt");
    }
    
    // --- Schritt 3: VMX Capability MSRs auslesen ---
    uint64_t vmx_basic = cosmos_rdmsr(IA32_VMX_BASIC);
    
    hv->vmcs_revision_id = (uint32_t)(vmx_basic & 0x7FFFFFFF);
    hv->vmcs_region_size = (uint32_t)((vmx_basic >> 32) & 0x1FFF);
    hv->has_true_msrs    = (vmx_basic >> 55) & 1;
    
    if (hv->vmcs_region_size == 0) hv->vmcs_region_size = 4096;
    
    HV_LOG("VMCS Revision ID:  0x%X", hv->vmcs_revision_id);
    HV_LOG("VMCS Region Size:  %u bytes", hv->vmcs_region_size);
    HV_LOG("True MSR Controls: %s", hv->has_true_msrs ? "JA" : "NEIN");
    
    // --- Schritt 4: Control-Capability MSRs lesen ---
    // Benutze "True" MSRs wenn verfügbar (genauere Kontrolle)
    if (hv->has_true_msrs) {
        hv->vmx_pin_based_allowed0  = cosmos_rdmsr(IA32_VMX_TRUE_PINBASED_CTLS);
        hv->vmx_proc_based_allowed0 = cosmos_rdmsr(IA32_VMX_TRUE_PROCBASED_CTLS);
        hv->vmx_exit_allowed0       = cosmos_rdmsr(IA32_VMX_TRUE_EXIT_CTLS);
        hv->vmx_entry_allowed0      = cosmos_rdmsr(IA32_VMX_TRUE_ENTRY_CTLS);
    } else {
        hv->vmx_pin_based_allowed0  = cosmos_rdmsr(IA32_VMX_PINBASED_CTLS);
        hv->vmx_proc_based_allowed0 = cosmos_rdmsr(IA32_VMX_PROCBASED_CTLS);
        hv->vmx_exit_allowed0       = cosmos_rdmsr(IA32_VMX_EXIT_CTLS);
        hv->vmx_entry_allowed0      = cosmos_rdmsr(IA32_VMX_ENTRY_CTLS);
    }
    
    // Secondary Proc-Based Controls (immer aus dem gleichen MSR)
    hv->vmx_proc_based2_allowed0 = cosmos_rdmsr(IA32_VMX_PROCBASED_CTLS2);
    
    // --- Schritt 5: EPT/VPID Capabilities ---
    hv->ept_vpid_cap = cosmos_rdmsr(IA32_VMX_EPT_VPID_CAP);
    
    uint64_t sec2 = hv->vmx_proc_based2_allowed0;
    uint32_t sec2_allowed1 = (uint32_t)((sec2 >> 32) & 0xFFFFFFFF);
    
    hv->has_ept                = (sec2_allowed1 >> 1) & 1;
    hv->has_vpid               = (sec2_allowed1 >> 5) & 1;
    hv->has_unrestricted_guest = (sec2_allowed1 >> 7) & 1;
    
    // Preemption Timer (für TSC-Stealth)
    uint64_t pin_cap = hv->vmx_pin_based_allowed0;
    uint32_t pin_allowed1 = (uint32_t)((pin_cap >> 32) & 0xFFFFFFFF);
    hv->has_preempt_timer = (pin_allowed1 >> 6) & 1;
    
    HV_LOG("--- VMX Feature Report ---");
    HV_LOG("  EPT:                %s", hv->has_ept ? "JA ✓" : "NEIN ✗");
    HV_LOG("  VPID:               %s", hv->has_vpid ? "JA ✓" : "NEIN ✗");
    HV_LOG("  Unrestricted Guest: %s", hv->has_unrestricted_guest ? "JA ✓" : "NEIN ✗");
    HV_LOG("  Preemption Timer:   %s", hv->has_preempt_timer ? "JA ✓" : "NEIN ✗");
    
    if (!hv->has_ept) {
        HV_LOG("WARNUNG: Ohne EPT ist Stealth-Virtualisierung nicht möglich!");
    }
    
    HV_LOG("========================================");
    HV_LOG("  VMX Detection: ERFOLGREICH");
    HV_LOG("========================================");
    
    return 0;
}

/* ============================================================================
 *  Phase 1.1: VMXON - VMX Root Operation aktivieren
 *
 *  Aktiviert VMX auf einer bestimmten CPU.
 *  Muss auf JEDER CPU einzeln aufgerufen werden (per IPI oder SMP init).
 * ============================================================================ */
uint32_t g_active_cpu_id = 0;

int cosmos_hv_enable_vmx(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || !hv->has_vmx || cpu_id >= 256) return -1;
    
    g_active_cpu_id = cpu_id;
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    vcpu->cpu_id = cpu_id;
    
    HV_LOG("CPU %u: VMX aktivieren...", cpu_id);
    
    // --- Schritt 1: CR4.VMXE setzen ---
    uint64_t cr4 = cosmos_read_cr4();
    if (!(cr4 & CR4_VMXE)) {
        cr4 |= CR4_VMXE;
        cosmos_write_cr4(cr4);
        HV_LOG("CPU %u: CR4.VMXE gesetzt", cpu_id);
    }
    
    // --- Schritt 2: CR0 an VMX-Anforderungen anpassen ---
    // Bestimmte CR0-Bits MÜSSEN gesetzt/gelöscht sein für VMX
    uint64_t cr0 = cosmos_read_cr0();
    uint64_t cr0_fixed0 = cosmos_rdmsr(IA32_VMX_CR0_FIXED0);
    uint64_t cr0_fixed1 = cosmos_rdmsr(IA32_VMX_CR0_FIXED1);
    cr0 |= cr0_fixed0;   // Must-be-1 Bits setzen
    cr0 &= cr0_fixed1;   // Must-be-0 Bits löschen
    cosmos_write_cr0(cr0);
    
    // CR4 ebenfalls an VMX-Anforderungen anpassen
    cr4 = cosmos_read_cr4();
    uint64_t cr4_fixed0 = cosmos_rdmsr(IA32_VMX_CR4_FIXED0);
    uint64_t cr4_fixed1 = cosmos_rdmsr(IA32_VMX_CR4_FIXED1);
    cr4 |= cr4_fixed0;
    cr4 &= cr4_fixed1;
    cosmos_write_cr4(cr4);
    
    // --- Schritt 3: VMXON Region allozieren ---
    vcpu->vmxon_region = (vmx_region*)hv_alloc_page(&vcpu->vmxon_phys);
    if (!vcpu->vmxon_region) {
        HV_LOG("CPU %u: FEHLER: Konnte VMXON Region nicht allozieren!", cpu_id);
        return -2;
    }
    
    // Revision ID in die VMXON Region schreiben (PFLICHT!)
    vcpu->vmxon_region->revision_id = hv->vmcs_revision_id;
    
    HV_LOG("CPU %u: VMXON Region @ virt=0x%llX phys=0x%llX",
           cpu_id,
           (unsigned long long)vcpu->vmxon_region,
           (unsigned long long)vcpu->vmxon_phys);
    
    // --- Schritt 4: VMXON ausführen! ---
    uint8_t result = cosmos_vmxon(&vcpu->vmxon_phys);
    if (result != 0) {
        HV_LOG("CPU %u: FEHLER: VMXON fehlgeschlagen! (result=%u)", cpu_id, result);
        hv_free_page(vcpu->vmxon_region);
        vcpu->vmxon_region = nullptr;
        return -3;
    }
    
    vcpu->vmx_enabled = 1;
    HV_LOG("CPU %u: ★ VMXON ERFOLGREICH! VMX Root Operation aktiv! ★", cpu_id);
    
    // --- Schritt 5: VMCS Region allozieren und initialisieren ---
    vcpu->vmcs_region = (vmx_region*)hv_alloc_page(&vcpu->vmcs_phys);
    if (!vcpu->vmcs_region) {
        HV_LOG("CPU %u: FEHLER: Konnte VMCS Region nicht allozieren!", cpu_id);
        cosmos_vmxoff();
        hv_free_page(vcpu->vmxon_region);
        return -4;
    }
    
    vcpu->vmcs_region->revision_id = hv->vmcs_revision_id;
    
    HV_LOG("CPU %u: VMCS Region @ virt=0x%llX phys=0x%llX",
           cpu_id,
           (unsigned long long)vcpu->vmcs_region,
           (unsigned long long)vcpu->vmcs_phys);
    
    // VMCLEAR: VMCS in den "clear" Zustand versetzen
    if (cosmos_vmclear(&vcpu->vmcs_phys) != 0) {
        HV_LOG("CPU %u: FEHLER: VMCLEAR fehlgeschlagen!", cpu_id);
        return -5;
    }
    
    // VMPTRLD: VMCS als aktives VMCS laden
    if (cosmos_vmptrld(&vcpu->vmcs_phys) != 0) {
        HV_LOG("CPU %u: FEHLER: VMPTRLD fehlgeschlagen!", cpu_id);
        return -6;
    }
    
    vcpu->vmcs_active = 1;
    HV_LOG("CPU %u: VMCS geladen und aktiv", cpu_id);
    
    // --- Schritt 6: MSR Bitmap allozieren ---
    vcpu->msr_bitmap = (uint8_t*)hv_alloc_page(&vcpu->msr_bitmap_phys);
    if (!vcpu->msr_bitmap) {
        HV_LOG("CPU %u: WARNUNG: MSR Bitmap konnte nicht alloziert werden", cpu_id);
    } else {
        // Alle MSRs durchlassen (Bitmap = 0 → kein VM-Exit)
        // Wir setzen gezielt nur die MSRs, die wir abfangen wollen
        memset(vcpu->msr_bitmap, 0, 4096);
        // Setze VMX MSRs (0x480 - 0x48F) auf intercept (Read & Write)
        for (uint32_t msr = 0x480; msr <= 0x48F; msr++) {
            vcpu->msr_bitmap[msr / 8] |= (1 << (msr % 8));              // Read intercept
            vcpu->msr_bitmap[2048 + (msr / 8)] |= (1 << (msr % 8));     // Write intercept
        }
        HV_LOG("CPU %u: MSR Bitmap initialisiert (VMX-Intercepts aktiv)", cpu_id);
    }
    
    // --- Schritt 7: Host Stack allozieren (16KB für sichere VM-Exit Verarbeitung) ---
    // 4KB reicht NICHT — printk/vprintk brauchen tiefe Callchains die >4KB Stack nutzen
    #define HOST_STACK_SIZE (16 * 1024)  // 16KB = 4 Pages
    uint64_t host_stack_phys;
    vcpu->host_stack = (uint8_t*)hv_alloc_pages(HOST_STACK_SIZE, &host_stack_phys);
    vcpu->host_stack_phys = host_stack_phys;
    
    if (!vcpu->host_stack) {
        HV_LOG("CPU %u: FEHLER: Host Stack konnte nicht alloziert werden!", cpu_id);
        return -7;
    }
    
    // TSC-Offset für Stealth initialisieren
    vcpu->tsc_offset = 0;
    vcpu->last_exit_tsc = 0;
    
    HV_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    HV_LOG("CPU %u: ★ VMX VOLLSTÄNDIG INITIALISIERT ★", cpu_id);
    HV_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    
    return 0;
}

/* ============================================================================
 *  Phase 1.1: VMXOFF - VMX deaktivieren
 * ============================================================================ */
void cosmos_hv_disable_vmx(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    
    if (!vcpu->vmx_enabled) {
        HV_LOG("CPU %u: VMX war nicht aktiv", cpu_id);
        return;
    }
    
    HV_LOG("CPU %u: VMX deaktivieren...", cpu_id);
    
    // VMCS freigeben
    if (vcpu->vmcs_active) {
        cosmos_vmclear(&vcpu->vmcs_phys);
        vcpu->vmcs_active = 0;
    }
    
    // VMXOFF
    cosmos_vmxoff();
    vcpu->vmx_enabled = 0;
    
    // CR4.VMXE löschen
    uint64_t cr4 = cosmos_read_cr4();
    cr4 &= ~CR4_VMXE;
    cosmos_write_cr4(cr4);
    
    // Speicher freigeben
    if (vcpu->vmxon_region) { hv_free_page(vcpu->vmxon_region); vcpu->vmxon_region = nullptr; }
    if (vcpu->vmcs_region)  { hv_free_page(vcpu->vmcs_region);  vcpu->vmcs_region = nullptr;  }
    if (vcpu->msr_bitmap)   { hv_free_page(vcpu->msr_bitmap);   vcpu->msr_bitmap = nullptr;   }
    if (vcpu->host_stack)   { hv_free_page(vcpu->host_stack);   vcpu->host_stack = nullptr;   }
    if (vcpu->host_cr3_region) { hv_free_page(vcpu->host_cr3_region); vcpu->host_cr3_region = nullptr; }
    
    HV_LOG("CPU %u: VMX deaktiviert und Ressourcen freigegeben", cpu_id);
}

/* ============================================================================
 *  Phase 1.2: VMCS SETUP
 *
 *  Konfiguriert alle VMCS-Felder für den Gast.
 *  Dies setzt die VM-Execution Controls, Guest State und Host State.
 * ============================================================================ */
static uint64_t cosmos_get_segment_base(uint16_t selector, uint64_t gdt_base) {
    if (selector == 0) return 0;
    uint32_t index = selector & ~7;
    uint64_t desc = *(uint64_t*)(gdt_base + index);
    
    // Base address extraction (Intel standard 32-bit base)
    uint64_t base = ((desc >> 16) & 0xFFFFFF) | ((desc >> 32) & 0xFF000000);
    
    // Check if it's a system segment (S-bit = 0). In 64-bit mode, TSS and LDT are 16 bytes.
    if ((desc & (1ULL << 44)) == 0) {
        uint64_t desc_high = *(uint64_t*)(gdt_base + index + 8);
        base |= (desc_high << 32);
    }
    
    return base;
}

static uint32_t cosmos_get_segment_limit(uint16_t selector, uint64_t gdt_base) {
    if (selector == 0) return 0;
    uint32_t index = selector & ~7;
    uint64_t desc = *(uint64_t*)(gdt_base + index);
    uint32_t limit = (desc & 0xFFFF) | ((desc >> 32) & 0xF0000);
    
    // VMX expects the actual unscaled limit. If G=1, scale by 4KB.
    if (desc & (1ULL << 55)) { 
        limit = (limit << 12) | 0xFFF;
    }
    return limit;
}

static uint32_t cosmos_get_segment_access_rights(uint16_t selector, uint64_t gdt_base) {
    if (selector == 0) return 0x10000; // Unusable
    uint32_t index = selector & ~7;
    uint64_t desc = *(uint64_t*)(gdt_base + index);
    
    uint32_t ar = (desc >> 40) & 0xFF; // Type, S, DPL, P
    ar |= ((desc >> 52) & 0xF) << 12;  // AVL, L, D/B, G
    
    // If not present (P=0), mark as unusable
    if ((ar & 0x80) == 0) {
        ar |= 0x10000;
    }
    return ar;
}

int cosmos_hv_setup_vmcs(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return -1;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    if (!vcpu->vmcs_active) {
        HV_LOG("CPU %u: VMCS ist nicht aktiv!", cpu_id);
        return -1;
    }
    
    HV_LOG("CPU %u: VMCS konfigurieren...", cpu_id);
    
    // ===== VM-EXECUTION CONTROLS =====
    
    // Pin-Based Controls (minimal)
    uint32_t pin_based = 0;
    pin_based = adjust_vmx_control(pin_based, hv->vmx_pin_based_allowed0);
    cosmos_vmwrite(VMCS_PIN_BASED_EXEC_CTRL, pin_based);
    
    // Primary Proc-Based Controls
    uint32_t proc_based = 0;
    proc_based |= PROCBASED_USE_MSR_BITMAPS;       // MSR Bitmap benutzen
    proc_based |= PROCBASED_ACTIVATE_SECONDARY;     // Secondary Controls aktivieren
    proc_based |= PROCBASED_USE_TSC_OFFSET;         // TSC-Offset für Stealth
    // KEIN PROCBASED_RDTSC_EXIT → TSC-Offset reicht für Stealth
    // CPUID löst IMMER einen VM-Exit aus (kein Control-Bit nötig)
    proc_based = adjust_vmx_control(proc_based, hv->vmx_proc_based_allowed0);
    cosmos_vmwrite(VMCS_PROC_BASED_EXEC_CTRL, proc_based);
    
    // Secondary Proc-Based Controls
    uint32_t proc_based2 = 0;
    if (hv->has_ept)                proc_based2 |= PROCBASED2_ENABLE_EPT;
    if (hv->has_vpid)               proc_based2 |= PROCBASED2_ENABLE_VPID;
    if (hv->has_unrestricted_guest) proc_based2 |= PROCBASED2_UNRESTRICTED_GUEST;
    proc_based2 |= PROCBASED2_ENABLE_RDTSCP;       // RDTSCP erlauben
    proc_based2 |= PROCBASED2_ENABLE_INVPCID;      // INVPCID erlauben
    proc_based2 |= PROCBASED2_ENABLE_XSAVES;       // XSAVES/XRSTORS erlauben
    proc_based2 = adjust_vmx_control(proc_based2, hv->vmx_proc_based2_allowed0);
    cosmos_vmwrite(VMCS_PROC_BASED_EXEC_CTRL2, proc_based2);
    
    // VM-Exit Controls
    uint32_t exit_controls = 0;
    exit_controls |= VMEXIT_CTRL_HOST_ADDR_SPACE;   // Host ist 64-bit!
    exit_controls |= VMEXIT_CTRL_SAVE_EFER;
    exit_controls |= VMEXIT_CTRL_LOAD_EFER;
    exit_controls |= VMEXIT_CTRL_ACK_INT_ON_EXIT;
    exit_controls = adjust_vmx_control(exit_controls, hv->vmx_exit_allowed0);
    cosmos_vmwrite(VMCS_EXIT_CONTROLS, exit_controls);
    
    // VM-Entry Controls
    uint32_t entry_controls = 0;
    entry_controls |= VMENTRY_CTRL_GUEST_IA32E;     // Gast ist 64-bit!
    entry_controls |= VMENTRY_CTRL_LOAD_EFER;
    entry_controls = adjust_vmx_control(entry_controls, hv->vmx_entry_allowed0);
    cosmos_vmwrite(VMCS_ENTRY_CONTROLS, entry_controls);
    
    // Exception Bitmap: Keine Exceptions abfangen (alles durchlassen)
    cosmos_vmwrite(VMCS_EXCEPTION_BITMAP, 0);
    
    // MSR Bitmap setzen
    if (vcpu->msr_bitmap) {
        cosmos_vmwrite(VMCS_MSR_BITMAP, vcpu->msr_bitmap_phys);
    }
    
    // TSC Offset (Stealth: kompensiert VM-Exit Overhead)
    cosmos_vmwrite(VMCS_TSC_OFFSET, (uint64_t)vcpu->tsc_offset);
    
    // VPID (wenn verfügbar)
    if (hv->has_vpid) {
        cosmos_vmwrite(VMCS_VPID, cpu_id + 1);  // VPID 0 ist reserviert
    }
    
    // VMCS Link Pointer (muss 0xFFFFFFFF_FFFFFFFF sein wenn kein Shadow VMCS)
    cosmos_vmwrite(VMCS_GUEST_VMCS_LINK_PTR, 0xFFFFFFFFFFFFFFFFULL);
    
    // EPT Pointer (EPTP) setzen
    if (hv->has_ept) {
        // EPTP = PML4 | (Walk Length 4 - 1)<<3 | Write-Back (6)
        uint64_t eptp = hv->ept_pml4_phys | (3ULL << 3) | 6ULL;
        cosmos_vmwrite(VMCS_EPTP, eptp);
    }
    
    // ===== HOST STATE =====
    // Der Host-State definiert, wohin die CPU bei einem VM-Exit springt.
    
    // Host Segment Selectors
    cosmos_vmwrite(VMCS_HOST_CS_SEL, cosmos_read_cs());
    cosmos_vmwrite(VMCS_HOST_SS_SEL, cosmos_read_ss());
    cosmos_vmwrite(VMCS_HOST_DS_SEL, cosmos_read_ds());
    cosmos_vmwrite(VMCS_HOST_ES_SEL, cosmos_read_es());
    cosmos_vmwrite(VMCS_HOST_FS_SEL, cosmos_read_fs());
    cosmos_vmwrite(VMCS_HOST_GS_SEL, cosmos_read_gs());
    cosmos_vmwrite(VMCS_HOST_TR_SEL, cosmos_read_tr());
    
    // --- Robustes Host CR3 ---
    // Wenn wir `cosmos_read_cr3()` verwenden, nehmen wir das CR3 des Prozesses, der 
    // zufällig gerade auf der CPU läuft (z.B. insmod). Wenn der Prozess sich beendet,
    // wird sein CR3 gelöscht, und der nächste VM-Exit löst einen Triple-Fault aus!
    // Lösung: Wir kopieren die oberen 256 Einträge (Kernel-Space) des aktuellen CR3 in
    // ein NEUES Page-Dir, das für immer im Speicher bleibt.
    vcpu->host_cr3_region = cosmos_alloc_page_aligned(&vcpu->host_cr3_phys);
    memset(vcpu->host_cr3_region, 0, 4096);
    // Kernel-Space (obere Hälfte der PML4, also 2048 Bytes ab Offset 2048) kopieren
    memcpy((uint8_t*)vcpu->host_cr3_region + 2048, (uint8_t*)cosmos_get_current_pml4() + 2048, 2048);
    
    // Host Control Registers
    cosmos_vmwrite(VMCS_HOST_CR0, cosmos_read_cr0());
    cosmos_vmwrite(VMCS_HOST_CR3, vcpu->host_cr3_phys); // Nutze unser sicheres CR3
    cosmos_vmwrite(VMCS_HOST_CR4, cosmos_read_cr4());
    
    // Stealth: Verstecke VMXE vor dem Gast
    // Der Gast liest CR4.VMXE als 0. Schreibversuche auf VMXE verursachen einen VM-Exit.
    cosmos_vmwrite(VMCS_CR4_GUEST_HOST_MASK, CR4_VMXE);
    cosmos_vmwrite(VMCS_CR4_READ_SHADOW, cosmos_read_cr4() & ~CR4_VMXE);
    
    // Host GDT/IDT
    desc_table_reg gdtr, idtr;
    cosmos_sgdt(&gdtr);
    cosmos_sidt(&idtr);
    cosmos_vmwrite(VMCS_HOST_GDTR_BASE, gdtr.base);
    cosmos_vmwrite(VMCS_HOST_IDTR_BASE, idtr.base);
    
    // Host TR Base aus der GDT berechnen
    uint64_t host_tr_base = cosmos_get_segment_base(cosmos_read_tr(), gdtr.base);
    cosmos_vmwrite(VMCS_HOST_TR_BASE, host_tr_base);
    
    // Host MSRs
    cosmos_vmwrite(VMCS_HOST_EFER, cosmos_rdmsr(IA32_EFER));
    
    // Host FS/GS Base
    cosmos_vmwrite(VMCS_HOST_FS_BASE, cosmos_rdmsr(IA32_FS_BASE));
    cosmos_vmwrite(VMCS_HOST_GS_BASE, cosmos_rdmsr(IA32_GS_BASE));
    
    // Host SYSENTER (für Kompatibilität)
    cosmos_vmwrite(VMCS_HOST_SYSENTER_ESP, cosmos_rdmsr(IA32_SYSENTER_ESP));
    cosmos_vmwrite(VMCS_HOST_SYSENTER_EIP, cosmos_rdmsr(IA32_SYSENTER_EIP));
    
    // Host RSP und RIP (VM-Exit Entry Point!)
    // RSP: Top of host stack (Stack wächst nach unten!)
    // 16-byte alignment ist wichtig für System V ABI
    cosmos_vmwrite(VMCS_HOST_RSP, (uint64_t)(vcpu->host_stack + HOST_STACK_SIZE));
    
    // RIP: cosmos_vmexit_stub (definiert in cosmos_vmx.S)
    cosmos_vmwrite(VMCS_HOST_RIP, (uint64_t)&cosmos_vmexit_stub);
    
    HV_LOG("CPU %u: Host RIP = 0x%llX (vmexit_stub)", cpu_id,
           (unsigned long long)&cosmos_vmexit_stub);
    HV_LOG("CPU %u: Host RSP = 0x%llX (stack size: %d bytes)", cpu_id,
           (unsigned long long)(vcpu->host_stack + HOST_STACK_SIZE), HOST_STACK_SIZE);
    
    // ===== GUEST STATE =====
    // Initialer Gast-Zustand (wird später für Windows angepasst)
    
    uint16_t cs_sel = cosmos_read_cs();
    uint16_t ss_sel = cosmos_read_ss();
    uint16_t ds_sel = cosmos_read_ds();
    uint16_t es_sel = cosmos_read_es();
    uint16_t fs_sel = cosmos_read_fs();
    uint16_t gs_sel = cosmos_read_gs();
    uint16_t tr_sel = cosmos_read_tr();
    uint16_t ldtr_sel = cosmos_read_ldtr();

    // Guest Segment Selectors
    cosmos_vmwrite(VMCS_GUEST_CS_SEL,    cs_sel);
    cosmos_vmwrite(VMCS_GUEST_SS_SEL,    ss_sel);
    cosmos_vmwrite(VMCS_GUEST_DS_SEL,    ds_sel);
    cosmos_vmwrite(VMCS_GUEST_ES_SEL,    es_sel);
    cosmos_vmwrite(VMCS_GUEST_FS_SEL,    fs_sel);
    cosmos_vmwrite(VMCS_GUEST_GS_SEL,    gs_sel);
    cosmos_vmwrite(VMCS_GUEST_TR_SEL,    tr_sel);
    cosmos_vmwrite(VMCS_GUEST_LDTR_SEL,  ldtr_sel);
    
    // Guest Segment Limits
    cosmos_vmwrite(VMCS_GUEST_CS_LIMIT,  cosmos_get_segment_limit(cs_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_SS_LIMIT,  cosmos_get_segment_limit(ss_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_DS_LIMIT,  cosmos_get_segment_limit(ds_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_ES_LIMIT,  cosmos_get_segment_limit(es_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_FS_LIMIT,  cosmos_get_segment_limit(fs_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_GS_LIMIT,  cosmos_get_segment_limit(gs_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_TR_LIMIT,  cosmos_get_segment_limit(tr_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_LDTR_LIMIT, cosmos_get_segment_limit(ldtr_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_GDTR_LIMIT, gdtr.limit);
    cosmos_vmwrite(VMCS_GUEST_IDTR_LIMIT, idtr.limit);
    
    // Guest Segment Access Rights
    cosmos_vmwrite(VMCS_GUEST_CS_ACCESS, cosmos_get_segment_access_rights(cs_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_SS_ACCESS, cosmos_get_segment_access_rights(ss_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_DS_ACCESS, cosmos_get_segment_access_rights(ds_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_ES_ACCESS, cosmos_get_segment_access_rights(es_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_FS_ACCESS, cosmos_get_segment_access_rights(fs_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_GS_ACCESS, cosmos_get_segment_access_rights(gs_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_TR_ACCESS, cosmos_get_segment_access_rights(tr_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_LDTR_ACCESS, cosmos_get_segment_access_rights(ldtr_sel, gdtr.base));
    
    // Guest Segment Bases
    cosmos_vmwrite(VMCS_GUEST_CS_BASE, cosmos_get_segment_base(cs_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_SS_BASE, cosmos_get_segment_base(ss_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_DS_BASE, cosmos_get_segment_base(ds_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_ES_BASE, cosmos_get_segment_base(es_sel, gdtr.base));
    cosmos_vmwrite(VMCS_GUEST_FS_BASE, cosmos_rdmsr(IA32_FS_BASE));
    cosmos_vmwrite(VMCS_GUEST_GS_BASE, cosmos_rdmsr(IA32_GS_BASE));
    cosmos_vmwrite(VMCS_GUEST_GDTR_BASE, gdtr.base);
    cosmos_vmwrite(VMCS_GUEST_IDTR_BASE, idtr.base);
    
    // Guest TR und LDTR Base (Host Werte übernehmen)
    cosmos_vmwrite(VMCS_GUEST_TR_BASE, host_tr_base);
    cosmos_vmwrite(VMCS_GUEST_LDTR_BASE, 0);
    
    // Guest Control Registers
    cosmos_vmwrite(VMCS_GUEST_CR0, cosmos_read_cr0());
    cosmos_vmwrite(VMCS_GUEST_CR3, cosmos_read_cr3());
    cosmos_vmwrite(VMCS_GUEST_CR4, cosmos_read_cr4());
    
    // Guest DR7 (Debug Registers)
    cosmos_vmwrite(VMCS_GUEST_DR7, 0x400);  // Default value
    
    // Guest RFLAGS
    cosmos_vmwrite(VMCS_GUEST_RFLAGS, cosmos_read_rflags() | 0x2);  // Bit 1 muss immer 1 sein
    
    // Guest MSRs
    cosmos_vmwrite(VMCS_GUEST_EFER, cosmos_rdmsr(IA32_EFER));
    cosmos_vmwrite(VMCS_GUEST_SYSENTER_CS, cosmos_rdmsr(IA32_SYSENTER_CS));
    cosmos_vmwrite(VMCS_GUEST_SYSENTER_ESP, cosmos_rdmsr(IA32_SYSENTER_ESP));
    cosmos_vmwrite(VMCS_GUEST_SYSENTER_EIP, cosmos_rdmsr(IA32_SYSENTER_EIP));
    cosmos_vmwrite(VMCS_GUEST_DEBUGCTL, cosmos_rdmsr(IA32_DEBUGCTL));
    
    // Guest Activity State (0 = Active)
    cosmos_vmwrite(VMCS_GUEST_ACTIVITY_STATE, 0);
    
    // Guest Interruptibility State (no blocking)
    cosmos_vmwrite(VMCS_GUEST_INT_STATE, 0);
    
    // Guest RIP und RSP werden SPÄTER gesetzt, wenn der Gast gestartet wird
    // cosmos_vmwrite(VMCS_GUEST_RIP, guest_entry_point);
    // cosmos_vmwrite(VMCS_GUEST_RSP, guest_stack_pointer);
    
    // Anti-Cheat Hardening (Stealth Mode) konfigurieren
    if (hv->stealth_mode) {
        cosmos_ac_configure_vmcs_stealth(hv, vcpu);
    }
    
    HV_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    HV_LOG("CPU %u: ★ VMCS VOLLSTÄNDIG KONFIGURIERT ★", cpu_id);
    HV_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    
    return 0;
}

/* ============================================================================
 *  Phase 1.3: STEALTH VM-EXIT HANDLERS
 *
 *  Diese Handler machen den Hypervisor unsichtbar für Anti-Cheat-Software.
 * ============================================================================ */

/* --- CPUID Stealth Handler ---
 * 
 * Anti-Cheat prüft:
 *   CPUID.1:ECX[31] = Hypervisor Present Bit
 *   CPUID.0x40000000 = Hypervisor Vendor String
 *   CPUID.0x40000001+ = Hypervisor Features
 *
 * Wir fälschen ALLE diese Antworten!
 */
void cosmos_hv_handle_cpuid(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint32_t leaf    = (uint32_t)vcpu->guest_regs.rax;
    uint32_t subleaf = (uint32_t)vcpu->guest_regs.rcx;
    
    uint32_t eax, ebx, ecx, edx;
    
    // Echtes CPUID ausführen (wir sind im Host/Root Mode)
    cosmos_cpuid(leaf, subleaf, &eax, &ebx, &ecx, &edx);
    
    switch (leaf) {
        case 0x01:
            // ★ STEALTH: Hypervisor Present Bit löschen! ★
            ecx &= ~CPUID_1_ECX_HYPERVISOR;
            
            // Optional: VMX-Bit auch löschen (damit der Gast nicht merkt,
            // dass VMX verfügbar ist und versucht, nested zu virtualisieren)
            if (hv->stealth_mode) {
                ecx &= ~CPUID_1_ECX_VMX;
            }
            break;
            
        case 0x40000000:
            // ★ STEALTH: Keine Hypervisor-Signatur zurückgeben! ★
            // Normalerweise gibt ein Hypervisor hier seinen Namen zurück
            // (z.B. "KVMKVMKVM\0\0\0" oder "Microsoft Hv").
            // Wir geben 0 zurück = "kein Hypervisor vorhanden".
            eax = 0;
            ebx = 0;
            ecx = 0;
            edx = 0;
            break;
            
        case 0x40000001:
        case 0x40000002:
        case 0x40000003:
        case 0x40000004:
        case 0x40000005:
        case 0x40000006:
            // ★ STEALTH: Alle Hypervisor-Info-Leaves auf 0 setzen ★
            eax = 0;
            ebx = 0;
            ecx = 0;
            edx = 0;
            break;
            
        default:
            // Alle anderen CPUID-Leaves: echte Werte durchreichen
            break;
    }
    
    // Ergebnisse in die Gast-Register schreiben
    vcpu->guest_regs.rax = eax;
    vcpu->guest_regs.rbx = ebx;
    vcpu->guest_regs.rcx = ecx;
    vcpu->guest_regs.rdx = edx;
}

/* --- RDTSC Stealth Handler ---
 *
 * Anti-Cheat misst die Zeit, die bestimmte Instruktionen brauchen.
 * Ein VM-Exit kostet ~1000-3000 Taktzyklen extra.
 * Wenn wir den TSC nicht kompensieren, erkennt das Anti-Cheat
 * die "Zeitlücke" und weiß, dass ein Hypervisor läuft.
 *
 * Lösung: TSC-Offset anpassen, um die VM-Exit-Kosten zu verstecken.
 */
void cosmos_hv_handle_rdtsc(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint64_t real_tsc = cosmos_rdtsc();
    
    // Kompensiere den TSC um den VM-Exit-Overhead
    // Der Offset wird dynamisch angepasst basierend auf der gemessenen
    // VM-Exit-Latenz
    real_tsc += vcpu->tsc_offset;
    
    vcpu->guest_regs.rax = (uint32_t)(real_tsc & 0xFFFFFFFF);
    vcpu->guest_regs.rdx = (uint32_t)(real_tsc >> 32);
}

/* --- RDMSR Stealth Handler ---
 *
 * Anti-Cheat liest bestimmte MSRs, um nach Hypervisorn zu suchen:
 * - IA32_VMX_BASIC: Existiert nur wenn VMX aktiv ist
 * - IA32_DEBUGCTL: Kann Breakpoints verraten
 * - IA32_TSC_AUX: Prozessor-ID
 */
void cosmos_hv_handle_rdmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint32_t msr = (uint32_t)vcpu->guest_regs.rcx;
    uint64_t value = 0;
    
    switch (msr) {
        // ★ STEALTH: VMX-bezogene MSRs verstecken ★
        case IA32_VMX_BASIC:
        case IA32_VMX_PINBASED_CTLS:
        case IA32_VMX_PROCBASED_CTLS:
        case IA32_VMX_EXIT_CTLS:
        case IA32_VMX_ENTRY_CTLS:
        case IA32_VMX_MISC:
        case IA32_VMX_CR0_FIXED0:
        case IA32_VMX_CR0_FIXED1:
        case IA32_VMX_CR4_FIXED0:
        case IA32_VMX_CR4_FIXED1:
        case IA32_VMX_VMCS_ENUM:
        case IA32_VMX_PROCBASED_CTLS2:
        case IA32_VMX_EPT_VPID_CAP:
        case IA32_VMX_TRUE_PINBASED_CTLS:
        case IA32_VMX_TRUE_PROCBASED_CTLS:
        case IA32_VMX_TRUE_EXIT_CTLS:
        case IA32_VMX_TRUE_ENTRY_CTLS:
            if (hv->stealth_mode) {
                // General Protection Fault (#GP) injizieren
                cosmos_vmwrite(VMCS_ENTRY_INT_INFO, (1U << 31) | (1U << 11) | (3U << 8) | 13U);
                cosmos_vmwrite(VMCS_ENTRY_EXCEPTION_ERRCODE, 0);
                value = 0;
            } else {
                value = cosmos_rdmsr(msr);
            }
            break;
            
        default:
            // Anti-Cheat Hardening MSR Filter
            if (hv->stealth_mode && cosmos_ac_handle_rdmsr(hv, vcpu, msr, &value)) {
                // Wurde vom Anti-Cheat Modul behandelt
                break;
            }
            // Alle anderen MSRs: echten Wert durchreichen
            value = cosmos_rdmsr(msr);
            break;
    }
    
    vcpu->guest_regs.rax = (uint32_t)(value & 0xFFFFFFFF);
    vcpu->guest_regs.rdx = (uint32_t)(value >> 32);
}

/* --- WRMSR Handler --- */
void cosmos_hv_handle_wrmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint32_t msr = (uint32_t)vcpu->guest_regs.rcx;
    uint64_t value = ((uint64_t)(uint32_t)vcpu->guest_regs.rdx << 32) |
                     (uint32_t)vcpu->guest_regs.rax;
    
    switch (msr) {
        // VMX MSRs: im Stealth-Modus ignorieren
        case IA32_VMX_BASIC:
        case IA32_VMX_PINBASED_CTLS:
        case IA32_VMX_PROCBASED_CTLS:
            if (hv->stealth_mode) {
                // General Protection Fault (#GP) injizieren
                cosmos_vmwrite(VMCS_ENTRY_INT_INFO, (1U << 31) | (1U << 11) | (3U << 8) | 13U);
                cosmos_vmwrite(VMCS_ENTRY_EXCEPTION_ERRCODE, 0);
                return;
            }
            break;
            
        default:
            // Durchreichen
            cosmos_wrmsr(msr, value);
            break;
    }
}

/* ============================================================================
 *  VM-EXIT HANDLER (Hauptdispatcher)
 *
 *  Diese Funktion wird von cosmos_vmexit_stub (Assembly) aufgerufen.
 *  Sie liest den Exit-Grund und dispatcht zum richtigen Handler.
 * ============================================================================ */

// Globaler Hypervisor State (Singleton)
static cosmos_hypervisor g_hypervisor;

// Accessor für den globalen Hypervisor
cosmos_hypervisor* cosmos_hv_get_global(void) {
    return &g_hypervisor;
}

extern "C" int cosmos_vmexit_handler(vmx_guest_regs* guest_regs) {
    cosmos_hypervisor* hv = &g_hypervisor;
    
    // Wir holen die korrekte CPU ID, da wir Multicore-fähig sind!
    // Der Linux GS-Base Pointer ist im Gast aktiv und vmexit_stub
    // läuft auf dem regulären Linux-Stack, daher ist smp_processor_id() sicher!
    extern unsigned int cosmos_get_cpu_id(void);
    uint32_t cpu_id = cosmos_get_cpu_id();
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    
    // Gast-Register kopieren
    vcpu->guest_regs = *guest_regs;
    
    // TSC bei VM-Exit erfassen (für Stealth-Kompensation)
    uint64_t exit_tsc = cosmos_rdtsc();
    
    // Exit-Grund aus VMCS lesen
    uint64_t exit_reason_raw = 0;
    cosmos_vmread(VMCS_EXIT_REASON, &exit_reason_raw);
    uint32_t exit_reason = (uint32_t)(exit_reason_raw & 0xFFFF);
    
    // Exit Qualification (zusätzliche Infos)
    uint64_t exit_qualification = 0;
    cosmos_vmread(VMCS_EXIT_QUALIFICATION, &exit_qualification);
    
    // Gast-RIP für Instruktionslänge
    uint64_t guest_rip = 0;
    uint64_t instr_len = 0;
    cosmos_vmread(VMCS_GUEST_RIP, &guest_rip);
    cosmos_vmread(VMCS_EXIT_INSTR_LENGTH, &instr_len);
    
    int action = 0; // 0 = resume, 1 = abort
    switch (exit_reason) {
        case VMEXIT_CPUID:
            cosmos_hv_handle_cpuid(hv, vcpu);
            // RIP vorrücken (CPUID ist 2 Bytes: 0F A2)
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            

        case VMEXIT_RDTSC:
            cosmos_hv_handle_rdtsc(hv, vcpu);
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_RDTSCP:
            cosmos_hv_handle_rdtsc(hv, vcpu);
            // RDTSCP setzt auch ECX = IA32_TSC_AUX
            vcpu->guest_regs.rcx = cosmos_rdmsr(IA32_TSC_AUX);
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_RDMSR:
            cosmos_hv_handle_rdmsr(hv, vcpu);
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_WRMSR:
            cosmos_hv_handle_wrmsr(hv, vcpu);
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_EPT_VIOLATION:
            if (cosmos_hv_handle_ept_violation(hv, vcpu) != 0) {
                HV_LOG("CRITICAL: EPT Violation @ GPA=0x%llX, RIP=0x%llX. Aborting!",
                       (unsigned long long)exit_qualification,
                       (unsigned long long)guest_rip);
                action = exit_reason | 0x10000;
            }
            break;
            
        case VMEXIT_HLT:
            // HLT: Gast will anhalten → weiterlaufen lassen
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_VMCALL:
            // Hypercall von CosmOS (für interne Kommunikation)
            if (vcpu->guest_regs.rax == 0x1337) {
                // EPT TLB Flush (Hypercall)
                extern void _Z13cosmos_inveptmm(unsigned long type, unsigned long desc);
                _Z13cosmos_inveptmm(2, 0); // Global INVEPT
                HV_LOG("CPU %u: VMCALL 0x1337 (EPT Flush)", cpu_id);
            } else {
                HV_LOG("VMCALL: RAX=0x%llX", (unsigned long long)vcpu->guest_regs.rax);
            }
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_CR_ACCESS:
            {
                uint64_t qual = exit_qualification;
                uint32_t cr = qual & 0xF;
                uint32_t access_type = (qual >> 4) & 3;
                uint32_t reg = (qual >> 8) & 0xF;
                
                uint64_t val = 0;
                if (access_type == 0) { // MOV to CR
                    switch(reg) {
                        case 0: val = vcpu->guest_regs.rax; break;
                        case 1: val = vcpu->guest_regs.rcx; break;
                        case 2: val = vcpu->guest_regs.rdx; break;
                        case 3: val = vcpu->guest_regs.rbx; break;
                        case 4: cosmos_vmread(VMCS_GUEST_RSP, &val); break;
                        case 5: val = vcpu->guest_regs.rbp; break;
                        case 6: val = vcpu->guest_regs.rsi; break;
                        case 7: val = vcpu->guest_regs.rdi; break;
                        case 8: val = vcpu->guest_regs.r8; break;
                        case 9: val = vcpu->guest_regs.r9; break;
                        case 10: val = vcpu->guest_regs.r10; break;
                        case 11: val = vcpu->guest_regs.r11; break;
                        case 12: val = vcpu->guest_regs.r12; break;
                        case 13: val = vcpu->guest_regs.r13; break;
                        case 14: val = vcpu->guest_regs.r14; break;
                        case 15: val = vcpu->guest_regs.r15; break;
                    }
                    if (cr == 0) {
                        cosmos_vmwrite(VMCS_GUEST_CR0, val);
                        cosmos_vmwrite(VMCS_CR0_READ_SHADOW, val);
                    } else if (cr == 3) {
                        cosmos_vmwrite(VMCS_GUEST_CR3, val);
                    } else if (cr == 4) {
                        cosmos_vmwrite(VMCS_CR4_READ_SHADOW, val);
                        cosmos_vmwrite(VMCS_GUEST_CR4, val | CR4_VMXE);
                    }
                } else if (access_type == 1) { // MOV from CR
                    if (cr == 0) cosmos_vmread(VMCS_CR0_READ_SHADOW, &val);
                    else if (cr == 3) cosmos_vmread(VMCS_GUEST_CR3, &val);
                    else if (cr == 4) cosmos_vmread(VMCS_CR4_READ_SHADOW, &val);
                    
                    if (reg == 4) cosmos_vmwrite(VMCS_GUEST_RSP, val);
                    else {
                        switch(reg) {
                            case 0: vcpu->guest_regs.rax = val; break;
                            case 1: vcpu->guest_regs.rcx = val; break;
                            case 2: vcpu->guest_regs.rdx = val; break;
                            case 3: vcpu->guest_regs.rbx = val; break;
                            case 5: vcpu->guest_regs.rbp = val; break;
                            case 6: vcpu->guest_regs.rsi = val; break;
                            case 7: vcpu->guest_regs.rdi = val; break;
                            case 8: vcpu->guest_regs.r8 = val; break;
                            case 9: vcpu->guest_regs.r9 = val; break;
                            case 10: vcpu->guest_regs.r10 = val; break;
                            case 11: vcpu->guest_regs.r11 = val; break;
                            case 12: vcpu->guest_regs.r12 = val; break;
                            case 13: vcpu->guest_regs.r13 = val; break;
                            case 14: vcpu->guest_regs.r14 = val; break;
                            case 15: vcpu->guest_regs.r15 = val; break;
                        }
                    }
                }
            }
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_XSETBV:
            {
                uint32_t ecx = (uint32_t)vcpu->guest_regs.rcx;
                uint32_t eax = (uint32_t)vcpu->guest_regs.rax;
                uint32_t edx = (uint32_t)vcpu->guest_regs.rdx;
                __asm__ __volatile__("xsetbv" : : "c"(ecx), "a"(eax), "d"(edx));
            }
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_INVD:
        case VMEXIT_WBINVD:
            __asm__ __volatile__("wbinvd");
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_VMCLEAR:
        case VMEXIT_VMPTRLD:
        case VMEXIT_VMPTRST:
        case VMEXIT_VMREAD:
        case VMEXIT_VMWRITE:
        case VMEXIT_VMXOFF:
        case VMEXIT_VMXON:
        case VMEXIT_VMLAUNCH:
        case VMEXIT_VMRESUME:
            // Wenn der Gast VMX-Befehle ausführt, injiziere #UD (Invalid Opcode)
            cosmos_vmwrite(VMCS_ENTRY_INT_INFO, (1U << 31) | (3U << 8) | 6U); 
            // RIP wird NICHT vorgerückt bei Exceptions!
            break;
            
        case VMEXIT_TRIPLE_FAULT:
            HV_LOG("!!! TRIPLE FAULT im Gast! RIP=0x%llX !!!", 
                   (unsigned long long)guest_rip);
            action = exit_reason | 0x10000;
            break;
            
        default:
            HV_LOG("Unbehandelter VM-Exit: reason=%u, qual=0x%llX, RIP=0x%llX. ABORT!",
                   exit_reason,
                   (unsigned long long)exit_qualification,
                   (unsigned long long)guest_rip);
            action = exit_reason | 0x10000;
            break;
    }
    
    // TSC-Stealth: Zeit für VM-Exit verstecken
    if (action == 0) {
        uint64_t new_exit_tsc = cosmos_rdtsc();
        if (hv->stealth_mode) {
            // Standard-Kosten für VM-Exits
            uint64_t simulated_cost = (exit_reason == VMEXIT_CPUID) ? VMEXIT_COST_CYCLES_CPUID : VMEXIT_COST_CYCLES_RDMSR;
            cosmos_ac_hide_vmexit_time(vcpu, exit_tsc, new_exit_tsc, simulated_cost);
        } else {
            // Altes TSC-Stealth (naiv)
            uint64_t exit_cost = new_exit_tsc - exit_tsc;
            vcpu->tsc_offset -= (int64_t)exit_cost;
            cosmos_vmwrite(VMCS_TSC_OFFSET, (uint64_t)vcpu->tsc_offset);
        }
    }
    
    // Register zurückschreiben (der Assembly-Stub stellt sie wieder her)
    *guest_regs = vcpu->guest_regs;
    
    return action;
}
