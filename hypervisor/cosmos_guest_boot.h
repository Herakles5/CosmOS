/*
 * ============================================================================
 *  COSMOS GUEST BOOT - Windows NT / OVMF Bootloader
 * ============================================================================
 *
 *  Phase 2.2: VM Preparation for Windows (UEFI Boot)
 *
 *  To boot Windows cleanly with GPU passthrough and high FPS,
 *  we utilize OVMF (Open Virtual Machine Firmware). OVMF initializes
 *  the PCIe bus, loads the UEFI GOP graphics driver, and launches
 *  the standard Windows bootloader (winload.efi).
 *
 *  Execution Flow:
 *  1. Load OVMF.fd firmware into guest physical memory (high address below 4GB).
 *  2. Configure VMCS for 16-bit Real Mode (Unrestricted Guest execution).
 *  3. Set Guest CS:RIP to standard x86 Reset Vector (0xFFFFFFF0).
 * ============================================================================
 */

#ifndef COSMOS_GUEST_BOOT_H
#define COSMOS_GUEST_BOOT_H

#include <stdint.h>
#include "cosmos_hypervisor.h"
#include "cosmos_ept.h"

// Reset Vector for x86 architecture (CPU entry point upon hardware reset)
#define X86_RESET_VECTOR        0xFFFFFFF0ULL

// Firmware Base (where firmware image is mapped in physical memory space)
// For a 2MB OVMF image, this maps to 0xFFE00000
#define OVMF_FIRMWARE_BASE      0xFFE00000ULL
#define OVMF_FIRMWARE_SIZE      (2 * 1024 * 1024)

// Boot Manager State
struct cosmos_boot_mgr {
    uint8_t*    firmware_data;
    uint64_t    firmware_size;
    uint64_t    firmware_phys;
    
    // Status
    uint8_t     is_ready;
};

// API
int cosmos_boot_init(cosmos_boot_mgr* mgr);
int cosmos_boot_load_ovmf(cosmos_boot_mgr* mgr, const char* filepath);
int cosmos_boot_setup_vmcs(cosmos_boot_mgr* mgr, cosmos_hypervisor* hv, uint32_t cpu_id);

#endif // COSMOS_GUEST_BOOT_H
