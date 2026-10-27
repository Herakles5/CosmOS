/*
 * ============================================================================
 *  COSMOS EPT BUILDER - Extended Page Table Identity Map
 * ============================================================================
 *
 *  Phase 1.5: Builds a 1:1 Identity Map (GPA == HPA) with 2MB Large Pages.
 *
 *  For i7-7700 with 16GB RAM:
 *    PML4[0] → PDPT
 *    PDPT[0..15] → PD[0..15]  (16 × 1GB = 16GB)
 *    Each PD has 512 entries of 2MB = 1GB per PD
 *    Total: 16 × 512 = 8192 2MB pages = 16GB mapped
 *
 *  Memory requirement: 1 PML4 + 1 PDPT + 16 PDs = 18 × 4KB = 72KB
 *
 *  MMIO ranges (e.g. GPU, PCI Config Space) are mapped as Uncacheable (UC)
 *  so that hardware registers function correctly.
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
    *out_phys = (uint64_t)p;  // Linux: virt == phys (for tests)
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
 *  These physical address ranges belong to hardware devices and
 *  MUST NOT be cached as Write-Back. They must be UC.
 *
 *  Typical MMIO ranges on a desktop PC:
 *  - 0xA0000  - 0xBFFFF:  VGA Frame Buffer (Legacy)
 *  - 0xC0000  - 0xFFFFF:  ROM Area
 *  - 0xE0000000 - 0xEFFFFFFF: PCIe MMCONFIG (256MB)
 *  - 0xF0000000 - 0xFFFFFFFF: Local APIC, IOAPIC, HPET, etc.
 *  - Above 4GB: GPU VRAM (variable, often 0x6000000000+)
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

// Check if a 2MB page (page_start to page_start+2MB) lies within an MMIO region
static bool is_mmio_region(uint64_t page_start) {
    uint64_t page_end = page_start + EPT_PAGE_SIZE_2M - 1;
    
    for (int i = 0; known_mmio_regions[i].name != nullptr; i++) {
        // Check overlap
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
 *  Constructs the complete EPT hierarchy:
 *    PML4 → PDPT → PD[] (with 2MB Large Pages)
 *
 *  Each 2MB page maps GPA directly to HPA (1:1).
 *  MMIO regions are marked as Uncacheable.
 * ============================================================================ */
int cosmos_hv_setup_ept(cosmos_hypervisor* hv) {
    if (!hv) return -1;
    
    EPT_LOG("========================================");
    EPT_LOG("  EPT Identity Map Builder");
    EPT_LOG("  Phase 1.5: Memory Virtualization");
    EPT_LOG("========================================");
    
    // We map 512GB: 16GB RAM + MMIO regions (PCIe devices often lie >128GB)
    // 512GB corresponds exactly to one fully utilized PDPT.
    uint64_t map_size = 512ULL * 1024 * 1024 * 1024;  // 512GB
    uint32_t num_gb = (uint32_t)(map_size / EPT_PAGE_SIZE_1G);
    
    EPT_LOG("Mapping %u GB physical memory (Identity Map)", num_gb);
    
    // --- Step 1: Allocate PML4 ---
    uint64_t pml4_phys;
    uint64_t* pml4 = (uint64_t*)ept_alloc_page(&pml4_phys);
    if (!pml4) {
        EPT_LOG("ERROR: Could not allocate PML4!");
        return -1;
    }
    
    hv->ept_pml4_phys = pml4_phys;
    hv->ept_pml4_virt = pml4;
    
    EPT_LOG("PML4 @ phys=0x%llX", (unsigned long long)pml4_phys);
    
    // --- Step 2: Allocate PDPT ---
    // One PDPT covers 512GB (512 entries × 1GB).
    // For 512GB we need only 1 PDPT.
    uint64_t pdpt_phys;
    uint64_t* pdpt = (uint64_t*)ept_alloc_page(&pdpt_phys);
    if (!pdpt) {
        EPT_LOG("ERROR: Could not allocate PDPT!");
        return -2;
    }
    
    // PML4[0] → PDPT (covers lower 512GB)
    pml4[0] = ept_make_table_entry(pdpt_phys);
    
    EPT_LOG("PDPT @ phys=0x%llX (PML4[0])", (unsigned long long)pdpt_phys);
    
    // --- Step 3: Allocate Page Directories (one per GB) ---
    uint32_t pages_mapped = 0;
    uint32_t mmio_pages = 0;
    
    for (uint32_t gb = 0; gb < num_gb; gb++) {
        uint64_t pd_phys;
        uint64_t* pd = (uint64_t*)ept_alloc_page(&pd_phys);
        if (!pd) {
            EPT_LOG("ERROR: Could not allocate PD[%u]!", gb);
            return -3;
        }
        
        // PDPT[gb] → PD
        pdpt[gb] = ept_make_table_entry(pd_phys);
        
        // 512 entries of 2MB = 1GB per PD
        for (uint32_t entry = 0; entry < 512; entry++) {
            uint64_t phys_addr = ((uint64_t)gb * EPT_PAGE_SIZE_1G) + 
                                 ((uint64_t)entry * EPT_PAGE_SIZE_2M);
            
            uint64_t flags;
            
            if (is_mmio_region(phys_addr)) {
                // MMIO: Uncacheable, Read+Write+Execute
                flags = EPT_RWX | (EPT_MT_UC << EPT_MEMORY_TYPE_SHIFT) | EPT_IGNORE_PAT;
                mmio_pages++;
            } else if (phys_addr == 0) {
                // First 2MB page: contains Real Mode IVT, BDA, etc.
                // Still WB, but could allow finer control later
                flags = EPT_RWX_WB;
            } else {
                // Normal RAM: Write-Back
                flags = EPT_RWX_WB;
            }
            
            pd[entry] = ept_make_2m_entry(phys_addr, flags);
            pages_mapped++;
        }
    }
    
    // --- Step 4: Assemble EPTP ---
    uint64_t eptp = ept_make_eptp(pml4_phys);
    
    EPT_LOG("--- EPT Statistics ---");
    EPT_LOG("  Mapped 2MB Pages:   %u", pages_mapped);
    EPT_LOG("  Thereof MMIO (UC):  %u", mmio_pages);
    EPT_LOG("  Thereof RAM (WB):   %u", pages_mapped - mmio_pages);
    EPT_LOG("  Total Memory:       %llu MB", 
           (unsigned long long)(pages_mapped * 2));
    EPT_LOG("  Tables allocated:   %u (PML4=1, PDPT=1, PDs=%u)",
           2 + num_gb, num_gb);
    EPT_LOG("  Memory required:    %u KB", (2 + num_gb) * 4);
    EPT_LOG("  EPTP:               0x%llX", (unsigned long long)eptp);
    
    // --- Step 5: Write EPTP into all active VMCS ---
    for (uint32_t cpu = 0; cpu < 256; cpu++) {
        if (hv->vcpus[cpu].vmcs_active) {
            // Ensure correct VMCS is loaded
            if (cosmos_vmptrld(&hv->vcpus[cpu].vmcs_phys) != 0) {
                EPT_LOG("WARNING: VMPTRLD failed for CPU %u", cpu);
                continue;
            }
            
            cosmos_vmwrite(VMCS_EPTP, eptp);
            EPT_LOG("CPU %u: EPTP written to VMCS", cpu);
        }
    }
    
    EPT_LOG("========================================");
    EPT_LOG("  ★ EPT IDENTITY MAP COMPLETE ★");
    EPT_LOG("========================================");
    
    return 0;
}

/* ============================================================================
 *  EPT PAGE FAULT HANDLER (for future dynamic expansion)
 *
 *  Called on EPT Violation VM-Exit.
 *  Currently logging only; future: dynamic mapping, Copy-on-Write, etc.
 * ============================================================================ */
int cosmos_hv_handle_ept_violation(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint64_t guest_phys = 0;
    uint64_t exit_qual = 0;
    uint64_t guest_rip = 0;
    
    cosmos_vmread(VMCS_EXIT_QUALIFICATION, &exit_qual);
    // Guest Physical Address is in separate VMCS field (VMCS_GUEST_PHYSICAL_ADDR = 0x2400)
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
    
    // TODO: Implement dynamic mapping
    // For now: avoid Triple Fault by mapping page with RWX
    // Unsafe, but useful for debugging
    
    return -1;  // Signals: unhandled
}

/* ============================================================================
 *  EPT MEMORY PROTECTION (for future stealth features)
 *
 *  Allows protecting specific guest memory areas:
 *  - Hide hypervisor code (Execute-only, no Read)
 *  - Detect guest kernel patches (Write-Protect)
 *  - Isolate GPU VRAM
 * ============================================================================ */

// Change access permissions of a single 2MB page in EPT
int cosmos_ept_set_page_flags(cosmos_hypervisor* hv, uint64_t guest_phys, uint64_t new_flags) {
    if (!hv || !hv->ept_pml4_virt) return -1;
    
    uint64_t* pml4 = (uint64_t*)hv->ept_pml4_virt;
    
    // PML4 Index
    uint64_t pml4_idx = ept_pml4_index(guest_phys);
    if (!(pml4[pml4_idx] & EPT_READ)) return -2;  // PML4 entry not present
    
    // PDPT
    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & EPT_ADDR_MASK);
    uint64_t pdpt_idx = ept_pdpt_index(guest_phys);
    if (!(pdpt[pdpt_idx] & EPT_READ)) return -3;  // PDPT entry not present
    
    // PD (contains our 2MB large pages)
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & EPT_ADDR_MASK);
    uint64_t pd_idx = ept_pd_index(guest_phys);
    
    // Retain physical address, change only flags
    uint64_t phys_addr = pd[pd_idx] & 0x000FFFFFFFE00000ULL;
    pd[pd_idx] = phys_addr | EPT_LARGE_PAGE | new_flags;
    
    // Invalidate TLB for this EPT translation
    extern void cosmos_invept(uint64_t type, uint64_t eptp);
    uint64_t eptp = ept_make_eptp(hv->ept_pml4_phys);
    cosmos_invept(1, eptp);  // Single-context invalidation
    
    return 0;
}

// Hide hypervisor memory from guest
// Makes pages Execute-Only (no Read, no Write) - guest can
// execute code, but cannot read or modify it
int cosmos_ept_hide_memory(cosmos_hypervisor* hv, uint64_t phys_start, uint64_t size) {
    EPT_LOG("Hiding memory: 0x%llX - 0x%llX (%llu KB)",
           (unsigned long long)phys_start,
           (unsigned long long)(phys_start + size),
           (unsigned long long)(size / 1024));
    
    // Align to 2MB boundaries
    uint64_t aligned_start = phys_start & ~(EPT_PAGE_SIZE_2M - 1);
    uint64_t aligned_end = (phys_start + size + EPT_PAGE_SIZE_2M - 1) & ~(EPT_PAGE_SIZE_2M - 1);
    
    for (uint64_t addr = aligned_start; addr < aligned_end; addr += EPT_PAGE_SIZE_2M) {
        // Execute-Only + WB (Guest cannot read what hypervisor does)
        uint64_t flags = EPT_EXECUTE | (EPT_MT_WB << EPT_MEMORY_TYPE_SHIFT) | EPT_IGNORE_PAT;
        int result = cosmos_ept_set_page_flags(hv, addr, flags);
        if (result != 0) {
            EPT_LOG("WARNING: Could not hide page 0x%llX (err=%d)",
                   (unsigned long long)addr, result);
        }
    }
    
    return 0;
}

/* ============================================================================
 *  EPT PAGE HIDING (Memory Stealth)
 *
 *  Hides a 4KB physical memory page from the guest OS.
 *  Splits a 2MB large page into 512 4KB pages when needed.
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
        // 2MB Page found! We must split it into 512 4KB pages.
        uint64_t pt_phys;
        pt = (uint64_t*)ept_alloc_page(&pt_phys);
        if (!pt) return -5; // Out of memory
        
        uint64_t base_phys = pd_entry & EPT_ADDR_MASK;
        uint64_t flags = pd_entry & 0xFFF;
        flags &= ~EPT_LARGE_PAGE; // Remove large page bit
        
        // Populate PT
        for (uint32_t i = 0; i < 512; i++) {
            pt[i] = ept_make_4k_entry(base_phys + (i * 4096), flags);
        }
        
        // Update PD entry
        pd[pd_idx] = ept_make_table_entry(pt_phys);
    } else {
        // Already a 4KB Page Table
        pt = (uint64_t*)cosmos_phys_to_virt(pd_entry & EPT_ADDR_MASK);
    }
    
    // Change PT entry for target_phys
    uint64_t pt_entry = pt[pt_idx];
    uint64_t flags = pt_entry & 0xFFF;
    
    // Map to dummy page!
    pt[pt_idx] = ept_make_4k_entry(dummy_phys, flags);
    
    // EPT TLB Flush is performed by caller (invept)
    EPT_LOG("Hiding page 0x%llX -> mapped to 0x%llX", 
            (unsigned long long)target_phys, (unsigned long long)dummy_phys);
            
    return 0;
}

/* ============================================================================
 *  HYPERVISOR MEMORY STEALTH (Full EPT Hiding)
 *
 *  Iterates across all CPUs and hides critical hypervisor memory pages
 *  (VMXON, VMCS, MSR bitmaps, host stacks, host CR3) from guest OS.
 *  All these pages are mapped to a dummy zero page for the guest.
 * ============================================================================ */
int cosmos_ept_hide_hv_memory(cosmos_hypervisor* hv) {
    if (!hv || !hv->ept_pml4_virt) return -1;
    
    // 1. Create a dummy page (zero filled)
    uint64_t dummy_phys = 0;
    void* dummy_virt = ept_alloc_page(&dummy_phys);
    if (!dummy_virt) return -2;
    
    EPT_LOG("Stealth dummy page allocated @ 0x%llX", (unsigned long long)dummy_phys);
    
    // 2. Hide pages across all CPUs
    for (uint32_t i = 0; i < 256; i++) {
        cosmos_vcpu* vcpu = &hv->vcpus[i];
        if (!vcpu->vmx_enabled) continue; // Skip inactive CPUs
        
        if (vcpu->vmxon_phys) cosmos_ept_hide_page(hv, vcpu->vmxon_phys, dummy_phys);
        if (vcpu->vmcs_phys) cosmos_ept_hide_page(hv, vcpu->vmcs_phys, dummy_phys);
        if (vcpu->msr_bitmap_phys) cosmos_ept_hide_page(hv, vcpu->msr_bitmap_phys, dummy_phys);
        if (vcpu->host_cr3_phys) cosmos_ept_hide_page(hv, vcpu->host_cr3_phys, dummy_phys);
        
        // Host stack is larger (HOST_STACK_SIZE = 16KB = 4 pages)
        if (vcpu->host_stack_phys) {
            for (int p = 0; p < 4; p++) {
                cosmos_ept_hide_page(hv, vcpu->host_stack_phys + (p * 4096), dummy_phys);
            }
        }
    }
    
    // 3. Hide EPT PML4 itself
    cosmos_ept_hide_page(hv, hv->ept_pml4_phys, dummy_phys);
    
    EPT_LOG("All hypervisor structures are now mapped to dummy pages in EPT!");
    return 0;
}
