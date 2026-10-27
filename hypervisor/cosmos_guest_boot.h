/*
 * ============================================================================
 *  COSMOS GUEST BOOT - Windows NT / OVMF Bootloader
 * ============================================================================
 *
 *  Phase 2.2: Vorbereitung der VM für Windows (UEFI Boot)
 *
 *  Um Windows vernünftig mit GPU-Passthrough und hohen FPS zu booten,
 *  benutzen wir OVMF (Open Virtual Machine Firmware). OVMF initialisiert
 *  den PCIe-Bus, lädt den UEFI-GOP Treiber der Grafikkarte und startet
 *  danach den normalen Windows Bootloader (winload.efi).
 *
 *  Ablauf:
 *  1. OVMF.fd (Firmware) in den Gast-Speicher laden (ganz oben unter 4GB)
 *  2. VMCS für 16-Bit Real Mode (Unrestricted Guest) konfigurieren
 *  3. Guest CS:RIP auf den x86 Reset Vector (0xFFFFFFF0) setzen
 * ============================================================================
 */

#ifndef COSMOS_GUEST_BOOT_H
#define COSMOS_GUEST_BOOT_H

#include <stdint.h>
#include "cosmos_hypervisor.h"
#include "cosmos_ept.h"

// Reset Vector für x86 (Hier startet die CPU nach einem Reset)
#define X86_RESET_VECTOR        0xFFFFFFF0ULL

// Firmware Base (Wo die Firmware im physischen Speicher eingeblendet wird)
// Bei einem 2MB OVMF Image ist das 0xFFE00000
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
