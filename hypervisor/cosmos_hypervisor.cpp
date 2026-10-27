/*
 * ============================================================================
 *  COSMOS HYPERVISOR - Implementation
 * ============================================================================
 *
 *  Phase 1.1: VMX Detection & VMXON
 *  Phase 1.3: Stealth VM-Exit Handlers (CPUID, RDTSC, MSR)
 *
 *  This module implements the core of the CosmOS Type-1 Hypervisor.
 *  It can be compiled both bare-metal (CosmOS) and under Linux (for testing).
 * ============================================================================
 */

#include "cosmos_hypervisor.h"
#include "cosmos_anti_cheat.h"

/* ============================================================================
 *  MEMORY ALLOCATION ABSTRACTION
 *
 *  Under Linux: mmap() for page-aligned memory
 *  Bare Metal:  Custom Page Allocator from CosmOS
 * ============================================================================ */

#ifdef __linux__
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Under Linux we cannot directly use physical addresses.
// For testing we assume virtual == physical (works only as root with KVM).
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
// Bare Metal CosmOS - custom allocator
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

// Bare metal printf and memset (already present in CosmOS kernel)
extern "C" int printf(const char* fmt, ...);
extern "C" void* memset(void*, int, unsigned long);
extern "C" void* memcpy(void*, const void*, unsigned long);
#define HV_LOG(fmt, ...) printf("[COSMOS-HV] " fmt "\n", ##__VA_ARGS__)

#endif

/* ============================================================================
 *  HELPER: Adjust VMX control value
 *
 *  Intel VMX requires certain bits in control fields to be mandatory 0 or 1.
 *  The IA32_VMX_*_CTLS MSRs specify which bits are mandatory 0 and mandatory 1.
 *
 *  allowed0 (Low 32 bits):  Bit=1 → MUST be set
 *  allowed1 (High 32 bits): Bit=0 → MUST be cleared
 * ============================================================================ */
static uint32_t adjust_vmx_control(uint32_t desired, uint64_t msr_value) {
    uint32_t allowed0 = (uint32_t)(msr_value & 0xFFFFFFFF);       // Must-be-1
    uint32_t allowed1 = (uint32_t)((msr_value >> 32) & 0xFFFFFFFF); // Can-be-1
    
    desired |= allowed0;    // Set all mandatory-1 bits
    desired &= allowed1;    // Clear all mandatory-0 bits
    
    return desired;
}

/* ============================================================================
 *  Phase 1.1: VMX DETECTION
 *
 *  Checks if CPU supports VMX and reads all capability MSRs.
 * ============================================================================ */
int cosmos_hv_detect_vmx(cosmos_hypervisor* hv) {
    if (!hv) return -1;
    
    // IMPORTANT: Only clear capabilities and VCPUs, NOT stealth_mode/initialized
    // (these were already set by cosmos_hv_init)
    uint8_t saved_stealth = hv->stealth_mode;
    uint8_t saved_init = hv->initialized;
    memset(hv, 0, sizeof(cosmos_hypervisor));
    hv->stealth_mode = saved_stealth;
    hv->initialized = saved_init;
    
    HV_LOG("========================================");
    HV_LOG("  CosmOS Stealth Hypervisor v0.1");
    HV_LOG("  Phase 1.1: VMX Detection");
    HV_LOG("========================================");
    
    // --- Step 1: Check CPUID Leaf 1 ---
    uint32_t eax, ebx, ecx, edx;
    cosmos_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    if (!(ecx & CPUID_1_ECX_VMX)) {
        HV_LOG("ERROR: CPU does NOT support VMX (VT-x)!");
        HV_LOG("Please enable VT-x in BIOS.");
        return -1;
    }
    
    hv->has_vmx = 1;
    HV_LOG("[OK] VMX (VT-x) supported");
    
    // Warning if already running in a hypervisor
    if (ecx & CPUID_1_ECX_HYPERVISOR) {
        HV_LOG("[WARN] A hypervisor is already active!");
        HV_LOG("[WARN] Nested virtualization required.");
    }
    
    // --- Step 2: Check IA32_FEATURE_CONTROL MSR ---
    uint64_t feature_control = cosmos_rdmsr(IA32_FEATURE_CONTROL);
    HV_LOG("IA32_FEATURE_CONTROL = 0x%llX", (unsigned long long)feature_control);
    
    if (feature_control & FEATURE_CONTROL_LOCKED) {
        if (!(feature_control & FEATURE_CONTROL_VMXON_OUTSIDE)) {
            HV_LOG("ERROR: VMXON is locked in BIOS!");
            HV_LOG("Please enable 'Intel Virtualization Technology' in BIOS.");
            return -2;
        }
        HV_LOG("[OK] VMXON enabled outside SMX");
    } else {
        // MSR is not locked - we can configure it ourselves
        feature_control |= FEATURE_CONTROL_VMXON_OUTSIDE | FEATURE_CONTROL_LOCKED;
        cosmos_wrmsr(IA32_FEATURE_CONTROL, feature_control);
        HV_LOG("[OK] VMXON enabled and MSR locked");
    }
    
    // --- Step 3: Read VMX Capability MSRs ---
    uint64_t vmx_basic = cosmos_rdmsr(IA32_VMX_BASIC);
    hv->vmcs_revision_id = (uint32_t)(vmx_basic & 0x7FFFFFFF);
    hv->vmcs_region_size = (uint32_t)((vmx_basic >> 32) & 0x1FFF);
    hv->has_true_msrs     = (vmx_basic & (1ULL << 55)) ? 1 : 0;
    
    HV_LOG("VMCS Revision ID: 0x%08X", hv->vmcs_revision_id);
    HV_LOG("VMCS Region Size: %u bytes", hv->vmcs_region_size);
    HV_LOG("True VMX MSRs:    %s", hv->has_true_msrs ? "JA" : "NEIN");
    
    // Memory type check: Write-Back must be supported
    uint8_t mem_type = (uint8_t)((vmx_basic >> 50) & 0xF);
    HV_LOG("VMCS Memory Type: %u (%s)", mem_type, mem_type == 6 ? "Write-Back [OK]" : "Other");
    
    // --- Step 4: Read Control-Capability MSRs ---
    // Use "True" MSRs if available (finer control)
    uint32_t pin_msr   = hv->has_true_msrs ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS;
    uint32_t proc_msr  = hv->has_true_msrs ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS;
    uint32_t exit_msr  = hv->has_true_msrs ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS;
    uint32_t entry_msr = hv->has_true_msrs ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS;
    
    hv->vmx_pin_based_allowed0   = cosmos_rdmsr(pin_msr);
    hv->vmx_proc_based_allowed0  = cosmos_rdmsr(proc_msr);
    hv->vmx_exit_allowed0        = cosmos_rdmsr(exit_msr);
    hv->vmx_entry_allowed0       = cosmos_rdmsr(entry_msr);
    
    // Secondary Proc-Based Controls (check if available)
    if (hv->vmx_proc_based_allowed0 & ((uint64_t)PROCBASED_ACTIVATE_SECONDARY << 32)) {
        hv->vmx_proc_based2_allowed0 = cosmos_rdmsr(IA32_VMX_PROCBASED_CTLS2);
        HV_LOG("[OK] Secondary Proc-Based Controls available");
    }
    
    // --- Step 5: EPT/VPID Capabilities ---
    if (hv->vmx_proc_based2_allowed0 & ((uint64_t)PROCBASED2_ENABLE_EPT << 32)) {
        hv->has_ept = 1;
        hv->ept_vpid_cap = cosmos_rdmsr(IA32_VMX_EPT_VPID_CAP);
        HV_LOG("[OK] EPT (Extended Page Tables) supported!");
        
        // Check 2MB Large Pages support
        if (hv->ept_vpid_cap & (1ULL << 16)) {
            HV_LOG("[OK] EPT 2MB Large Pages supported (optimal for Identity Map)");
        }
        // Check 1GB Huge Pages support
        if (hv->ept_vpid_cap & (1ULL << 17)) {
            HV_LOG("[OK] EPT 1GB Huge Pages supported");
        }
        // Check EPT INVEPT support
        if (hv->ept_vpid_cap & (1ULL << 20)) {
            HV_LOG("[OK] EPT INVEPT supported");
        }
    } else {
        HV_LOG("WARNING: Without EPT, stealth virtualization is not possible!");
    }
    
    HV_LOG("========================================");
    HV_LOG("  VMX Detection: SUCCESSFUL");
    HV_LOG("========================================");
    
    return 0;
}

/* ============================================================================
 *  Phase 1.1: VMXON - Enable VMX Root Operation
 *
 *  Enables VMX on a specific CPU.
 *  Must be called on EACH CPU individually (via IPI or SMP init).
 * ============================================================================ */
#define HOST_STACK_SIZE 16384 // 16KB Host Stack per Core

int cosmos_hv_enable_vmx(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return -1;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    vcpu->cpu_id = cpu_id;
    
    HV_LOG("CPU %u: Enabling VMX...", cpu_id);
    
    // --- Step 1: Set CR4.VMXE ---
    uint64_t cr4 = cosmos_read_cr4();
    cr4 |= CR4_VMXE;
    cosmos_write_cr4(cr4);
    
    // Verify CR4.VMXE was set
    if (!(cosmos_read_cr4() & CR4_VMXE)) {
        HV_LOG("CPU %u: FEHLER: CR4.VMXE konnte nicht gesetzt werden!", cpu_id);
        return -1;
    }
    
    // --- Step 2: Adjust CR0 to VMX requirements ---
    uint64_t cr0 = cosmos_read_cr0();
    uint64_t cr0_fixed0 = cosmos_rdmsr(IA32_VMX_CR0_FIXED0);
    uint64_t cr0_fixed1 = cosmos_rdmsr(IA32_VMX_CR0_FIXED1);
    cr0 |= cr0_fixed0;   // Bits that must be 1
    cr0 &= cr0_fixed1;   // Bits that must be 0
    cosmos_write_cr0(cr0);
    
    // Adjust CR4 to VMX requirements as well
    uint64_t cr4_fixed0 = cosmos_rdmsr(IA32_VMX_CR4_FIXED0);
    uint64_t cr4_fixed1 = cosmos_rdmsr(IA32_VMX_CR4_FIXED1);
    cr4 |= cr4_fixed0;
    cr4 &= cr4_fixed1;
    cosmos_write_cr4(cr4);
    
    // --- Step 3: Allocate VMXON Region ---
    vcpu->vmxon_region = (vmx_region*)hv_alloc_page(&vcpu->vmxon_phys);
    if (!vcpu->vmxon_region) {
        HV_LOG("CPU %u: ERROR: Could not allocate VMXON region!", cpu_id);
        return -2;
    }
    
    // Write Revision ID into VMXON region (MANDATORY!)
    vcpu->vmxon_region->revision_id = hv->vmcs_revision_id;
    
    HV_LOG("CPU %u: VMXON Region @ phys=0x%llX (rev=0x%X)", 
           cpu_id, 
           (unsigned long long)vcpu->vmxon_phys, 
           vcpu->vmxon_region->revision_id);
    
    // --- Step 4: Execute VMXON! ---
    uint8_t result = cosmos_vmxon(&vcpu->vmxon_phys);
    if (result != 0) {
        HV_LOG("CPU %u: ERROR: VMXON failed! (result=%u)", cpu_id, result);
        hv_free_page(vcpu->vmxon_region);
        vcpu->vmxon_region = nullptr;
        return -3;
    }
    
    vcpu->vmx_enabled = 1;
    HV_LOG("CPU %u: ★ VMXON SUCCESSFUL! VMX Root Operation active! ★", cpu_id);
    
    // --- Step 5: Allocate and initialize VMCS Region ---
    vcpu->vmcs_region = (vmx_region*)hv_alloc_page(&vcpu->vmcs_phys);
    if (!vcpu->vmcs_region) {
        HV_LOG("CPU %u: ERROR: Could not allocate VMCS region!", cpu_id);
        cosmos_hv_disable_vmx(hv, cpu_id);
        return -4;
    }
    
    vcpu->vmcs_region->revision_id = hv->vmcs_revision_id;
    
    HV_LOG("CPU %u: VMCS Region  @ phys=0x%llX (rev=0x%X)",
           cpu_id,
           (unsigned long long)vcpu->vmcs_phys,
           vcpu->vmcs_region->revision_id);
    
    // VMCLEAR: Clears VMCS and sets state to Clear
    result = cosmos_vmclear(&vcpu->vmcs_phys);
    if (result != 0) {
        HV_LOG("CPU %u: ERROR: VMCLEAR failed!", cpu_id);
        return -5;
    }
    
    // VMPTRLD: Load VMCS as active VMCS
    result = cosmos_vmptrld(&vcpu->vmcs_phys);
    if (result != 0) {
        HV_LOG("CPU %u: ERROR: VMPTRLD failed!", cpu_id);
        return -6;
    }
    
    vcpu->vmcs_active = 1;
    HV_LOG("CPU %u: VMCS loaded and active", cpu_id);
    
    // --- Step 6: Allocate MSR Bitmap ---
    vcpu->msr_bitmap = (uint8_t*)hv_alloc_page(&vcpu->msr_bitmap_phys);
    if (!vcpu->msr_bitmap) {
        HV_LOG("CPU %u: WARNING: Could not allocate MSR bitmap", cpu_id);
    } else {
        memset(vcpu->msr_bitmap, 0, 4096);
        // Specifically target only the MSRs we want to intercept
        // Set intercept bits for VMX MSRs
        extern void cosmos_ac_init_msr_bitmap(uint8_t* msr_bitmap);
        cosmos_ac_init_msr_bitmap(vcpu->msr_bitmap);
        HV_LOG("CPU %u: MSR bitmap initialized (VMX intercepts active)", cpu_id);
    }
    
    // --- Step 7: Allocate Host Stack (16KB for safe VM-Exit processing) ---
    // 4KB is NOT enough — printk/vprintk use deep callchains requiring >4KB stack
    vcpu->host_stack = (uint8_t*)hv_alloc_pages(HOST_STACK_SIZE, &vcpu->host_stack_phys);
    if (!vcpu->host_stack) {
        HV_LOG("CPU %u: ERROR: Could not allocate host stack!", cpu_id);
        cosmos_hv_disable_vmx(hv, cpu_id);
        return -7;
    }
    memset(vcpu->host_stack, 0, HOST_STACK_SIZE);
    
    HV_LOG("CPU %u: Host Stack @ phys=0x%llX (size: %d bytes)",
           cpu_id, (unsigned long long)vcpu->host_stack_phys, HOST_STACK_SIZE);
    
    HV_LOG("CPU %u: ★ VMX FULLY INITIALIZED ★", cpu_id);
    return 0;
}

/* ============================================================================
 *  Phase 1.1: VMXOFF - Disable VMX
 * ============================================================================ */
void cosmos_hv_disable_vmx(cosmos_hypervisor* hv, uint32_t cpu_id) {
    if (!hv || cpu_id >= 256) return;
    
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    
    if (!vcpu->vmx_enabled) {
        HV_LOG("CPU %u: VMX was not active", cpu_id);
        return;
    }
    
    HV_LOG("CPU %u: Disabling VMX...", cpu_id);
    
    // Clear active VMCS
    if (vcpu->vmcs_active) {
        cosmos_vmclear(&vcpu->vmcs_phys);
        vcpu->vmcs_active = 0;
    }
    
    // Execute VMXOFF
    cosmos_vmxoff();
    vcpu->vmx_enabled = 0;
    
    // Clear CR4.VMXE
    uint64_t cr4 = cosmos_read_cr4();
    cr4 &= ~CR4_VMXE;
    cosmos_write_cr4(cr4);
    
    // Free resources
    if (vcpu->vmcs_region) {
        hv_free_page(vcpu->vmcs_region);
        vcpu->vmcs_region = nullptr;
    }
    if (vcpu->vmxon_region) {
        hv_free_page(vcpu->vmxon_region);
        vcpu->vmxon_region = nullptr;
    }
    if (vcpu->msr_bitmap) {
        hv_free_page(vcpu->msr_bitmap);
        vcpu->msr_bitmap = nullptr;
    }
    if (vcpu->host_stack) {
        // Free host stack
        vcpu->host_stack = nullptr;
    }
    
    HV_LOG("CPU %u: VMX disabled and resources freed", cpu_id);
}

/* ============================================================================
 *  Phase 1.2: VMCS SETUP
 *
 *  Configures all VMCS fields for the guest.
 *  Sets VM-Execution Controls, Guest State, and Host State.
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
        HV_LOG("CPU %u: VMCS is not active!", cpu_id);
        return -1;
    }
    
    HV_LOG("CPU %u: Configuring VMCS...", cpu_id);
    
    // ===== VM-EXECUTION CONTROLS =====
    
    // Pin-Based Controls (minimal)
    uint32_t pin_based = 0;
    pin_based = adjust_vmx_control(pin_based, hv->vmx_pin_based_allowed0);
    cosmos_vmwrite(VMCS_PIN_BASED_EXEC_CTRL, pin_based);
    
    // Primary Proc-Based Controls
    uint32_t proc_based = 0;
    proc_based |= PROCBASED_USE_MSR_BITMAPS;       // Use MSR bitmap
    proc_based |= PROCBASED_ACTIVATE_SECONDARY;     // Activate secondary controls
    proc_based |= PROCBASED_USE_TSC_OFFSET;         // TSC offset for stealth
    // NO PROCBASED_RDTSC_EXIT → TSC offset is sufficient for stealth
    // CPUID ALWAYS causes VM-Exit (no control bit required)
    proc_based = adjust_vmx_control(proc_based, hv->vmx_proc_based_allowed0);
    cosmos_vmwrite(VMCS_PROC_BASED_EXEC_CTRL, proc_based);
    
    // Secondary Proc-Based Controls
    uint32_t proc_based2 = 0;
    if (hv->has_ept)                proc_based2 |= PROCBASED2_ENABLE_EPT;
    if (hv->has_vpid)               proc_based2 |= PROCBASED2_ENABLE_VPID;
    if (hv->has_unrestricted_guest) proc_based2 |= PROCBASED2_UNRESTRICTED_GUEST;
    proc_based2 |= PROCBASED2_ENABLE_RDTSCP;       // Allow RDTSCP
    proc_based2 |= PROCBASED2_ENABLE_INVPCID;      // Allow INVPCID
    proc_based2 |= PROCBASED2_ENABLE_XSAVES;       // Allow XSAVES/XRSTORS
    proc_based2 = adjust_vmx_control(proc_based2, hv->vmx_proc_based2_allowed0);
    cosmos_vmwrite(VMCS_PROC_BASED_EXEC_CTRL2, proc_based2);
    
    // VM-Exit Controls
    uint32_t exit_controls = 0;
    exit_controls |= VMEXIT_CTRL_HOST_ADDR_SPACE;   // Host is 64-bit!
    exit_controls |= VMEXIT_CTRL_SAVE_EFER;
    exit_controls |= VMEXIT_CTRL_LOAD_EFER;
    exit_controls |= VMEXIT_CTRL_ACK_INT_ON_EXIT;
    exit_controls = adjust_vmx_control(exit_controls, hv->vmx_exit_allowed0);
    cosmos_vmwrite(VMCS_EXIT_CONTROLS, exit_controls);
    
    // VM-Entry Controls
    uint32_t entry_controls = 0;
    entry_controls |= VMENTRY_CTRL_GUEST_IA32E;     // Guest is 64-bit!
    entry_controls |= VMENTRY_CTRL_LOAD_EFER;
    entry_controls = adjust_vmx_control(entry_controls, hv->vmx_entry_allowed0);
    cosmos_vmwrite(VMCS_ENTRY_CONTROLS, entry_controls);
    
    // Exception Bitmap: Do not intercept exceptions (pass all through)
    cosmos_vmwrite(VMCS_EXCEPTION_BITMAP, 0);
    
    // Set MSR Bitmap
    if (vcpu->msr_bitmap) {
        cosmos_vmwrite(VMCS_MSR_BITMAP, vcpu->msr_bitmap_phys);
    }
    
    // TSC Offset (Stealth: compensates for VM-Exit overhead)
    cosmos_vmwrite(VMCS_TSC_OFFSET, (uint64_t)vcpu->tsc_offset);
    
    // VPID (if available)
    if (hv->has_vpid) {
        cosmos_vmwrite(VMCS_VPID, cpu_id + 1);  // VPID 0 is reserved
    }
    
    // VMCS Link Pointer (must be 0xFFFFFFFF_FFFFFFFF if not shadow VMCS)
    cosmos_vmwrite(VMCS_GUEST_VMCS_LINK_PTR, 0xFFFFFFFFFFFFFFFFULL);
    
    // Set EPT Pointer (EPTP)
    if (hv->has_ept) {
        // EPTP = PML4 | (Walk Length 4 - 1)<<3 | Write-Back (6)
        uint64_t eptp = hv->ept_pml4_phys | (3ULL << 3) | 6ULL;
        cosmos_vmwrite(VMCS_EPTP, eptp);
    }
    
    // ===== HOST STATE =====
    // Host state defines where the CPU jumps upon a VM-Exit.
    
    // Host Segment Selectors
    cosmos_vmwrite(VMCS_HOST_CS_SEL, cosmos_read_cs());
    cosmos_vmwrite(VMCS_HOST_SS_SEL, cosmos_read_ss());
    cosmos_vmwrite(VMCS_HOST_DS_SEL, cosmos_read_ds());
    cosmos_vmwrite(VMCS_HOST_ES_SEL, cosmos_read_es());
    cosmos_vmwrite(VMCS_HOST_FS_SEL, cosmos_read_fs());
    cosmos_vmwrite(VMCS_HOST_GS_SEL, cosmos_read_gs());
    cosmos_vmwrite(VMCS_HOST_TR_SEL, cosmos_read_tr());
    
    // --- Robust Host CR3 ---
    // If we use `cosmos_read_cr3()`, we take the CR3 of the process currently
    // running on the CPU (e.g. insmod). When that process terminates,
    // its CR3 is freed, and the next VM-Exit triggers a Triple Fault!
    // Solution: We copy the upper 256 entries (kernel space) of the current CR3 into
    // a NEW page directory that stays permanently in memory.
    vcpu->host_cr3_region = cosmos_alloc_page_aligned(&vcpu->host_cr3_phys);
    memset(vcpu->host_cr3_region, 0, 4096);
    // Copy kernel space (upper half of PML4, 2048 bytes from offset 2048)
    memcpy((uint8_t*)vcpu->host_cr3_region + 2048, (uint8_t*)cosmos_get_current_pml4() + 2048, 2048);
    
    // Host Control Registers
    cosmos_vmwrite(VMCS_HOST_CR0, cosmos_read_cr0());
    cosmos_vmwrite(VMCS_HOST_CR3, vcpu->host_cr3_phys); // Use our safe CR3
    cosmos_vmwrite(VMCS_HOST_CR4, cosmos_read_cr4());
    
    // Stealth: Hide VMXE from guest
    // Guest reads CR4.VMXE as 0. Writes to VMXE trigger a VM-Exit.
    cosmos_vmwrite(VMCS_CR4_GUEST_HOST_MASK, CR4_VMXE);
    cosmos_vmwrite(VMCS_CR4_READ_SHADOW, cosmos_read_cr4() & ~CR4_VMXE);
    
    // Host GDT/IDT
    desc_table_reg gdtr, idtr;
    cosmos_sgdt(&gdtr);
    cosmos_sidt(&idtr);
    cosmos_vmwrite(VMCS_HOST_GDTR_BASE, gdtr.base);
    cosmos_vmwrite(VMCS_HOST_IDTR_BASE, idtr.base);
    
    // Compute Host TR Base from GDT
    uint64_t host_tr_base = cosmos_get_segment_base(cosmos_read_tr(), gdtr.base);
    cosmos_vmwrite(VMCS_HOST_TR_BASE, host_tr_base);
    
    // Host MSRs
    cosmos_vmwrite(VMCS_HOST_EFER, cosmos_rdmsr(IA32_EFER));
    
    // Host FS/GS Base
    cosmos_vmwrite(VMCS_HOST_FS_BASE, cosmos_rdmsr(IA32_FS_BASE));
    cosmos_vmwrite(VMCS_HOST_GS_BASE, cosmos_rdmsr(IA32_GS_BASE));
    
    // Host SYSENTER (for compatibility)
    cosmos_vmwrite(VMCS_HOST_SYSENTER_ESP, cosmos_rdmsr(IA32_SYSENTER_ESP));
    cosmos_vmwrite(VMCS_HOST_SYSENTER_EIP, cosmos_rdmsr(IA32_SYSENTER_EIP));
    
    // Host RSP and RIP (VM-Exit Entry Point!)
    // RSP: Top of host stack (stack grows downwards!)
    // 16-byte alignment is important for System V ABI
    cosmos_vmwrite(VMCS_HOST_RSP, (uint64_t)(vcpu->host_stack + HOST_STACK_SIZE));
    
    // RIP: cosmos_vmexit_stub (defined in cosmos_vmx.S)
    cosmos_vmwrite(VMCS_HOST_RIP, (uint64_t)&cosmos_vmexit_stub);
    
    HV_LOG("CPU %u: Host RIP = 0x%llX (vmexit_stub)", cpu_id,
           (unsigned long long)&cosmos_vmexit_stub);
    HV_LOG("CPU %u: Host RSP = 0x%llX (stack size: %d bytes)", cpu_id,
           (unsigned long long)(vcpu->host_stack + HOST_STACK_SIZE), HOST_STACK_SIZE);
    
    // ===== GUEST STATE =====
    // Initial guest state (adapted later for Windows)
    
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
    
    // Guest TR and LDTR Base (adopt host values)
    cosmos_vmwrite(VMCS_GUEST_TR_BASE, host_tr_base);
    cosmos_vmwrite(VMCS_GUEST_LDTR_BASE, 0);
    
    // Guest Control Registers
    cosmos_vmwrite(VMCS_GUEST_CR0, cosmos_read_cr0());
    cosmos_vmwrite(VMCS_GUEST_CR3, cosmos_read_cr3());
    cosmos_vmwrite(VMCS_GUEST_CR4, cosmos_read_cr4());
    
    // Guest DR7 (Debug Registers)
    cosmos_vmwrite(VMCS_GUEST_DR7, 0x400);  // Default value
    
    // Guest RFLAGS
    cosmos_vmwrite(VMCS_GUEST_RFLAGS, cosmos_read_rflags() | 0x2);  // Bit 1 must always be 1
    
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
    
    // Guest RIP and RSP are set LATER when guest is launched
    // cosmos_vmwrite(VMCS_GUEST_RIP, guest_entry_point);
    // cosmos_vmwrite(VMCS_GUEST_RSP, guest_stack_pointer);
    
    // Configure Anti-Cheat Hardening (Stealth Mode)
    if (hv->stealth_mode) {
        cosmos_ac_configure_vmcs_stealth(hv, vcpu);
    }
    
    HV_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    HV_LOG("CPU %u: ★ VMCS FULLY CONFIGURED ★", cpu_id);
    HV_LOG("CPU %u: ═══════════════════════════════════", cpu_id);
    
    return 0;
}

/* ============================================================================
 *  Phase 1.3: STEALTH VM-EXIT HANDLERS
 *
 *  These handlers make the hypervisor invisible to anti-cheat software.
 * ============================================================================ */

/* --- CPUID Stealth Handler ---
 * 
 * Anti-Cheat checks:
 *   CPUID.1:ECX[31] = Hypervisor Present Bit
 *   CPUID.0x40000000 = Hypervisor Vendor String
 *   CPUID.0x40000001+ = Hypervisor Features
 *
 * We spoof ALL of these responses!
 */
void cosmos_hv_handle_cpuid(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint32_t leaf    = (uint32_t)vcpu->guest_regs.rax;
    uint32_t subleaf = (uint32_t)vcpu->guest_regs.rcx;
    
    uint32_t eax, ebx, ecx, edx;
    
    // Execute real CPUID (we are in host/root mode)
    cosmos_cpuid(leaf, subleaf, &eax, &ebx, &ecx, &edx);
    
    switch (leaf) {
        case 0x01:
            // ★ STEALTH: Clear Hypervisor Present Bit! ★
            ecx &= ~CPUID_1_ECX_HYPERVISOR;
            
            // Optional: Also clear VMX bit (so guest doesn't attempt nested virtualization)
            if (hv->stealth_mode) {
                ecx &= ~CPUID_1_ECX_VMX;
            }
            break;
            
        case 0x40000000:
            // ★ STEALTH: Return no hypervisor signature! ★
            // Normally a hypervisor returns its name here
            // (e.g. "KVMKVMKVM\0\0\0" or "Microsoft Hv").
            // We return 0 = "no hypervisor present".
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
            // ★ STEALTH: Set all hypervisor info leaves to 0 ★
            eax = 0;
            ebx = 0;
            ecx = 0;
            edx = 0;
            break;
            
        default:
            // All other CPUID leaves: pass through real values
            break;
    }
    
    // Write results to guest registers
    vcpu->guest_regs.rax = eax;
    vcpu->guest_regs.rbx = ebx;
    vcpu->guest_regs.rcx = ecx;
    vcpu->guest_regs.rdx = edx;
}

/* --- RDTSC Stealth Handler ---
 *
 * Anti-Cheat measures the execution duration of certain instructions.
 * A VM-Exit costs ~1000-3000 extra clock cycles.
 * If we do not compensate TSC, anti-cheat detects the time gap
 * and discovers the hypervisor.
 *
 * Solution: Adjust TSC offset to hide VM-Exit overhead.
 */
void cosmos_hv_handle_rdtsc(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint64_t real_tsc = cosmos_rdtsc();
    
    // Compensate TSC by VM-Exit overhead
    // The offset is dynamically adjusted based on measured VM-Exit latency
    real_tsc += vcpu->tsc_offset;
    
    vcpu->guest_regs.rax = (uint32_t)(real_tsc & 0xFFFFFFFF);
    vcpu->guest_regs.rdx = (uint32_t)(real_tsc >> 32);
}

/* --- RDMSR Stealth Handler ---
 *
 * Anti-Cheat reads certain MSRs to search for hypervisors:
 * - IA32_VMX_BASIC: Exists only when VMX is active
 * - IA32_DEBUGCTL: Can reveal breakpoints
 * - IA32_TSC_AUX: Processor ID
 */
void cosmos_hv_handle_rdmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    uint32_t msr = (uint32_t)vcpu->guest_regs.rcx;
    uint64_t value = 0;
    
    switch (msr) {
        // ★ STEALTH: Hide VMX-related MSRs ★
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
                // Inject General Protection Fault (#GP)
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
                // Handled by anti-cheat module
                break;
            }
            // All other MSRs: pass through real value
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
        // VMX MSRs: ignore in stealth mode
        case IA32_VMX_BASIC:
        case IA32_VMX_PINBASED_CTLS:
        case IA32_VMX_PROCBASED_CTLS:
            if (hv->stealth_mode) {
                // Inject General Protection Fault (#GP)
                cosmos_vmwrite(VMCS_ENTRY_INT_INFO, (1U << 31) | (1U << 11) | (3U << 8) | 13U);
                cosmos_vmwrite(VMCS_ENTRY_EXCEPTION_ERRCODE, 0);
                return;
            }
            break;
            
        default:
            // Pass through
            cosmos_wrmsr(msr, value);
            break;
    }
}

/* ============================================================================
 *  VM-EXIT HANDLER (Main Dispatcher)
 *
 *  Called by cosmos_vmexit_stub (Assembly).
 *  Reads exit reason and dispatches to appropriate handler.
 * ============================================================================ */

// Global Hypervisor State (Singleton)
static cosmos_hypervisor g_hypervisor;

// Accessor for global hypervisor
cosmos_hypervisor* cosmos_hv_get_global(void) {
    return &g_hypervisor;
}

extern "C" int cosmos_vmexit_handler(vmx_guest_regs* guest_regs) {
    cosmos_hypervisor* hv = &g_hypervisor;
    
    // Retrieve correct CPU ID as we are multicore capable!
    // Linux GS-Base pointer is active in guest and vmexit_stub
    // runs on regular Linux stack, so smp_processor_id() is safe!
    extern unsigned int cosmos_get_cpu_id(void);
    uint32_t cpu_id = cosmos_get_cpu_id();
    cosmos_vcpu* vcpu = &hv->vcpus[cpu_id];
    
    // Copy guest registers
    vcpu->guest_regs = *guest_regs;
    
    // Capture TSC at VM-Exit (for stealth compensation)
    uint64_t exit_tsc = cosmos_rdtsc();
    
    // Read exit reason from VMCS
    uint64_t exit_reason_raw = 0;
    cosmos_vmread(VMCS_EXIT_REASON, &exit_reason_raw);
    uint32_t exit_reason = (uint32_t)(exit_reason_raw & 0xFFFF);
    
    // Exit Qualification (additional info)
    uint64_t exit_qualification = 0;
    cosmos_vmread(VMCS_EXIT_QUALIFICATION, &exit_qualification);
    
    // Guest RIP for instruction length
    uint64_t guest_rip = 0;
    uint64_t instr_len = 0;
    cosmos_vmread(VMCS_GUEST_RIP, &guest_rip);
    cosmos_vmread(VMCS_EXIT_INSTR_LENGTH, &instr_len);
    
    int action = 0; // 0 = resume, 1 = abort
    switch (exit_reason) {
        case VMEXIT_CPUID:
            cosmos_hv_handle_cpuid(hv, vcpu);
            // Advance RIP (CPUID is 2 bytes: 0F A2)
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            

        case VMEXIT_RDTSC:
            cosmos_hv_handle_rdtsc(hv, vcpu);
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_RDTSCP:
            cosmos_hv_handle_rdtsc(hv, vcpu);
            // RDTSCP also sets ECX = IA32_TSC_AUX
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
            // HLT: Guest wants to halt → allow continuation
            cosmos_vmwrite(VMCS_GUEST_RIP, guest_rip + instr_len);
            break;
            
        case VMEXIT_VMCALL:
            // Hypercall from CosmOS (for internal communication)
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
            // If guest executes VMX instructions, inject #UD (Invalid Opcode)
            cosmos_vmwrite(VMCS_ENTRY_INT_INFO, (1U << 31) | (3U << 8) | 6U); 
            // RIP is NOT advanced on exceptions!
            break;
            
        case VMEXIT_TRIPLE_FAULT:
            HV_LOG("!!! TRIPLE FAULT in guest! RIP=0x%llX !!!", 
                   (unsigned long long)guest_rip);
            action = exit_reason | 0x10000;
            break;
            
        default:
            HV_LOG("Unhandled VM-Exit: reason=%u, qual=0x%llX, RIP=0x%llX. ABORT!",
                   exit_reason,
                   (unsigned long long)exit_qualification,
                   (unsigned long long)guest_rip);
            action = exit_reason | 0x10000;
            break;
    }
    
    // TSC Stealth: Hide time spent in VM-Exit
    if (action == 0) {
        uint64_t new_exit_tsc = cosmos_rdtsc();
        if (hv->stealth_mode) {
            // Standard cost for VM-Exits
            uint64_t simulated_cost = (exit_reason == VMEXIT_CPUID) ? VMEXIT_COST_CYCLES_CPUID : VMEXIT_COST_CYCLES_RDMSR;
            cosmos_ac_hide_vmexit_time(vcpu, exit_tsc, new_exit_tsc, simulated_cost);
        } else {
            // Old TSC stealth (naive)
            uint64_t exit_cost = new_exit_tsc - exit_tsc;
            vcpu->tsc_offset -= (int64_t)exit_cost;
            cosmos_vmwrite(VMCS_TSC_OFFSET, (uint64_t)vcpu->tsc_offset);
        }
    }
    
    // Write back registers (restored by assembly stub)
    *guest_regs = vcpu->guest_regs;
    
    return action;
}
