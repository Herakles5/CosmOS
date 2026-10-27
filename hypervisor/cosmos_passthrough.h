/*
 * ============================================================================
 *  COSMOS PCIe PASSTHROUGH - GPU Passthrough for the Stealth Hypervisor
 * ============================================================================
 *
 *  Phase 2.1: Direct GPU Assignment to Windows Guest
 *
 *  Target Hardware:
 *    GPU:  NVIDIA GeForce GTX 1060 6GB (GP106) [10de:1c03]
 *    HDMI: NVIDIA GP106 HD Audio        [10de:10f1]
 *    Bus:  01:00.0 / 01:00.1 (IOMMU Group 1)
 *
 *  BARs of GTX 1060:
 *    BAR0: 0xF6000000 (32-bit, non-prefetch, 16MB)  → MMIO Registers
 *    BAR1: 0xE0000000 (64-bit, prefetch, 256MB)     → VRAM
 *    BAR3: 0xF0000000 (64-bit, prefetch, 32MB)      → VRAM Aperture
 *    BAR5: 0xE000      (I/O, 128 bytes)              → Legacy I/O
 *
 *  Strategy:
 *  1. Unbind GPU from Linux driver
 *  2. Map GPU BARs 1:1 into EPT (Guest sees real hardware)
 *  3. Pass MSI interrupts directly through to guest
 *  4. Assign DMA via VT-d DMAR table to guest
 * ============================================================================
 */

#ifndef COSMOS_PASSTHROUGH_H
#define COSMOS_PASSTHROUGH_H

#include <stdint.h>

/* ============================================================================
 *  PCI CONFIGURATION SPACE
 * ============================================================================ */

// PCI Config Space Offsets
#define PCI_VENDOR_ID           0x00
#define PCI_DEVICE_ID           0x02
#define PCI_COMMAND             0x04
#define PCI_STATUS              0x06
#define PCI_REVISION            0x08
#define PCI_CLASS_CODE          0x09
#define PCI_SUBCLASS            0x0A
#define PCI_BASE_CLASS          0x0B
#define PCI_HEADER_TYPE         0x0E
#define PCI_BAR0                0x10
#define PCI_BAR1                0x14
#define PCI_BAR2                0x18
#define PCI_BAR3                0x1C
#define PCI_BAR4                0x20
#define PCI_BAR5                0x24
#define PCI_SUBSYSTEM_VENDOR    0x2C
#define PCI_SUBSYSTEM_ID        0x2E
#define PCI_CAP_PTR             0x34
#define PCI_INT_LINE            0x3C
#define PCI_INT_PIN             0x3D

// PCI Command Register Bits
#define PCI_CMD_IO_SPACE        (1 << 0)
#define PCI_CMD_MEM_SPACE       (1 << 1)
#define PCI_CMD_BUS_MASTER      (1 << 2)
#define PCI_CMD_INT_DISABLE     (1 << 10)

// PCI Capability IDs
#define PCI_CAP_ID_PM           0x01    // Power Management
#define PCI_CAP_ID_MSI          0x05    // Message Signaled Interrupts
#define PCI_CAP_ID_PCIE         0x10    // PCI Express
#define PCI_CAP_ID_MSIX         0x11    // MSI-X

// MSI Capability Offsets (relative to capability start)
#define MSI_CONTROL             0x02
#define MSI_ADDR_LO             0x04
#define MSI_ADDR_HI             0x08
#define MSI_DATA_32             0x08    // When no 64-bit addressing
#define MSI_DATA_64             0x0C    // When 64-bit addressing

// MSI Control Bits
#define MSI_CTRL_ENABLE         (1 << 0)
#define MSI_CTRL_64BIT          (1 << 7)
#define MSI_CTRL_PERVECTOR      (1 << 8)

/* ============================================================================
 *  PCIe MMCONFIG (Memory-Mapped Configuration Space)
 *
 *  On i7-7700/H110 chipset MMCONFIG is at 0xE0000000
 *  Every PCIe device has 4KB config space:
 *    Address = MMCONFIG_BASE + (Bus << 20) + (Device << 15) + (Function << 12) + Offset
 * ============================================================================ */

#define PCIE_MMCONFIG_BASE      0xE0000000ULL

static inline uint64_t pcie_config_addr(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    return PCIE_MMCONFIG_BASE + 
           ((uint64_t)bus << 20) + 
           ((uint64_t)dev << 15) + 
           ((uint64_t)func << 12) + 
           offset;
}

/* ============================================================================
 *  BAR (Base Address Register) DECODING
 * ============================================================================ */

#define BAR_TYPE_MASK           0x01    // Bit 0: 0=Memory, 1=I/O
#define BAR_MEM_TYPE_MASK       0x06    // Bits 2:1: 00=32-bit, 10=64-bit
#define BAR_MEM_PREFETCH        0x08    // Bit 3: Prefetchable
#define BAR_MEM_ADDR_MASK       0xFFFFFFF0ULL
#define BAR_IO_ADDR_MASK        0xFFFFFFFC

struct pci_bar_info {
    uint64_t    base_addr;      // Physical base address
    uint64_t    size;           // Size in bytes
    uint8_t     type;           // 0=Memory, 1=I/O
    uint8_t     is_64bit;       // 64-bit BAR (consumes 2 slots)
    uint8_t     prefetchable;   // Prefetchable Memory
    uint8_t     bar_index;      // BAR number (0-5)
};

/* ============================================================================
 *  PASSTHROUGH DEVICE DESCRIPTOR
 *
 *  Describes a PCI device passed directly to the guest.
 * ============================================================================ */

struct passthrough_device {
    // PCI Location
    uint8_t     bus;
    uint8_t     device;
    uint8_t     function;
    
    // PCI IDs
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint16_t    subsystem_vendor;
    uint16_t    subsystem_id;
    
    // BARs
    pci_bar_info bars[6];
    uint8_t     num_bars;
    
    // MSI/MSI-X
    uint16_t    msi_cap_offset;     // Offset of MSI Capability
    uint16_t    msix_cap_offset;    // Offset of MSI-X Capability
    uint8_t     has_msi;
    uint8_t     has_msix;
    
    // Status
    uint8_t     attached;           // Assigned to guest?
    uint8_t     original_driver;    // Previous loaded driver
    
    // Name (for logging)
    char        name[64];
};

/* ============================================================================
 *  VT-d / DMAR STRUCTURES
 *
 *  Intel VT-d (Virtualization Technology for Directed I/O)
 *  DMA Remapping: Controls which DMA accesses from PCIe devices
 *  can access which physical memory ranges.
 *
 *  DMAR Register Base: 0xFED90000 (from ACPI DMAR table)
 * ============================================================================ */

#define VTD_DMAR_BASE           0xFED90000ULL

// VT-d Register Offsets
#define VTD_VER                 0x00    // Version
#define VTD_CAP                 0x08    // Capability
#define VTD_ECAP                0x10    // Extended Capability
#define VTD_GCMD                0x18    // Global Command
#define VTD_GSTS                0x1C    // Global Status
#define VTD_RTADDR              0x20    // Root Table Address
#define VTD_CCMD                0x28    // Context Command
#define VTD_FSTS                0x34    // Fault Status
#define VTD_FECTL               0x38    // Fault Event Control
#define VTD_FEDATA              0x3C    // Fault Event Data
#define VTD_FEADDR              0x40    // Fault Event Address
#define VTD_IVA                 0x00    // Invalidate Address (IOTLB offset)
#define VTD_IOTLB               0x08    // IOTLB Invalidate (IOTLB offset)

// VT-d Global Command Bits
#define VTD_GCMD_TE             (1UL << 31)    // Translation Enable
#define VTD_GCMD_SRTP           (1UL << 30)    // Set Root Table Pointer
#define VTD_GCMD_IRE            (1UL << 25)    // Interrupt Remapping Enable
#define VTD_GCMD_QIE            (1UL << 26)    // Queued Invalidation Enable

// VT-d Global Status Bits
#define VTD_GSTS_TES            (1UL << 31)    // Translation Enable Status
#define VTD_GSTS_RTPS           (1UL << 30)    // Root Table Pointer Status
#define VTD_GSTS_IRES           (1UL << 25)    // Interrupt Remapping Enable Status

/* ============================================================================
 *  VT-d PAGE TABLES (Second-Level Translation)
 *
 *  VT-d uses its own page tables (similar to EPT):
 *    Root Table → Context Table → Second-Level PML4 → ... → Page
 *
 *  Root Table:   256 entries (one per PCI bus)
 *  Context Table: 256 entries (one per Device:Function on bus)
 *  Each context entry points to its own Second-Level Page Table
 * ============================================================================ */

// Root Entry (16 Bytes)
struct __attribute__((packed)) vtd_root_entry {
    uint64_t lo;    // Bit 0: Present, Bits [63:12]: Context Table Address
    uint64_t hi;    // Reserved
};

// Context Entry (16 Bytes)
struct __attribute__((packed)) vtd_context_entry {
    uint64_t lo;    // Bit 0: Present, Bit 1: Fault Disable, Bits [3:2]: Translation Type
                    // Bits [63:12]: Second-Level Page Table Address
    uint64_t hi;    // Bits [2:0]: Address Width (0=30bit, 1=39bit, 2=48bit)
                    // Bits [23:8]: Domain ID
};

// VT-d Root/Context Table Alignment
#define VTD_ROOT_TABLE_SIZE     (256 * sizeof(vtd_root_entry))  // 4KB
#define VTD_CONTEXT_TABLE_SIZE  (256 * sizeof(vtd_context_entry)) // 4KB

/* ============================================================================
 *  PASSTHROUGH MANAGER STATE
 * ============================================================================ */

#define MAX_PASSTHROUGH_DEVICES 16

struct cosmos_passthrough_mgr {
    // Device list
    passthrough_device  devices[MAX_PASSTHROUGH_DEVICES];
    uint32_t            num_devices;
    
    // VT-d State
    vtd_root_entry*     root_table;
    uint64_t            root_table_phys;
    
    // VT-d Register Base (from ACPI DMAR)
    uint64_t            dmar_base;
    
    // VT-d Capabilities
    uint8_t             has_vtd;
    uint8_t             vtd_enabled;
    uint8_t             vtd_ir_enabled;     // Interrupt Remapping
    uint32_t            vtd_sagaw;          // Supported Address Guest Width
    uint32_t            vtd_num_domains;    // Number of supported domains
    
    // EPT Pointer (for guest memory mapping)
    uint64_t            guest_ept_pml4_phys;
};

/* ============================================================================
 *  API DECLARATIONS
 * ============================================================================ */

struct cosmos_hypervisor;

// Phase 2.1: PCIe Device Enumeration & Passthrough
int  cosmos_pt_init(cosmos_passthrough_mgr* mgr);
int  cosmos_pt_scan_device(cosmos_passthrough_mgr* mgr, 
                           uint8_t bus, uint8_t dev, uint8_t func);
int  cosmos_pt_attach_device(cosmos_passthrough_mgr* mgr, 
                             cosmos_hypervisor* hv,
                             uint32_t device_index);
int  cosmos_pt_detach_device(cosmos_passthrough_mgr* mgr, uint32_t device_index);

// Phase 2.1: VT-d / DMA Remapping
int  cosmos_pt_init_vtd(cosmos_passthrough_mgr* mgr);
int  cosmos_pt_assign_device_dma(cosmos_passthrough_mgr* mgr,
                                 uint8_t bus, uint8_t dev, uint8_t func,
                                 uint64_t second_level_pml4_phys);

// Phase 2.1: MSI Interrupt Routing
int  cosmos_pt_setup_msi(cosmos_passthrough_mgr* mgr, 
                         passthrough_device* pdev,
                         uint8_t vector, uint8_t dest_cpu);

#endif // COSMOS_PASSTHROUGH_H
