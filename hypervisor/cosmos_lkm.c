#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/io.h>
#include <asm/io.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CosmOS");
MODULE_DESCRIPTION("CosmOS Stealth Hypervisor LKM");
MODULE_VERSION("1.0");

// --- Declarations of C++ Hypervisor functions ---
// These are linked from cosmos_hypervisor_all.o (baremetal mode).
struct cosmos_hypervisor;
extern struct cosmos_hypervisor* _Z20cosmos_hv_get_globalv(void);
extern int _Z14cosmos_hv_initP17cosmos_hypervisor(struct cosmos_hypervisor* hv);
extern int _Z20cosmos_hv_detect_vmxP17cosmos_hypervisor(struct cosmos_hypervisor* hv);
extern int _Z19cosmos_hv_setup_eptP17cosmos_hypervisor(struct cosmos_hypervisor* hv);
extern int _Z20cosmos_hv_enable_vmxP17cosmos_hypervisorj(struct cosmos_hypervisor* hv, unsigned int cpu_id);
extern int _Z20cosmos_hv_setup_vmcsP17cosmos_hypervisorj(struct cosmos_hypervisor* hv, unsigned int cpu_id);
extern int _Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj(struct cosmos_hypervisor* hv, unsigned int cpu_id);
extern int _Z27cosmos_hv_launch_test_guestP17cosmos_hypervisorj(struct cosmos_hypervisor* hv, unsigned int cpu_id);
extern void _Z18cosmos_hv_shutdownP17cosmos_hypervisor(struct cosmos_hypervisor* hv);

// --- Provision of "Bare-Metal" dependencies for the hypervisor ---

void* cosmos_alloc_page_aligned(uint64_t* out_phys);
int printf(const char* fmt, ...);

void* cosmos_alloc_page_aligned(uint64_t* out_phys) {
    // Fetch a 4KB page initialized with zeros
    unsigned long page = __get_free_page(GFP_KERNEL | __GFP_ZERO);
    if (!page) {
        printk(KERN_ERR "CosmOS-HV: alloc_page failed!\n");
        return NULL;
    }
    
    // Calculate physical address for VMCS/VMXON
    *out_phys = virt_to_phys((void*)page);
    return (void*)page;
}

// C++ Mangled Wrapper
void* _Z25cosmos_alloc_page_alignedPm(uint64_t* out_phys) {
    return cosmos_alloc_page_aligned(out_phys);
}

void* _Z26cosmos_alloc_pages_alignedmPm(unsigned long bytes, uint64_t* out_phys) {
    // For kernel testing we allocate contiguous pages
    int order = get_order(bytes);
    unsigned long page = __get_free_pages(GFP_KERNEL | __GFP_ZERO, order);
    if (!page) return NULL;
    *out_phys = virt_to_phys((void*)page);
    return (void*)page;
}

// C Wrapper for CPU ID (called via extern "C")
unsigned int cosmos_get_cpu_id(void) {
    return smp_processor_id();
}

// C Wrapper: Fetch virtual address of current Page Directory (PML4)
void* cosmos_get_current_pml4(void) {
    unsigned long cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    // Mask out bits 0-11 (flags & PCID) to obtain physical address
    cr3 &= ~0xFFFUL;
    // Convert physical address to Kernel Logical Address (virtual)
    return phys_to_virt(cr3);
}

// C Wrapper: Convert physical address to virtual kernel address
void* cosmos_phys_to_virt(uint64_t phys_addr) {
    return phys_to_virt(phys_addr);
}

void _Z16cosmos_free_pagePv(void* ptr) {
    if (ptr) free_page((unsigned long)ptr);
}

int _Z20cosmos_vfs_read_filePKcPPvPm(const char* path, void** out_buf, unsigned long* out_size) {
    // Dummy implementation since kernel does not read files directly this way
    printk(KERN_ERR "CosmOS-HV: VFS Read not implemented in LKM (%s)\n", path);
    return -1;
}

// cosmos_invept is defined in cosmos_vmx.S, declared as extern C
// If missing, define stub here or invoke it
extern void cosmos_invept(unsigned long type, unsigned long desc);
void _Z13cosmos_inveptmm(unsigned long type, unsigned long desc) {
    cosmos_invept(type, desc);
}

// 2. Redirect printf to printk
int printf(const char* fmt, ...) {
    va_list args;
    int r;
    va_start(args, fmt);
    // Call kernel internal vprintk
    r = vprintk(fmt, args);
    va_end(args);
    return r;
}

// memset and snprintf are automatically resolved by kernel
// since they exist in kernel-lib (lib/string.c and lib/vsprintf.c).

// --- Module Init / Exit ---

static void hv_init_on_cpu(void* info) {
    struct cosmos_hypervisor* hv = (struct cosmos_hypervisor*)info;
    unsigned int cpu_id = smp_processor_id();
    int res;

    printk(KERN_INFO "CosmOS-HV: Initializing CPU %u...\n", cpu_id);

    res = _Z20cosmos_hv_enable_vmxP17cosmos_hypervisorj(hv, cpu_id);
    if (res != 0) {
        printk(KERN_ERR "CosmOS-HV: VMXON failed on CPU %u (res=%d)\n", cpu_id, res);
        return;
    }

    res = _Z20cosmos_hv_setup_vmcsP17cosmos_hypervisorj(hv, cpu_id);
    if (res != 0) {
        printk(KERN_ERR "CosmOS-HV: VMCS setup failed on CPU %u (res=%d)\n", cpu_id, res);
        return;
    }

    printk(KERN_INFO "CosmOS-HV: Executing Host Subversion on CPU %u...\n", cpu_id);
    res = _Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj(hv, cpu_id);
    
    if (res == 0) {
        printk(KERN_INFO "CosmOS-HV: Host Subversion on CPU %u successful!\n", cpu_id);
    } else {
        printk(KERN_ERR "CosmOS-HV: Host Subversion failed on CPU %u (%d)\n", cpu_id, res);
    }
}

static int __init cosmos_hv_init_module(void) {
    struct cosmos_hypervisor* hv;
    int res;

    printk(KERN_INFO "CosmOS-HV: Loading Stealth Hypervisor LKM...\n");

    hv = _Z20cosmos_hv_get_globalv();
    if (!hv) {
        printk(KERN_ERR "CosmOS-HV: Could not fetch global state!\n");
        return -1;
    }

    res = _Z14cosmos_hv_initP17cosmos_hypervisor(hv);
    if (res != 0) return res;

    res = _Z20cosmos_hv_detect_vmxP17cosmos_hypervisor(hv);
    if (res != 0) return res;

    res = _Z19cosmos_hv_setup_eptP17cosmos_hypervisor(hv);
    if (res != 0) return res;

    // Execute VMX initialization on ALL CPUs!
    on_each_cpu(hv_init_on_cpu, hv, 1);
    
    // Activate EPT Memory Stealth
    // Hide all critical hypervisor pages (VMXON, VMCS, stacks, etc.)
    // onto a dummy page so they can no longer be read by the guest!
    extern int _Z25cosmos_ept_hide_hv_memoryP17cosmos_hypervisor(struct cosmos_hypervisor* hv);
    _Z25cosmos_ept_hide_hv_memoryP17cosmos_hypervisor(hv);
    
    // After EPT changes we MUST flush the EPT TLB!
    // Since we are in guest mode, request hypervisor via VMCALL
    // to execute the INVEPT instruction (allowed only in root mode).
    __asm__ __volatile__("mov $0x1337, %%rax; vmcall" : : : "rax");

    // Prevent module unloading since CPUs are subverted
    // and no de-subversion logic (VM-Exit to root-mode OS) exists yet!
    __module_get(THIS_MODULE);

    return 0; // Module loaded
}

static void __exit cosmos_hv_cleanup_module(void) {
    struct cosmos_hypervisor* hv = _Z20cosmos_hv_get_globalv();
    printk(KERN_INFO "CosmOS-HV: Unloading hypervisor...\n");
    if (hv) {
        _Z18cosmos_hv_shutdownP17cosmos_hypervisor(hv);
    }
    printk(KERN_INFO "CosmOS-HV: LKM unloaded.\n");
}

module_init(cosmos_hv_init_module);
module_exit(cosmos_hv_cleanup_module);
