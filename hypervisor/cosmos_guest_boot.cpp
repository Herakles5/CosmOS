/*
 * ============================================================================
 *  COSMOS GUEST BOOT - Implementation
 * ============================================================================
 *
 *  Implementiert den OVMF-Boot (UEFI) für den Windows NT Gast.
 *  Voraussetzung: Unrestricted Guest (damit die VM im Real Mode starten kann).
 * ============================================================================
 */

#include "cosmos_guest_boot.h"

#ifdef __linux__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define BOOT_LOG(fmt, ...) printf("[BOOT] " fmt "\n", ##__VA_ARGS__)

// Page Allocator Wrapper
static void* boot_alloc_pages(uint64_t size, uint64_t* out_phys) {
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    memset(p, 0, size);
    *out_phys = (uint64_t)p;
    return p;
}

#else
extern "C" int printf(const char* fmt, ...);
extern "C" void* memset(void*, int, unsigned long);
extern "C" void* memcpy(void*, const void*, unsigned long);
extern void* cosmos_alloc_pages_aligned(uint64_t size, uint64_t* out_phys);
extern int cosmos_vfs_read_file(const char* path, void** out_buf, uint64_t* out_size);

#define BOOT_LOG(fmt, ...) printf("[BOOT] " fmt "\n", ##__VA_ARGS__)

static void* boot_alloc_pages(uint64_t size, uint64_t* out_phys) {
    return cosmos_alloc_pages_aligned(size, out_phys);
}
#endif

/* ============================================================================
 *  INIT
 * ============================================================================ */
int cosmos_boot_init(cosmos_boot_mgr* mgr) {
    if (!mgr) return -1;
    memset(mgr, 0, sizeof(cosmos_boot_mgr));
    
    BOOT_LOG("╔══════════════════════════════════════════╗");
    BOOT_LOG("║   CosmOS Guest Boot Manager (UEFI/OVMF)  ║");
    BOOT_LOG("╚══════════════════════════════════════════╝");
    
    return 0;
}

/* ============================================================================
 *  FIRMWARE LADEN & EPT MAPPING
 * ============================================================================ */
int cosmos_boot_load_ovmf(cosmos_boot_mgr* mgr, const char* filepath) {
    if (!mgr) return -1;
    
    BOOT_LOG("Lade Firmware: %s", filepath);
    
#ifdef __linux__
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        BOOT_LOG("FEHLER: Konnte %s nicht öffnen!", filepath);
        return -1;
    }
    
    struct stat st;
    fstat(fd, &st);
    mgr->firmware_size = st.st_size;
    
    // Speicher allozieren (2MB aligned für EPT)
    uint64_t alloc_size = (mgr->firmware_size + EPT_PAGE_SIZE_2M - 1) & ~(EPT_PAGE_SIZE_2M - 1);
    mgr->firmware_data = (uint8_t*)boot_alloc_pages(alloc_size, &mgr->firmware_phys);
    
    if (!mgr->firmware_data) {
        BOOT_LOG("FEHLER: Speichermangel für Firmware!");
        close(fd);
        return -2;
    }
    
    // In Speicher lesen
    ssize_t bytes_read = read(fd, mgr->firmware_data, mgr->firmware_size);
    close(fd);
    
    if (bytes_read != (ssize_t)mgr->firmware_size) {
        BOOT_LOG("FEHLER: Konnte Datei nicht komplett lesen!");
        return -3;
    }
#else
    // Bare Metal: VFS benutzen
    void* buf = nullptr;
    uint64_t size = 0;
    if (cosmos_vfs_read_file(filepath, &buf, &size) != 0) {
        BOOT_LOG("FEHLER: Konnte %s nicht öffnen!", filepath);
        return -1;
    }
    
    mgr->firmware_size = size;
    uint64_t alloc_size = (size + EPT_PAGE_SIZE_2M - 1) & ~(EPT_PAGE_SIZE_2M - 1);
    mgr->firmware_data = (uint8_t*)boot_alloc_pages(alloc_size, &mgr->firmware_phys);
    
    if (!mgr->firmware_data) return -2;
    memcpy(mgr->firmware_data, buf, size);
#endif
    
    BOOT_LOG("Firmware geladen: %llu KB", (unsigned long long)(mgr->firmware_size / 1024));
    mgr->is_ready = 1;
    
    return 0;
}

/* ============================================================================
 *  VMCS SETUP FÜR REAL MODE (UNRESTRICTED GUEST)
 * ============================================================================ */
static void set_segment(uint32_t selector_field, uint32_t base_field, 
                        uint32_t limit_field, uint32_t ar_field,
                        uint16_t selector, uint64_t base, uint32_t limit, uint32_t ar) {
    cosmos_vmwrite(selector_field, selector);
    cosmos_vmwrite(base_field, base);
    cosmos_vmwrite(limit_field, limit);
    cosmos_vmwrite(ar_field, ar);
}

int cosmos_boot_setup_vmcs(cosmos_boot_mgr* mgr, cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!mgr || !mgr->is_ready || !hv || cpu_id >= 256) return -1;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    if (!vcpu->vmcs_active) return -2;
    
    BOOT_LOG("Konfiguriere VMCS für CPU %u (16-bit Real Mode Boot)", cpu_id);
    
    // 1. EPT Mapping für das ROM
    // Die Firmware wird physikalisch direkt unter 4GB (0xFFE00000) gemapped.
    // Gast-Physikalisch = Host-Physikalisch
    uint64_t guest_rom_base = (0x100000000ULL - mgr->firmware_size);
    
    // Flags: Read+Execute, Cacheable
    uint64_t rom_flags = EPT_READ | EPT_EXECUTE | (EPT_MT_WB << EPT_MEMORY_TYPE_SHIFT) | EPT_IGNORE_PAT;
    
    for (uint64_t offset = 0; offset < mgr->firmware_size; offset += EPT_PAGE_SIZE_2M) {
        uint64_t gpa = guest_rom_base + offset;
        
        // Trick: Da wir im Host-OS Speicher alloziert haben, liegt die Firmware
        // irgendwo. Wir müssen die EPT-Einträge für den Gast bei 0xFFE00000 
        // auf unsere allozierten Host-Seiten umbiegen.
        
        // Lese aktuellen PML4/PDPT/PD
        uint64_t* pml4 = (uint64_t*)hv->ept_pml4_virt;
        uint64_t pml4_idx = ept_pml4_index(gpa);
        uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & EPT_ADDR_MASK);
        uint64_t pdpt_idx = ept_pdpt_index(gpa);
        uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & EPT_ADDR_MASK);
        uint64_t pd_idx = ept_pd_index(gpa);
        
        // Überschreibe den Identity-Map Eintrag mit der Firmware-Adresse
        uint64_t hpa = mgr->firmware_phys + offset;
        pd[pd_idx] = hpa | EPT_LARGE_PAGE | rom_flags;
        
        BOOT_LOG("  Mappe ROM GPA 0x%llX -> HPA 0x%llX", 
                 (unsigned long long)gpa, (unsigned long long)hpa);
    }
    
    // TLB Flush
    extern void cosmos_invept(uint64_t type, uint64_t eptp);
    uint64_t eptp = ept_make_eptp(hv->ept_pml4_phys);
    cosmos_invept(1, eptp);
    
    // 2. Control Registers für Real Mode (Unrestricted Guest)
    // CR0: Alles aus außer NE (Numeric Error)
    cosmos_vmwrite(VMCS_GUEST_CR0, 0x20);      // Nur NE
    cosmos_vmwrite(VMCS_GUEST_CR3, 0);         // Kein Paging
    cosmos_vmwrite(VMCS_GUEST_CR4, 0x2000);    // VMXE
    
    // EFER (Extended Feature Enable Register) = 0
    cosmos_vmwrite(VMCS_GUEST_SYSENTER_EIP, 0);
    cosmos_vmwrite(VMCS_GUEST_SYSENTER_ESP, 0);
    cosmos_vmwrite(VMCS_GUEST_SYSENTER_CS, 0);
    
    // 3. Segment Register (Real Mode)
    // Access Rights: 0x93 = Present, Read/Write, Accessed (Standard Data/Code Segment im Real Mode)
    // CS ist speziell: Base=0xFFFF0000, Limit=0xFFFF (für Reset Vector)
    set_segment(VMCS_GUEST_ES_SEL, VMCS_GUEST_ES_BASE, VMCS_GUEST_ES_LIMIT, VMCS_GUEST_ES_ACCESS,
                0, 0, 0xFFFF, 0x93);
    set_segment(VMCS_GUEST_SS_SEL, VMCS_GUEST_SS_BASE, VMCS_GUEST_SS_LIMIT, VMCS_GUEST_SS_ACCESS,
                0, 0, 0xFFFF, 0x93);
    set_segment(VMCS_GUEST_DS_SEL, VMCS_GUEST_DS_BASE, VMCS_GUEST_DS_LIMIT, VMCS_GUEST_DS_ACCESS,
                0, 0, 0xFFFF, 0x93);
    set_segment(VMCS_GUEST_FS_SEL, VMCS_GUEST_FS_BASE, VMCS_GUEST_FS_LIMIT, VMCS_GUEST_FS_ACCESS,
                0, 0, 0xFFFF, 0x93);
    set_segment(VMCS_GUEST_GS_SEL, VMCS_GUEST_GS_BASE, VMCS_GUEST_GS_LIMIT, VMCS_GUEST_GS_ACCESS,
                0, 0, 0xFFFF, 0x93);
                
    // CS: F000:FFF0 -> Phys = FFFF0000 + FFF0 = FFFFFFF0 (Reset Vector)
    set_segment(VMCS_GUEST_CS_SEL, VMCS_GUEST_CS_BASE, VMCS_GUEST_CS_LIMIT, VMCS_GUEST_CS_ACCESS,
                0xF000, 0xFFFF0000, 0xFFFF, 0x9B); // 0x9B = Code, Read/Execute, Accessed
                
    // TR, LDTR
    set_segment(VMCS_GUEST_TR_SEL, VMCS_GUEST_TR_BASE, VMCS_GUEST_TR_LIMIT, VMCS_GUEST_TR_ACCESS,
                0, 0, 0xFFFF, 0x008B); // 32-bit TSS (Busy)
    set_segment(VMCS_GUEST_LDTR_SEL, VMCS_GUEST_LDTR_BASE, VMCS_GUEST_LDTR_LIMIT, VMCS_GUEST_LDTR_ACCESS,
                0, 0, 0xFFFF, 0x10000); // Unusable
                
    // GDTR / IDTR
    cosmos_vmwrite(VMCS_GUEST_GDTR_BASE, 0);
    cosmos_vmwrite(VMCS_GUEST_GDTR_LIMIT, 0xFFFF);
    cosmos_vmwrite(VMCS_GUEST_IDTR_BASE, 0);
    cosmos_vmwrite(VMCS_GUEST_IDTR_LIMIT, 0xFFFF);
    
    // 4. RIP und RFLAGS
    cosmos_vmwrite(VMCS_GUEST_RFLAGS, 0x2); // Bit 1 ist immer 1
    cosmos_vmwrite(VMCS_GUEST_RIP, 0xFFF0); // RIP = 0xFFF0
    cosmos_vmwrite(VMCS_GUEST_RSP, 0);
    
    // 5. Activity State
    cosmos_vmwrite(VMCS_GUEST_ACTIVITY_STATE, 0); // Active
    
    // 6. Guest GPRs zurücksetzen
    memset(&vcpu->guest_regs, 0, sizeof(vmx_guest_regs));
    
    BOOT_LOG("★ VMCS für UEFI Boot vorbereitet!");
    BOOT_LOG("  CS:RIP = F000:FFF0 (Base: 0xFFFF0000)");
    BOOT_LOG("  Nächster VMLAUNCH startet das OVMF BIOS.");
    
    return 0;
}
