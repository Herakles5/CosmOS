/*
 * ============================================================================
 *  COSMOS HYPERVISOR - Type-1 Stealth Hypervisor for CosmOS
 * ============================================================================
 *  
 *  Ein unsichtbarer Hypervisor, der direkt auf der CPU läuft (Ring -1).
 *  Ziel: Windows-Gast mit GPU-Passthrough ausführen, wobei Anti-Cheat-
 *  Systeme den Hypervisor NICHT erkennen können.
 *
 *  Target CPU: Intel i7-7700 (Kaby Lake) mit VT-x, EPT, VPID
 *  
 *  Phase 1.1: VMX Detection & VMXON
 * ============================================================================
 */

#ifndef COSMOS_HYPERVISOR_H
#define COSMOS_HYPERVISOR_H

#include <stdint.h>

/* ============================================================================
 *  CPU FEATURE DETECTION (CPUID)
 * ============================================================================ */

// CPUID Leaf 1, ECX Bit-Flags
#define CPUID_1_ECX_VMX             (1 << 5)    // Virtual Machine Extensions
#define CPUID_1_ECX_SMX             (1 << 6)    // Safer Mode Extensions
#define CPUID_1_ECX_HYPERVISOR      (1 << 31)   // Hypervisor Present (MUSS im Stealth-Modus gelöscht werden!)

/* ============================================================================
 *  MODEL SPECIFIC REGISTERS (MSRs)
 * ============================================================================ */

// VMX Capability MSRs
#define IA32_FEATURE_CONTROL            0x003A
#define IA32_VMX_BASIC                  0x0480
#define IA32_VMX_PINBASED_CTLS          0x0481
#define IA32_VMX_PROCBASED_CTLS         0x0482
#define IA32_VMX_EXIT_CTLS              0x0483
#define IA32_VMX_ENTRY_CTLS             0x0484
#define IA32_VMX_MISC                   0x0485
#define IA32_VMX_CR0_FIXED0             0x0486
#define IA32_VMX_CR0_FIXED1             0x0487
#define IA32_VMX_CR4_FIXED0             0x0488
#define IA32_VMX_CR4_FIXED1             0x0489
#define IA32_VMX_VMCS_ENUM              0x048A
#define IA32_VMX_PROCBASED_CTLS2        0x048B
#define IA32_VMX_EPT_VPID_CAP           0x048C
#define IA32_VMX_TRUE_PINBASED_CTLS     0x048D
#define IA32_VMX_TRUE_PROCBASED_CTLS    0x048E
#define IA32_VMX_TRUE_EXIT_CTLS         0x048F
#define IA32_VMX_TRUE_ENTRY_CTLS        0x0490

// General MSRs
#define IA32_DEBUGCTL                   0x01D9
#define IA32_SYSENTER_CS                0x0174
#define IA32_SYSENTER_ESP               0x0175
#define IA32_SYSENTER_EIP               0x0176
#define IA32_PAT                        0x0277
#define IA32_EFER                       0x0C0000080
#define IA32_STAR                       0xC0000081
#define IA32_LSTAR                      0xC0000082
#define IA32_CSTAR                      0xC0000083
#define IA32_FMASK                      0xC0000084
#define IA32_FS_BASE                    0xC0000100
#define IA32_GS_BASE                    0xC0000101
#define IA32_KERNEL_GS_BASE             0xC0000102
#define IA32_TSC_AUX                    0xC0000103

// Feature Control MSR Bits
#define FEATURE_CONTROL_LOCKED          (1 << 0)
#define FEATURE_CONTROL_VMXON_IN_SMX    (1 << 1)
#define FEATURE_CONTROL_VMXON_OUTSIDE   (1 << 2)

/* ============================================================================
 *  CONTROL REGISTER BITS
 * ============================================================================ */

#define CR0_PE  (1UL << 0)   // Protection Enable
#define CR0_MP  (1UL << 1)   // Monitor Coprocessor
#define CR0_EM  (1UL << 2)   // Emulation
#define CR0_TS  (1UL << 3)   // Task Switched
#define CR0_ET  (1UL << 4)   // Extension Type
#define CR0_NE  (1UL << 5)   // Numeric Error
#define CR0_WP  (1UL << 16)  // Write Protect
#define CR0_NW  (1UL << 29)  // Not Write-through
#define CR0_CD  (1UL << 30)  // Cache Disable
#define CR0_PG  (1UL << 31)  // Paging

#define CR4_VME         (1UL << 0)
#define CR4_PVI         (1UL << 1)
#define CR4_TSD         (1UL << 2)   // Time Stamp Disable (WICHTIG für Stealth!)
#define CR4_DE          (1UL << 3)
#define CR4_PSE         (1UL << 4)
#define CR4_PAE         (1UL << 5)
#define CR4_MCE         (1UL << 6)
#define CR4_PGE         (1UL << 7)
#define CR4_PCE         (1UL << 8)
#define CR4_OSFXSR      (1UL << 9)
#define CR4_OSXMMEXCPT  (1UL << 10)
#define CR4_VMXE        (1UL << 13)  // VMX Enable (DAS ist der Schlüssel!)
#define CR4_SMXE        (1UL << 14)
#define CR4_FSGSBASE    (1UL << 16)
#define CR4_PCIDE       (1UL << 17)
#define CR4_OSXSAVE     (1UL << 18)
#define CR4_SMEP        (1UL << 20)
#define CR4_SMAP        (1UL << 21)

/* ============================================================================
 *  VMCS FIELD ENCODINGS (Intel SDM Vol. 3, Appendix B)
 * ============================================================================
 *
 *  Encoding Format: [Width:2][Type:2][Index:9][Reserved:1][AccessType:1]
 *  Width:  0=16-bit, 1=64-bit, 2=32-bit, 3=natural-width
 *  Type:   0=Control, 1=VM-Exit info, 2=Guest state, 3=Host state
 * ============================================================================ */

// --- 16-bit Control Fields ---
#define VMCS_VPID                       0x0000
#define VMCS_POSTED_INT_NOTIFY          0x0002
#define VMCS_EPTP_INDEX                 0x0004

// --- 16-bit Guest State Fields ---
#define VMCS_GUEST_ES_SEL               0x0800
#define VMCS_GUEST_CS_SEL               0x0802
#define VMCS_GUEST_SS_SEL               0x0804
#define VMCS_GUEST_DS_SEL               0x0806
#define VMCS_GUEST_FS_SEL               0x0808
#define VMCS_GUEST_GS_SEL               0x080A
#define VMCS_GUEST_LDTR_SEL             0x080C
#define VMCS_GUEST_TR_SEL               0x080E

// --- 16-bit Host State Fields ---
#define VMCS_HOST_ES_SEL                0x0C00
#define VMCS_HOST_CS_SEL                0x0C02
#define VMCS_HOST_SS_SEL                0x0C04
#define VMCS_HOST_DS_SEL                0x0C06
#define VMCS_HOST_FS_SEL                0x0C08
#define VMCS_HOST_GS_SEL                0x0C0A
#define VMCS_HOST_TR_SEL                0x0C0C

// --- 64-bit Control Fields ---
#define VMCS_IO_BITMAP_A                0x2000
#define VMCS_IO_BITMAP_B                0x2002
#define VMCS_MSR_BITMAP                 0x2004
#define VMCS_EXIT_MSR_STORE_ADDR        0x2006
#define VMCS_EXIT_MSR_LOAD_ADDR         0x2008
#define VMCS_ENTRY_MSR_LOAD_ADDR        0x200A
#define VMCS_EXECUTIVE_VMCS_PTR         0x200C
#define VMCS_TSC_OFFSET                 0x2010  // KRITISCH für RDTSC-Stealth!
#define VMCS_VIRTUAL_APIC_PAGE          0x2012
#define VMCS_APIC_ACCESS_ADDR           0x2014
#define VMCS_EPTP                       0x201A  // Extended Page Table Pointer
#define VMCS_XSS_EXIT_BITMAP            0x202C

// --- 64-bit Guest State Fields ---
#define VMCS_GUEST_VMCS_LINK_PTR        0x2800
#define VMCS_GUEST_DEBUGCTL             0x2802
#define VMCS_GUEST_PAT                  0x2804
#define VMCS_GUEST_EFER                 0x2806
#define VMCS_GUEST_PERF_GLOBAL_CTRL     0x2808

// --- 64-bit Host State Fields ---
#define VMCS_HOST_PAT                   0x2C00
#define VMCS_HOST_EFER                  0x2C02

// --- 64-bit Natural-Width Control Fields ---
#define VMCS_CR0_GUEST_HOST_MASK        0x6000
#define VMCS_CR4_GUEST_HOST_MASK        0x6002
#define VMCS_CR0_READ_SHADOW            0x6004
#define VMCS_CR4_READ_SHADOW            0x6006
#define VMCS_HOST_PERF_GLOBAL_CTRL      0x2C04

// --- 32-bit Control Fields ---
#define VMCS_PIN_BASED_EXEC_CTRL        0x4000
#define VMCS_PROC_BASED_EXEC_CTRL       0x4002  // Primary
#define VMCS_EXCEPTION_BITMAP           0x4004
#define VMCS_PF_ERROR_CODE_MASK         0x4006
#define VMCS_PF_ERROR_CODE_MATCH        0x4008
#define VMCS_CR3_TARGET_COUNT           0x400A
#define VMCS_EXIT_CONTROLS              0x400C
#define VMCS_EXIT_MSR_STORE_COUNT       0x400E
#define VMCS_EXIT_MSR_LOAD_COUNT        0x4010
#define VMCS_ENTRY_CONTROLS             0x4012
#define VMCS_ENTRY_MSR_LOAD_COUNT       0x4014
#define VMCS_ENTRY_INT_INFO             0x4016
#define VMCS_ENTRY_EXCEPTION_ERRCODE    0x4018
#define VMCS_ENTRY_INSTR_LENGTH         0x401A
#define VMCS_TPR_THRESHOLD              0x401C
#define VMCS_PROC_BASED_EXEC_CTRL2      0x401E  // Secondary

// --- 32-bit VM-Exit Information Fields ---
#define VMCS_EXIT_REASON                0x4402
#define VMCS_EXIT_QUALIFICATION         0x6400  // Natural-width!
#define VMCS_EXIT_INT_INFO              0x4404
#define VMCS_EXIT_INT_ERRCODE           0x4406
#define VMCS_IDT_VECTORING_INFO         0x4408
#define VMCS_IDT_VECTORING_ERRCODE      0x440A
#define VMCS_EXIT_INSTR_LENGTH          0x440C
#define VMCS_EXIT_INSTR_INFO            0x440E
#define VMCS_GUEST_LINEAR_ADDR          0x640A  // Natural-width!

// --- 32-bit Guest State Fields ---
#define VMCS_GUEST_ES_LIMIT             0x4800
#define VMCS_GUEST_CS_LIMIT             0x4802
#define VMCS_GUEST_SS_LIMIT             0x4804
#define VMCS_GUEST_DS_LIMIT             0x4806
#define VMCS_GUEST_FS_LIMIT             0x4808
#define VMCS_GUEST_GS_LIMIT             0x480A
#define VMCS_GUEST_LDTR_LIMIT           0x480C
#define VMCS_GUEST_TR_LIMIT             0x480E
#define VMCS_GUEST_GDTR_LIMIT           0x4810
#define VMCS_GUEST_IDTR_LIMIT           0x4812
#define VMCS_GUEST_ES_ACCESS            0x4814
#define VMCS_GUEST_CS_ACCESS            0x4816
#define VMCS_GUEST_SS_ACCESS            0x4818
#define VMCS_GUEST_DS_ACCESS            0x481A
#define VMCS_GUEST_FS_ACCESS            0x481C
#define VMCS_GUEST_GS_ACCESS            0x481E
#define VMCS_GUEST_LDTR_ACCESS          0x4820
#define VMCS_GUEST_TR_ACCESS            0x4822
#define VMCS_GUEST_INT_STATE            0x4824
#define VMCS_GUEST_ACTIVITY_STATE       0x4826
#define VMCS_GUEST_SMBASE               0x4828
#define VMCS_GUEST_SYSENTER_CS          0x482A
#define VMCS_GUEST_PREEMPT_TIMER        0x482E

// --- Natural-Width Guest State Fields ---
#define VMCS_GUEST_CR0                  0x6800
#define VMCS_GUEST_CR3                  0x6802
#define VMCS_GUEST_CR4                  0x6804
#define VMCS_GUEST_ES_BASE              0x6806
#define VMCS_GUEST_CS_BASE              0x6808
#define VMCS_GUEST_SS_BASE              0x680A
#define VMCS_GUEST_DS_BASE              0x680C
#define VMCS_GUEST_FS_BASE              0x680E
#define VMCS_GUEST_GS_BASE              0x6810
#define VMCS_GUEST_LDTR_BASE            0x6812
#define VMCS_GUEST_TR_BASE              0x6814
#define VMCS_GUEST_GDTR_BASE            0x6816
#define VMCS_GUEST_IDTR_BASE            0x6818
#define VMCS_GUEST_DR7                  0x681A
#define VMCS_GUEST_RSP                  0x681C
#define VMCS_GUEST_RIP                  0x681E
#define VMCS_GUEST_RFLAGS               0x6820
#define VMCS_GUEST_PENDING_DBG          0x6822
#define VMCS_GUEST_SYSENTER_ESP         0x6824
#define VMCS_GUEST_SYSENTER_EIP         0x6826

// --- Natural-Width Host State Fields ---
#define VMCS_HOST_CR0                   0x6C00
#define VMCS_HOST_CR3                   0x6C02
#define VMCS_HOST_CR4                   0x6C04
#define VMCS_HOST_FS_BASE               0x6C06
#define VMCS_HOST_GS_BASE               0x6C08
#define VMCS_HOST_TR_BASE               0x6C0A
#define VMCS_HOST_GDTR_BASE             0x6C0C
#define VMCS_HOST_IDTR_BASE             0x6C0E
#define VMCS_HOST_SYSENTER_ESP          0x6C10
#define VMCS_HOST_SYSENTER_EIP          0x6C12
#define VMCS_HOST_RSP                   0x6C14
#define VMCS_HOST_RIP                   0x6C16

/* ============================================================================
 *  VM-EXIT REASONS (Intel SDM Vol. 3, Appendix C)
 * ============================================================================ */

#define VMEXIT_EXCEPTION_NMI            0
#define VMEXIT_EXTERNAL_INT             1
#define VMEXIT_TRIPLE_FAULT             2
#define VMEXIT_INIT_SIGNAL              3
#define VMEXIT_SIPI                     4
#define VMEXIT_SMI                      6
#define VMEXIT_OTHER_SMI                7
#define VMEXIT_INT_WINDOW               7
#define VMEXIT_NMI_WINDOW               8
#define VMEXIT_TASK_SWITCH              9
#define VMEXIT_CPUID                    10  // ← DAS ist der wichtigste für Stealth!
#define VMEXIT_GETSEC                   11
#define VMEXIT_HLT                      12
#define VMEXIT_INVD                     13
#define VMEXIT_INVLPG                   14
#define VMEXIT_RDPMC                    15
#define VMEXIT_RDTSC                    16  // ← Timing-Stealth!
#define VMEXIT_RSM                      17
#define VMEXIT_VMCALL                   18
#define VMEXIT_VMCLEAR                  19
#define VMEXIT_VMLAUNCH                 20
#define VMEXIT_VMPTRLD                  21
#define VMEXIT_VMPTRST                  22
#define VMEXIT_VMREAD                   23
#define VMEXIT_VMRESUME                 24
#define VMEXIT_VMWRITE                  25
#define VMEXIT_VMXOFF                   26
#define VMEXIT_VMXON                    27
#define VMEXIT_CR_ACCESS                28
#define VMEXIT_DR_ACCESS                29
#define VMEXIT_IO                       30
#define VMEXIT_RDMSR                    31  // ← MSR-Stealth!
#define VMEXIT_WRMSR                    32
#define VMEXIT_ENTRY_FAIL_GUEST         33
#define VMEXIT_ENTRY_FAIL_MSR           34
#define VMEXIT_MWAIT                    36
#define VMEXIT_MONITOR_TRAP             37
#define VMEXIT_MONITOR                  39
#define VMEXIT_PAUSE                    40
#define VMEXIT_ENTRY_FAIL_MC            41
#define VMEXIT_TPR_BELOW                43
#define VMEXIT_APIC_ACCESS              44
#define VMEXIT_VIRTUALIZED_EOI          45
#define VMEXIT_GDTR_IDTR                46
#define VMEXIT_LDTR_TR                  47
#define VMEXIT_EPT_VIOLATION            48  // ← Speicher-Stealth!
#define VMEXIT_EPT_MISCONFIG            49
#define VMEXIT_INVEPT                   50
#define VMEXIT_RDTSCP                   51  // ← Timing-Stealth!
#define VMEXIT_PREEMPT_TIMER            52
#define VMEXIT_INVVPID                  53
#define VMEXIT_WBINVD                   54
#define VMEXIT_XSETBV                   55
#define VMEXIT_APIC_WRITE               56
#define VMEXIT_RDRAND                   57
#define VMEXIT_INVPCID                  58
#define VMEXIT_VMFUNC                   59
#define VMEXIT_RDSEED                   63
#define VMEXIT_XSAVES                   63
#define VMEXIT_XRSTORS                  64

/* ============================================================================
 *  PRIMARY PROC-BASED VM-EXECUTION CONTROLS
 * ============================================================================ */

#define PROCBASED_INT_WINDOW_EXIT       (1UL << 2)
#define PROCBASED_USE_TSC_OFFSET        (1UL << 3)   // Für RDTSC-Stealth
#define PROCBASED_HLT_EXIT              (1UL << 7)
#define PROCBASED_INVLPG_EXIT           (1UL << 9)
#define PROCBASED_MWAIT_EXIT            (1UL << 10)
#define PROCBASED_RDPMC_EXIT            (1UL << 11)
#define PROCBASED_RDTSC_EXIT            (1UL << 12)  // RDTSC abfangen
#define PROCBASED_CR3_LOAD_EXIT         (1UL << 15)
#define PROCBASED_CR3_STORE_EXIT        (1UL << 16)
#define PROCBASED_CR8_LOAD_EXIT         (1UL << 19)
#define PROCBASED_CR8_STORE_EXIT        (1UL << 20)
#define PROCBASED_USE_TPR_SHADOW        (1UL << 21)
#define PROCBASED_NMI_WINDOW_EXIT       (1UL << 22)
#define PROCBASED_MOV_DR_EXIT           (1UL << 23)
#define PROCBASED_UNCOND_IO_EXIT        (1UL << 24)
#define PROCBASED_USE_IO_BITMAPS        (1UL << 25)
#define PROCBASED_MONITOR_TRAP          (1UL << 27)
#define PROCBASED_USE_MSR_BITMAPS       (1UL << 28)  // Für MSR-Stealth
#define PROCBASED_MONITOR_EXIT          (1UL << 29)
#define PROCBASED_PAUSE_EXIT            (1UL << 30)
#define PROCBASED_ACTIVATE_SECONDARY    (1UL << 31)  // Secondary Controls aktivieren!

/* ============================================================================
 *  SECONDARY PROC-BASED VM-EXECUTION CONTROLS
 * ============================================================================ */

#define PROCBASED2_VIRT_APIC_ACCESS     (1UL << 0)
#define PROCBASED2_ENABLE_EPT           (1UL << 1)   // EPT aktivieren!
#define PROCBASED2_DESC_TABLE_EXIT      (1UL << 2)
#define PROCBASED2_ENABLE_RDTSCP        (1UL << 3)
#define PROCBASED2_VIRT_X2APIC          (1UL << 4)
#define PROCBASED2_ENABLE_VPID          (1UL << 5)   // VPID für Performance
#define PROCBASED2_WBINVD_EXIT          (1UL << 6)
#define PROCBASED2_UNRESTRICTED_GUEST   (1UL << 7)   // Real-Mode Gast!
#define PROCBASED2_APIC_REGISTER_VIRT   (1UL << 8)
#define PROCBASED2_VIRTUAL_INT_DELIVERY (1UL << 9)
#define PROCBASED2_PAUSE_LOOP_EXIT      (1UL << 10)
#define PROCBASED2_RDRAND_EXIT          (1UL << 11)
#define PROCBASED2_ENABLE_INVPCID       (1UL << 12)
#define PROCBASED2_ENABLE_VMFUNC        (1UL << 13)
#define PROCBASED2_VMCS_SHADOWING       (1UL << 14)
#define PROCBASED2_ENABLE_ENCLS_EXIT    (1UL << 15)
#define PROCBASED2_RDSEED_EXIT          (1UL << 16)
#define PROCBASED2_ENABLE_PML           (1UL << 17)
#define PROCBASED2_EPT_VIOLATION_VE     (1UL << 18)
#define PROCBASED2_CONCEAL_VMX          (1UL << 19)  // Intel sagt: bitte nicht benutzen :)
#define PROCBASED2_ENABLE_XSAVES        (1UL << 20)
#define PROCBASED2_TSC_SCALING          (1UL << 25)

/* ============================================================================
 *  VM-EXIT CONTROLS
 * ============================================================================ */

#define VMEXIT_CTRL_SAVE_DBG            (1UL << 2)
#define VMEXIT_CTRL_HOST_ADDR_SPACE     (1UL << 9)   // Host ist 64-bit
#define VMEXIT_CTRL_LOAD_PERF           (1UL << 12)
#define VMEXIT_CTRL_ACK_INT_ON_EXIT     (1UL << 15)
#define VMEXIT_CTRL_SAVE_PAT            (1UL << 18)
#define VMEXIT_CTRL_LOAD_PAT            (1UL << 19)
#define VMEXIT_CTRL_SAVE_EFER           (1UL << 20)
#define VMEXIT_CTRL_LOAD_EFER           (1UL << 21)
#define VMEXIT_CTRL_SAVE_PREEMPT_TIMER  (1UL << 22)

/* ============================================================================
 *  VM-ENTRY CONTROLS
 * ============================================================================ */

#define VMENTRY_CTRL_LOAD_DBG           (1UL << 2)
#define VMENTRY_CTRL_GUEST_IA32E        (1UL << 9)   // Gast ist 64-bit
#define VMENTRY_CTRL_ENTRY_TO_SMM       (1UL << 10)
#define VMENTRY_CTRL_DEACT_DUAL_MON     (1UL << 11)
#define VMENTRY_CTRL_LOAD_PERF          (1UL << 13)
#define VMENTRY_CTRL_LOAD_PAT           (1UL << 14)
#define VMENTRY_CTRL_LOAD_EFER          (1UL << 15)

/* ============================================================================
 *  DATENSTRUKTUREN
 * ============================================================================ */

// Guest-Register-Kontext (wird bei jedem VM-Exit gesichert/wiederhergestellt)
struct __attribute__((packed)) vmx_guest_regs {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    // XMM wird separat via XSAVE/XRSTOR gehandhabt
};

// VMXON/VMCS Region Header (muss 4KB-aligned sein!)
struct __attribute__((packed, aligned(4096))) vmx_region {
    uint32_t revision_id;   // Muss mit IA32_VMX_BASIC[30:0] übereinstimmen
    uint32_t abort_indicator;
    uint8_t  data[4096 - 8];
};

// Per-CPU Hypervisor-Zustand
struct cosmos_vcpu {
    // VMX Regions (müssen physisch 4KB-aligned sein)
    vmx_region*     vmxon_region;       // VMXON Region (Host)
    vmx_region*     vmcs_region;        // VMCS Region (Gast-Zustand)

    // Physische Adressen der Regions
    uint64_t        vmxon_phys;
    uint64_t        vmcs_phys;

    // Guest Register State (nicht in VMCS gespeichert)
    vmx_guest_regs  guest_regs;

    // Host Stack für VM-Exit Handler
    uint8_t*        host_stack;
    uint64_t        host_stack_phys;

    // MSR Bitmap (4KB, jedes Bit = ein MSR)
    uint8_t*        msr_bitmap;
    uint64_t        msr_bitmap_phys;

    // CPU-Nummer
    uint32_t        cpu_id;

    // Status Flags
    uint8_t         vmx_enabled;        // VMXON erfolgreich?
    uint8_t         vmcs_active;        // VMCS geladen?
    uint8_t         guest_running;      // Gast läuft?

    // Stealth: TSC-Offset für Timing-Kompensation
    int64_t         tsc_offset;
    uint64_t        last_exit_tsc;      // TSC beim letzten VM-Exit
    
    // Robustes Host-CR3 (verhindert Absturz wenn aufrufender Prozess terminiert)
    void*           host_cr3_region;
    uint64_t        host_cr3_phys;
};

// Globaler Hypervisor-Zustand
struct cosmos_hypervisor {
    // VMX Capabilities (aus MSRs gelesen)
    uint32_t        vmcs_revision_id;
    uint32_t        vmcs_region_size;
    uint64_t        vmx_pin_based_allowed0;
    uint64_t        vmx_pin_based_allowed1;
    uint64_t        vmx_proc_based_allowed0;
    uint64_t        vmx_proc_based_allowed1;
    uint64_t        vmx_proc_based2_allowed0;
    uint64_t        vmx_proc_based2_allowed1;
    uint64_t        vmx_exit_allowed0;
    uint64_t        vmx_exit_allowed1;
    uint64_t        vmx_entry_allowed0;
    uint64_t        vmx_entry_allowed1;
    uint64_t        ept_vpid_cap;

    // Feature Flags
    uint8_t         has_vmx;
    uint8_t         has_ept;
    uint8_t         has_vpid;
    uint8_t         has_unrestricted_guest;
    uint8_t         has_true_msrs;
    uint8_t         has_preempt_timer;

    // EPT Root (PML4 physische Adresse)
    uint64_t        ept_pml4_phys;
    void*           ept_pml4_virt;

    // Per-CPU State (max 256 CPUs)
    cosmos_vcpu     vcpus[256];
    uint32_t        num_cpus;

    // Globale Status-Flags
    uint8_t         initialized;
    uint8_t         stealth_mode;       // Anti-Detection aktiv?
};

/* ============================================================================
 *  INLINE ASSEMBLY HELPERS
 * ============================================================================ */

static inline uint64_t cosmos_rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ __volatile__(
        "rdmsr"
        : "=a"(lo), "=d"(hi)
        : "c"(msr)
    );
    return ((uint64_t)hi << 32) | lo;
}

static inline void cosmos_wrmsr(uint32_t msr, uint64_t value) {
    __asm__ __volatile__(
        "wrmsr"
        :
        : "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32))
    );
}

static inline uint64_t cosmos_read_cr0(void) {
    uint64_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

static inline void cosmos_write_cr0(uint64_t cr0) {
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0));
}

static inline uint64_t cosmos_read_cr3(void) {
    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline uint64_t cosmos_read_cr4(void) {
    uint64_t cr4;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

static inline void cosmos_write_cr4(uint64_t cr4) {
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4));
}

static inline uint64_t cosmos_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void cosmos_cpuid(uint32_t leaf, uint32_t subleaf,
                                uint32_t* eax, uint32_t* ebx,
                                uint32_t* ecx, uint32_t* edx) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

static inline uint16_t cosmos_read_cs(void) {
    uint16_t cs;
    __asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static inline uint16_t cosmos_read_ss(void) {
    uint16_t ss;
    __asm__ __volatile__("mov %%ss, %0" : "=r"(ss));
    return ss;
}

static inline uint16_t cosmos_read_ds(void) {
    uint16_t ds;
    __asm__ __volatile__("mov %%ds, %0" : "=r"(ds));
    return ds;
}

static inline uint16_t cosmos_read_es(void) {
    uint16_t es;
    __asm__ __volatile__("mov %%es, %0" : "=r"(es));
    return es;
}

static inline uint16_t cosmos_read_fs(void) {
    uint16_t fs;
    __asm__ __volatile__("mov %%fs, %0" : "=r"(fs));
    return fs;
}

static inline uint16_t cosmos_read_gs(void) {
    uint16_t gs;
    __asm__ __volatile__("mov %%gs, %0" : "=r"(gs));
    return gs;
}

static inline uint16_t cosmos_read_tr(void) {
    uint16_t tr;
    __asm__ __volatile__("str %0" : "=r"(tr));
    return tr;
}

static inline uint16_t cosmos_read_ldtr(void) {
    uint16_t ldtr;
    __asm__ __volatile__("sldt %0" : "=r"(ldtr));
    return ldtr;
}

struct desc_table_reg {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static inline void cosmos_sgdt(desc_table_reg* gdtr) {
    __asm__ __volatile__("sgdt %0" : "=m"(*gdtr));
}

static inline void cosmos_sidt(desc_table_reg* idtr) {
    __asm__ __volatile__("sidt %0" : "=m"(*idtr));
}

static inline uint64_t cosmos_read_rflags(void) {
    uint64_t rflags;
    __asm__ __volatile__("pushfq; popq %0" : "=r"(rflags));
    return rflags;
}

static inline uint64_t cosmos_read_rsp(void) {
    uint64_t rsp;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(rsp));
    return rsp;
}

/* ============================================================================
 *  VMX INSTRUCTION WRAPPERS (return 0 on success, error flags otherwise)
 *  These are defined in cosmos_vmx.S
 * ============================================================================ */

extern "C" {
    // VMX Lifecycle
    uint8_t cosmos_vmxon(uint64_t* vmxon_region_phys);
    uint8_t cosmos_vmxoff(void);

    // VMCS Management
    uint8_t cosmos_vmptrld(uint64_t* vmcs_phys);
    uint8_t cosmos_vmclear(uint64_t* vmcs_phys);
    uint8_t cosmos_vmread(uint64_t field, uint64_t* value);
    uint8_t cosmos_vmwrite(uint64_t field, uint64_t value);

    // VM Entry/Exit
    void    cosmos_vmlaunch_trampoline(vmx_guest_regs* guest_regs);
    void    cosmos_vmresume_trampoline(vmx_guest_regs* guest_regs);

    // VM-Exit Stub (entry point from hardware, calls cosmos_vmexit_handler)
    void    cosmos_vmexit_stub(void);

    // VM-Exit Handler (called from assembly)
    int     cosmos_vmexit_handler(vmx_guest_regs* guest_regs);
    int     cosmos_hv_handle_ept_violation(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);
}

/* ============================================================================
 *  HIGH-LEVEL HYPERVISOR API
 *  Implemented in cosmos_hypervisor.cpp
 * ============================================================================ */

// Phase 1.1: Detection & Initialization
int  cosmos_hv_detect_vmx(cosmos_hypervisor* hv);
int  cosmos_hv_enable_vmx(cosmos_hypervisor* hv, uint32_t cpu_id);
void cosmos_hv_disable_vmx(cosmos_hypervisor* hv, uint32_t cpu_id);

// Phase 1.2: VMCS Setup
int  cosmos_hv_setup_vmcs(cosmos_hypervisor* hv, uint32_t cpu_id);

// Phase 1.3: Stealth
void cosmos_hv_handle_cpuid(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);
void cosmos_hv_handle_rdtsc(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);
void cosmos_hv_handle_rdmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);
void cosmos_hv_handle_wrmsr(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);

// Phase 1.4: EPT
int  cosmos_hv_setup_ept(cosmos_hypervisor* hv);

#endif // COSMOS_HYPERVISOR_H
