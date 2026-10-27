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

// --- Deklarationen der C++ Hypervisor-Funktionen ---
// Diese werden aus cosmos_hypervisor_all.o (baremetal mode) gelinkt.
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

// --- Bereitstellung der "Bare-Metal" Abhängigkeiten für den Hypervisor ---

void* cosmos_alloc_page_aligned(uint64_t* out_phys);
int printf(const char* fmt, ...);

void* cosmos_alloc_page_aligned(uint64_t* out_phys) {
    // Hole eine 4KB Page, initialisiert mit Nullen
    unsigned long page = __get_free_page(GFP_KERNEL | __GFP_ZERO);
    if (!page) {
        printk(KERN_ERR "CosmOS-HV: alloc_page fehlgeschlagen!\n");
        return NULL;
    }
    
    // Berechne die physische Adresse für das VMCS/VMXON
    *out_phys = virt_to_phys((void*)page);
    return (void*)page;
}

// C++ Mangled Wrapper
void* _Z25cosmos_alloc_page_alignedPm(uint64_t* out_phys) {
    return cosmos_alloc_page_aligned(out_phys);
}

void* _Z26cosmos_alloc_pages_alignedmPm(unsigned long bytes, uint64_t* out_phys) {
    // Für Kernel-Testing weisen wir einfach Seiten am Stück zu
    int order = get_order(bytes);
    unsigned long page = __get_free_pages(GFP_KERNEL | __GFP_ZERO, order);
    if (!page) return NULL;
    *out_phys = virt_to_phys((void*)page);
    return (void*)page;
}

// C Wrapper für CPU ID (wird via extern "C" aufgerufen)
unsigned int cosmos_get_cpu_id(void) {
    return smp_processor_id();
}

// C Wrapper: Hole die virtuelle Adresse der aktuellen Page Directory (PML4)
void* cosmos_get_current_pml4(void) {
    unsigned long cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    // Bits 0-11 ausmaskieren (Flags & PCID entfernen) um die physische Adresse zu bekommen
    cr3 &= ~0xFFFUL;
    // Physische Adresse in Kernel Logical Address (virtuell) umwandeln
    return phys_to_virt(cr3);
}

// C Wrapper: Konvertiere physikalische Adresse zu virtueller Kernel-Adresse
void* cosmos_phys_to_virt(uint64_t phys_addr) {
    return phys_to_virt(phys_addr);
}

void _Z16cosmos_free_pagePv(void* ptr) {
    if (ptr) free_page((unsigned long)ptr);
}

int _Z20cosmos_vfs_read_filePKcPPvPm(const char* path, void** out_buf, unsigned long* out_size) {
    // Dummy-Implementierung, da Kernel keine Dateien direkt so liest
    printk(KERN_ERR "CosmOS-HV: VFS Read nicht in LKM implementiert (%s)\n", path);
    return -1;
}

// cosmos_invept wird in cosmos_vmx.S definiert, wir müssen es als extern C deklarieren
// Wenn es fehlt, definieren wir hier einen Stub oder rufen es auf
extern void cosmos_invept(unsigned long type, unsigned long desc);
void _Z13cosmos_inveptmm(unsigned long type, unsigned long desc) {
    cosmos_invept(type, desc);
}

// 2. printf Umleitung auf printk
int printf(const char* fmt, ...) {
    va_list args;
    int r;
    va_start(args, fmt);
    // Wir rufen den kernel internen vprintk auf
    r = vprintk(fmt, args);
    va_end(args);
    return r;
}

// memset und snprintf werden normalerweise automatisch vom Kernel aufgelöst,
// da sie in der Kernel-Lib (lib/string.c bzw. lib/vsprintf.c) existieren.

// --- Modul Init / Exit ---

static void hv_init_on_cpu(void* info) {
    struct cosmos_hypervisor* hv = (struct cosmos_hypervisor*)info;
    unsigned int cpu_id = smp_processor_id();
    int res;

    printk(KERN_INFO "CosmOS-HV: Initialisiere CPU %u...\n", cpu_id);

    res = _Z20cosmos_hv_enable_vmxP17cosmos_hypervisorj(hv, cpu_id);
    if (res != 0) {
        printk(KERN_ERR "CosmOS-HV: VMXON fehlgeschlagen auf CPU %u (res=%d)\n", cpu_id, res);
        return;
    }

    res = _Z20cosmos_hv_setup_vmcsP17cosmos_hypervisorj(hv, cpu_id);
    if (res != 0) {
        printk(KERN_ERR "CosmOS-HV: VMCS Setup fehlgeschlagen auf CPU %u (res=%d)\n", cpu_id, res);
        return;
    }

    printk(KERN_INFO "CosmOS-HV: Führe Host Subversion auf CPU %u aus...\n", cpu_id);
    res = _Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj(hv, cpu_id);
    
    if (res == 0) {
        printk(KERN_INFO "CosmOS-HV: Host Subversion auf CPU %u erfolgreich!\n", cpu_id);
    } else {
        printk(KERN_ERR "CosmOS-HV: Host Subversion fehlgeschlagen auf CPU %u (%d)\n", cpu_id, res);
    }
}

static int __init cosmos_hv_init_module(void) {
    struct cosmos_hypervisor* hv;
    int res;

    printk(KERN_INFO "CosmOS-HV: Lade Stealth Hypervisor LKM...\n");

    hv = _Z20cosmos_hv_get_globalv();
    if (!hv) {
        printk(KERN_ERR "CosmOS-HV: Konnte globalen State nicht holen!\n");
        return -1;
    }

    res = _Z14cosmos_hv_initP17cosmos_hypervisor(hv);
    if (res != 0) return res;

    res = _Z20cosmos_hv_detect_vmxP17cosmos_hypervisor(hv);
    if (res != 0) return res;

    res = _Z19cosmos_hv_setup_eptP17cosmos_hypervisor(hv);
    if (res != 0) return res;

    // Führe die VMX-Initialisierung auf ALLEN CPUs aus!
    on_each_cpu(hv_init_on_cpu, hv, 1);
    
    // Aktiviere EPT Memory Stealth
    // Verstecke alle kritischen Hypervisor-Seiten (VMXON, VMCS, Stacks, etc.)
    // auf eine Dummy-Page, damit sie vom Gast nicht mehr gelesen werden können!
    extern int _Z25cosmos_ept_hide_hv_memoryP17cosmos_hypervisor(struct cosmos_hypervisor* hv);
    _Z25cosmos_ept_hide_hv_memoryP17cosmos_hypervisor(hv);
    
    // Nach EPT-Änderungen MÜSSEN wir den EPT TLB flushen!
    // Da wir jetzt im Guest-Modus sind, müssen wir den Hypervisor per VMCALL bitten,
    // das INVEPT-Kommando (das nur im Root-Modus erlaubt ist) für uns auszuführen.
    __asm__ __volatile__("mov $0x1337, %%rax; vmcall" : : : "rax");

    // Verhindere das Entladen des Moduls, da wir die CPUs subvertiert haben
    // und noch keine De-Subvertierungslogik (VM-Exit ins Root-Mode-OS) existiert!
    __module_get(THIS_MODULE);

    return 0; // Modul geladen
}

static void __exit cosmos_hv_cleanup_module(void) {
    struct cosmos_hypervisor* hv = _Z20cosmos_hv_get_globalv();
    printk(KERN_INFO "CosmOS-HV: Entlade Hypervisor...\n");
    if (hv) {
        _Z18cosmos_hv_shutdownP17cosmos_hypervisor(hv);
    }
    printk(KERN_INFO "CosmOS-HV: LKM entladen.\n");
}

module_init(cosmos_hv_init_module);
module_exit(cosmos_hv_cleanup_module);
