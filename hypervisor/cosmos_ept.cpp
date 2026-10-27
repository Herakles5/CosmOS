/*
 * ============================================================================
 *  COSMOS EPT BUILDER - Extended Page Table Identity Map
 * ============================================================================
 *
 *  Phase 1.5: Baut eine 1:1 Identity Map (GPA == HPA) mit 2MB Large Pages.
 *
 *  Für den i7-7700 mit 16GB RAM:
 *    PML4[0] → PDPT
 *    PDPT[0..15] → PD[0..15]  (16 × 1GB = 16GB)
 *    Jede PD hat 512 Einträge à 2MB = 1GB pro PD
 *    Total: 16 × 512 = 8192 2MB-Pages = 16GB gemapped
 *
 *  Speicherbedarf: 1 PML4 + 1 PDPT + 16 PDs = 18 × 4KB = 72KB
 *
 *  MMIO-Bereiche (z.B. GPU, PCI Config Space) werden als Uncacheable (UC)
 *  gemapped, damit Hardware-Register korrekt funktionieren.
 * ============================================================================
 */

#include "cosmos_hypervisor.h"
#include "cosmos_ept.h"

#ifdef __linux__
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void* ept_alloc_page(uint64_t* out_phys) {
    void* p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    memset(p, 0, 4096);
    *out_phys = (uint64_t)p;  // Linux: virt == phys (für Tests)
    return p;
}

#define EPT_LOG(fmt, ...) printf("[EPT] " fmt "\n", ##__VA_ARGS__)

#else
extern void* cosmos_alloc_page_aligned(uint64_t* out_phys);
extern "C" void* cosmos_phys_to_virt(uint64_t phys_addr);
extern "C" int printf(const char* fmt, ...);

static void* ept_alloc_page(uint64_t* out_phys) {
    return cosmos_alloc_page_aligned(out_phys);
}

#define EPT_LOG(fmt, ...) printf("[EPT] " fmt "\n", ##__VA_ARGS__)
#endif

/* ============================================================================
 *  MMIO REGIONS
 *
 *  Diese physischen Adressbereiche gehören zu Hardware-Geräten und
 *  DÜRFEN NICHT als Write-Back gecached werden. Sie müssen UC sein.
 *
 *  Typische MMIO-Bereiche auf einem Desktop-PC:
 *  - 0xA0000  - 0xBFFFF:  VGA Frame Buffer (Legacy)
 *  - 0xC0000  - 0xFFFFF:  ROM Area
 *  - 0xE0000000 - 0xEFFFFFFF: PCIe MMCONFIG (256MB)
 *  - 0xF0000000 - 0xFFFFFFFF: Local APIC, IOAPIC, HPET, etc.
 *  - Über 4GB: GPU VRAM (variabel, oft 0x6000000000+)
 * ============================================================================ */

struct mmio_region {
    uint64_t start;
    uint64_t end;
    const char* name;
};

static const mmio_region known_mmio_regions[] = {
    { 0x000A0000,   0x000FFFFF,   "Legacy VGA + ROM" },
    { 0xE0000000,   0xEFFFFFFF,   "PCIe MMCONFIG" },
    { 0xF0000000,   0xFEFFFFFF,   "PCI MMIO Window" },
    { 0xFEC00000,   0xFEC00FFF,   "IOAPIC" },
    { 0xFED00000,   0xFED003FF,   "HPET" },
    { 0xFEE00000,   0xFEE00FFF,   "Local APIC" },
    { 0xFF000000,   0xFFFFFFFF,   "Flash/BIOS ROM" },
    { 0, 0, nullptr }  // Sentinel
};

// Prüfe ob eine 2MB-Page (start_addr bis start_addr+2MB) in einer MMIO-Region liegt
static bool is_mmio_region(uint64_t page_start) {
    uint64_t page_end = page_start + EPT_PAGE_SIZE_2M - 1;
    
    for (int i = 0; known_mmio_regions[i].name != nullptr; i++) {
        // Überlappung prüfen
        if (page_start <= known_mmio_regions[i].end && 
            page_end >= known_mmio_regions[i].start) {
            return true;
        }
    }
    return false;
}

/* ============================================================================
 *  EPT IDENTITY MAP BUILDER
 *
 *  Erstellt die komplette EPT-Hierarchie:
 *    PML4 → PDPT → PD[] (mit 2MB Large Pages)
 *
 *  Jede 2MB-Page mappt GPA direkt auf HPA (1:1).
 *  MMIO-Bereiche werden als Uncacheable markiert.
 * ============================================================================ */
int cosmos_hv_setup_ept(cosmos_hypervisor* hv) {
    if (!hv) return -1;
    
    EPT_LOG("========================================");
    EPT_LOG("  EPT Identity Map Builder");
    EPT_LOG("  Phase 1.5: Speicher-Virtualisierung");
    EPT_LOG("========================================");
    
    // Wir mappen 512GB: 16GB RAM + MMIO-Bereiche (PCIe Geräte liegen oft >128GB)
    // 512GB entspricht exakt einer voll ausgeschöpften PDPT.
    uint64_t map_size = 512ULL * 1024 * 1024 * 1024;  // 512GB
    uint32_t num_gb = (uint32_t)(map_size / EPT_PAGE_SIZE_1G);
    
    EPT_LOG("Mappe %u GB physischen Speicher (Identity Map)", num_gb);
    
    // --- Schritt 1: PML4 allozieren ---
    uint64_t pml4_phys;
    uint64_t* pml4 = (uint64_t*)ept_alloc_page(&pml4_phys);
    if (!pml4) {
        EPT_LOG("FEHLER: PML4 konnte nicht alloziert werden!");
        return -1;
    }
    
    hv->ept_pml4_phys = pml4_phys;
    hv->ept_pml4_virt = pml4;
    
    EPT_LOG("PML4 @ phys=0x%llX", (unsigned long long)pml4_phys);
    
    // --- Schritt 2: PDPT allozieren ---
    // Eine PDPT deckt 512GB ab (512 Einträge × 1GB).
    // Für 512GB brauchen wir nur 1 PDPT.
    uint64_t pdpt_phys;
    uint64_t* pdpt = (uint64_t*)ept_alloc_page(&pdpt_phys);
    if (!pdpt) {
        EPT_LOG("FEHLER: PDPT konnte nicht alloziert werden!");
        return -2;
    }
    
    // PML4[0] → PDPT (deckt die unteren 512GB ab)
    pml4[0] = ept_make_table_entry(pdpt_phys);
    
    EPT_LOG("PDPT @ phys=0x%llX (PML4[0])", (unsigned long long)pdpt_phys);
    
    // --- Schritt 3: Page Directories allozieren (eine pro GB) ---
    uint32_t pages_mapped = 0;
    uint32_t mmio_pages = 0;
    
    for (uint32_t gb = 0; gb < num_gb; gb++) {
        uint64_t pd_phys;
        uint64_t* pd = (uint64_t*)ept_alloc_page(&pd_phys);
        if (!pd) {
            EPT_LOG("FEHLER: PD[%u] konnte nicht alloziert werden!", gb);
            return -3;
        }
        
        // PDPT[gb] → PD
        pdpt[gb] = ept_make_table_entry(pd_phys);
        
        // 512 Einträge à 2MB = 1GB pro PD
        for (uint32_t entry = 0; entry < 512; entry++) {
            uint64_t phys_addr = ((uint64_t)gb * EPT_PAGE_SIZE_1G) + 
                                 ((uint64_t)entry * EPT_PAGE_SIZE_2M);
            
            uint64_t flags;
            
            if (is_mmio_region(phys_addr)) {
                // MMIO: Uncacheable, Read+Write+Execute
                flags = EPT_RWX | (EPT_MT_UC << EPT_MEMORY_TYPE_SHIFT) | EPT_IGNORE_PAT;
                mmio_pages++;
            } else if (phys_addr == 0) {
                // Erste 2MB-Page: Enthält Real-Mode IVT, BDA, etc.
                // Trotzdem WB, aber wir könnten hier später feinere Kontrolle haben
                flags = EPT_RWX_WB;
            } else {
                // Normaler RAM: Write-Back
                flags = EPT_RWX_WB;
            }
            
            pd[entry] = ept_make_2m_entry(phys_addr, flags);
            pages_mapped++;
        }
    }
    
    // --- Schritt 4: EPTP zusammenbauen ---
    uint64_t eptp = ept_make_eptp(pml4_phys);
    
    EPT_LOG("--- EPT Statistik ---");
    EPT_LOG("  Gemappte 2MB-Pages: %u", pages_mapped);
    EPT_LOG("  Davon MMIO (UC):    %u", mmio_pages);
    EPT_LOG("  Davon RAM (WB):     %u", pages_mapped - mmio_pages);
    EPT_LOG("  Gesamter Speicher:  %llu MB", 
           (unsigned long long)(pages_mapped * 2));
    EPT_LOG("  Tabellen alloziert: %u (PML4=1, PDPT=1, PDs=%u)",
           2 + num_gb, num_gb);
    EPT_LOG("  Speicherbedarf:     %u KB", (2 + num_gb) * 4);
    EPT_LOG("  EPTP:               0x%llX", (unsigned long long)eptp);
    
    // --- Schritt 5: EPTP in alle aktiven VMCS schreiben ---
    for (uint32_t cpu = 0; cpu < 256; cpu++) {
        if (hv->vcpus[cpu].vmcs_active) {
            // Sicherstellen, dass der richtige VMCS geladen ist
            if (cosmos_vmptrld(&hv->vcpus[cpu].vmcs_phys) != 0) {
                EPT_LOG("WARNUNG: VMPTRLD fehlgeschlagen für CPU %u", cpu);
                continue;
            }
            
            cosmos_vmwrite(VMCS_EPTP, eptp);
            EPT_LOG("CPU %u: EPTP in VMCS geschrieben", cpu);
        }
    }
    
    EPT_LOG("========================================");
    EPT_LOG("  ★ EPT IDENTITY MAP KOMPLETT ★");
    EPT_LOG("========================================");
    
    return 0;
}

/* ============================================================================
 *  EPT PAGE FAULT HANDLER (für spätere dynamische Erweiterung)
 *
 *  Wird aufgerufen bei EPT Violation VM-Exit.
 *  Aktuell nur Logging, später: dynamisches Mapping, Copy-on-Write, etc.
 * ============================================================================ */
int cosmos_hv_handle_ept_violation(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint64_t guest_phys = 0;
    uint64_t exit_qual = 0;
    uint64_t guest_rip = 0;
    
    cosmos_vmread(VMCS_EXIT_QUALIFICATION, &exit_qual);
    // Guest Physical Address ist in einem separaten VMCS-Feld
    // (VMCS_GUEST_PHYSICAL_ADDR = 0x2400)
    cosmos_vmread(0x2400, &guest_phys);
    cosmos_vmread(VMCS_GUEST_RIP, &guest_rip);
    
    bool is_read    = (exit_qual >> 0) & 1;
    bool is_write   = (exit_qual >> 1) & 1;
    bool is_execute = (exit_qual >> 2) & 1;
    
    EPT_LOG("EPT VIOLATION:");
    EPT_LOG("  Guest Phys Addr: 0x%llX", (unsigned long long)guest_phys);
    EPT_LOG("  Guest RIP:       0x%llX", (unsigned long long)guest_rip);
    EPT_LOG("  Access Type:     %s%s%s",
           is_read ? "READ " : "",
           is_write ? "WRITE " : "",
           is_execute ? "EXEC" : "");
    
    // TODO: Dynamisches Mapping implementieren
    // Für jetzt: Triple Fault vermeiden, indem wir die Page mit RWX mappen
    // Das ist unsicher, aber für Debugging nützlich
    
    return -1;  // Signalisiert: nicht behandelt
}

/* ============================================================================
 *  EPT MEMORY PROTECTION (für spätere Stealth-Features)
 *
 *  Erlaubt es, bestimmte Speicherbereiche des Gasts zu schützen:
 *  - Hypervisor-Code verstecken (Execute-only, kein Read)
 *  - Gast-Kernel-Patches erkennen (Write-Protect)
 *  - GPU-VRAM isolieren
 * ============================================================================ */

// Ändere die Zugriffsrechte einer einzelnen 2MB-Page in der EPT
int cosmos_ept_set_page_flags(cosmos_hypervisor* hv, uint64_t guest_phys, uint64_t new_flags) {
    if (!hv || !hv->ept_pml4_virt) return -1;
    
    uint64_t* pml4 = (uint64_t*)hv->ept_pml4_virt;
    
    // PML4 Index
    uint64_t pml4_idx = ept_pml4_index(guest_phys);
    if (!(pml4[pml4_idx] & EPT_READ)) return -2;  // PML4 Entry nicht vorhanden
    
    // PDPT
    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & EPT_ADDR_MASK);
    uint64_t pdpt_idx = ept_pdpt_index(guest_phys);
    if (!(pdpt[pdpt_idx] & EPT_READ)) return -3;  // PDPT Entry nicht vorhanden
    
    // PD (hier sind unsere 2MB Large Pages)
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & EPT_ADDR_MASK);
    uint64_t pd_idx = ept_pd_index(guest_phys);
    
    // Physische Adresse beibehalten, nur Flags ändern
    uint64_t phys_addr = pd[pd_idx] & 0x000FFFFFFFE00000ULL;
    pd[pd_idx] = phys_addr | EPT_LARGE_PAGE | new_flags;
    
    // TLB für diese EPT-Übersetzung invalidieren
    extern void cosmos_invept(uint64_t type, uint64_t eptp);
    uint64_t eptp = ept_make_eptp(hv->ept_pml4_phys);
    cosmos_invept(1, eptp);  // Single-context invalidation
    
    return 0;
}

// Verstecke den Hypervisor-Speicher vor dem Gast
// Macht Pages Execute-Only (kein Read, kein Write) - der Gast kann
// den Code ausführen, aber nicht lesen oder modifizieren
int cosmos_ept_hide_memory(cosmos_hypervisor* hv, uint64_t phys_start, uint64_t size) {
    EPT_LOG("Verstecke Speicher: 0x%llX - 0x%llX (%llu KB)",
           (unsigned long long)phys_start,
           (unsigned long long)(phys_start + size),
           (unsigned long long)(size / 1024));
    
    // Auf 2MB-Grenzen alignen
    uint64_t aligned_start = phys_start & ~(EPT_PAGE_SIZE_2M - 1);
    uint64_t aligned_end = (phys_start + size + EPT_PAGE_SIZE_2M - 1) & ~(EPT_PAGE_SIZE_2M - 1);
    
    for (uint64_t addr = aligned_start; addr < aligned_end; addr += EPT_PAGE_SIZE_2M) {
        // Execute-Only + WB (Gast kann nicht lesen was der Hypervisor tut)
        uint64_t flags = EPT_EXECUTE | (EPT_MT_WB << EPT_MEMORY_TYPE_SHIFT) | EPT_IGNORE_PAT;
        int result = cosmos_ept_set_page_flags(hv, addr, flags);
        if (result != 0) {
            EPT_LOG("WARNUNG: Konnte Page 0x%llX nicht verstecken (err=%d)",
                   (unsigned long long)addr, result);
        }
    }
    
    return 0;
}

/* ============================================================================
 *  EPT PAGE HIDING (Memory Stealth)
 *
 *  Versteckt eine 4KB physische Speicherseite vor dem Gast-OS.
 *  Teilt bei Bedarf eine 2MB Large Page in 512 4KB Pages auf.
 * ============================================================================ */
int cosmos_ept_hide_page(cosmos_hypervisor* hv, uint64_t target_phys, uint64_t dummy_phys) {
    if (!hv || !hv->ept_pml4_virt) return -1;
    
    uint64_t* pml4 = (uint64_t*)hv->ept_pml4_virt;
    uint64_t pml4_idx = ept_pml4_index(target_phys);
    uint64_t pdpt_idx = ept_pdpt_index(target_phys);
    uint64_t pd_idx   = ept_pd_index(target_phys);
    uint64_t pt_idx   = ept_pt_index(target_phys);
    
    if ((pml4[pml4_idx] & EPT_READ) == 0) return -2;
    
    uint64_t* pdpt = (uint64_t*)cosmos_phys_to_virt(pml4[pml4_idx] & EPT_ADDR_MASK);
    if ((pdpt[pdpt_idx] & EPT_READ) == 0) return -3;
    
    uint64_t* pd = (uint64_t*)cosmos_phys_to_virt(pdpt[pdpt_idx] & EPT_ADDR_MASK);
    uint64_t pd_entry = pd[pd_idx];
    
    if ((pd_entry & EPT_READ) == 0) return -4;
    
    uint64_t* pt;
    if (pd_entry & EPT_LARGE_PAGE) {
        // 2MB Page gefunden! Wir müssen sie in 512 4KB Pages aufteilen.
        uint64_t pt_phys;
        pt = (uint64_t*)ept_alloc_page(&pt_phys);
        if (!pt) return -5; // Out of memory
        
        uint64_t base_phys = pd_entry & EPT_ADDR_MASK;
        uint64_t flags = pd_entry & 0xFFF;
        flags &= ~EPT_LARGE_PAGE; // Large Page bit entfernen
        
        // PT füllen
        for (uint32_t i = 0; i < 512; i++) {
            pt[i] = ept_make_4k_entry(base_phys + (i * 4096), flags);
        }
        
        // PD Eintrag aktualisieren
        pd[pd_idx] = ept_make_table_entry(pt_phys);
    } else {
        // Bereits eine 4KB Page Table
        pt = (uint64_t*)cosmos_phys_to_virt(pd_entry & EPT_ADDR_MASK);
    }
    
    // PT Eintrag für target_phys ändern
    uint64_t pt_entry = pt[pt_idx];
    uint64_t flags = pt_entry & 0xFFF;
    
    // Map auf Dummy Page!
    pt[pt_idx] = ept_make_4k_entry(dummy_phys, flags);
    
    // EPT TLB Flush wird vom Aufrufer (invept) gemacht.
    EPT_LOG("Verstecke Page 0x%llX -> gemappt auf 0x%llX", 
            (unsigned long long)target_phys, (unsigned long long)dummy_phys);
            
    return 0;
}

/* ============================================================================
 *  HYPERVISOR MEMORY STEALTH (Full EPT Hiding)
 *
 *  Iteriert über alle CPUs und versteckt die kritischen Hypervisor-Speicherseiten
 *  (VMXON, VMCS, MSR Bitmaps, Host-Stacks, Host-CR3) vor dem Gast-OS.
 *  Alle diese Seiten werden für den Gast auf eine "Dummy"-Zero-Page gemappt.
 * ============================================================================ */
int cosmos_ept_hide_hv_memory(cosmos_hypervisor* hv) {
    if (!hv || !hv->ept_pml4_virt) return -1;
    
    // 1. Erstelle eine Dummy-Page (mit Nullen gefüllt)
    uint64_t dummy_phys = 0;
    void* dummy_virt = ept_alloc_page(&dummy_phys);
    if (!dummy_virt) return -2;
    
    EPT_LOG("Stealth Dummy-Page alloziert @ 0x%llX", (unsigned long long)dummy_phys);
    
    // 2. Verstecke die Pages von allen CPUs
    for (uint32_t i = 0; i < 256; i++) {
        cosmos_vcpu* vcpu = &hv->vcpus[i];
        if (!vcpu->vmx_enabled) continue; // Überspringe inaktive CPUs
        
        if (vcpu->vmxon_phys) cosmos_ept_hide_page(hv, vcpu->vmxon_phys, dummy_phys);
        if (vcpu->vmcs_phys) cosmos_ept_hide_page(hv, vcpu->vmcs_phys, dummy_phys);
        if (vcpu->msr_bitmap_phys) cosmos_ept_hide_page(hv, vcpu->msr_bitmap_phys, dummy_phys);
        if (vcpu->host_cr3_phys) cosmos_ept_hide_page(hv, vcpu->host_cr3_phys, dummy_phys);
        
        // Host Stack ist größer (HOST_STACK_SIZE = 16KB = 4 Pages)
        if (vcpu->host_stack_phys) {
            for (int p = 0; p < 4; p++) {
                cosmos_ept_hide_page(hv, vcpu->host_stack_phys + (p * 4096), dummy_phys);
            }
        }
    }
    
    // 3. Verstecke die EPT PML4 selbst
    cosmos_ept_hide_page(hv, hv->ept_pml4_phys, dummy_phys);
    
    EPT_LOG("Alle Hypervisor-Strukturen sind nun in der EPT auf Dummy-Pages gemappt!");
    return 0;
}
