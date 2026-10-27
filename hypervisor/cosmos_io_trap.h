/*
 * ============================================================================
 *  COSMOS I/O TRAP MANAGER
 * ============================================================================
 *
 *  Phase 2.3: I/O Virtualisierung
 *
 *  Diese Komponente fängt Zugriffe des Windows-Gastes auf Hardware-Ports (IN/OUT)
 *  und Memory-Mapped I/O (MMIO via EPT-Violations) ab.
 *  Dadurch können wir virtuelle Geräte (wie Festplatten oder Netzwerkkarten)
 *  emulieren, ohne echte Hardware zu benötigen.
 * ============================================================================
 */

#ifndef COSMOS_IO_TRAP_H
#define COSMOS_IO_TRAP_H

#include <stdint.h>
#include "cosmos_hypervisor.h"

// Typ des I/O Zugriffs
enum io_access_type {
    IO_READ,
    IO_WRITE
};

// Callback für einen I/O Port Zugriff
typedef void (*io_port_handler_t)(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, 
                                  uint16_t port, io_access_type type, 
                                  uint8_t size, uint32_t* value);

// Callback für einen MMIO Zugriff
typedef void (*mmio_handler_t)(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, 
                               uint64_t phys_addr, io_access_type type, 
                               uint8_t size, uint64_t* value);

struct io_port_trap {
    uint16_t port_start;
    uint16_t port_end;
    io_port_handler_t handler;
};

struct mmio_trap {
    uint64_t addr_start;
    uint64_t addr_end;
    mmio_handler_t handler;
};

#define MAX_IO_TRAPS    64
#define MAX_MMIO_TRAPS  64

struct cosmos_io_manager {
    io_port_trap port_traps[MAX_IO_TRAPS];
    uint32_t num_port_traps;
    
    mmio_trap mmio_traps[MAX_MMIO_TRAPS];
    uint32_t num_mmio_traps;
    
    // I/O Bitmap (2 x 4KB Pages) für das VMCS
    // 1 Bit = 1 Port. Wenn Bit 1 -> VM-Exit bei IN/OUT
    uint8_t* io_bitmap_a; // Ports 0x0000 - 0x7FFF
    uint8_t* io_bitmap_b; // Ports 0x8000 - 0xFFFF
    uint64_t io_bitmap_a_phys;
    uint64_t io_bitmap_b_phys;
};

// API
int cosmos_io_init(cosmos_io_manager* io);
int cosmos_io_register_port_trap(cosmos_io_manager* io, uint16_t start, uint16_t end, io_port_handler_t handler);
int cosmos_io_register_mmio_trap(cosmos_io_manager* io, uint64_t start, uint64_t end, mmio_handler_t handler);

// Handler die vom VMX Exit Dispatcher aufgerufen werden
void cosmos_io_handle_port_access(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);
void cosmos_io_handle_mmio_access(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, uint64_t guest_phys);

#endif // COSMOS_IO_TRAP_H
