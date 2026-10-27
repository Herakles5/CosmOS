/*
 * ============================================================================
 *  COSMOS I/O TRAP MANAGER - Implementation
 * ============================================================================
 */

#include "cosmos_io_trap.h"

#ifdef __linux__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define IO_LOG(fmt, ...) printf("[IO_TRAP] " fmt "\n", ##__VA_ARGS__)

static void* io_alloc_page(uint64_t* out_phys) {
    void* p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    memset(p, 0, 4096);
    *out_phys = (uint64_t)p;
    return p;
}

#else
extern "C" int printf(const char* fmt, ...);
extern "C" void* memset(void*, int, unsigned long);
extern void* cosmos_alloc_page_aligned(uint64_t* out_phys);

#define IO_LOG(fmt, ...) printf("[IO_TRAP] " fmt "\n", ##__VA_ARGS__)

static void* io_alloc_page(uint64_t* out_phys) {
    return cosmos_alloc_page_aligned(out_phys);
}
#endif

/* ============================================================================
 *  INITIALIZATION
 * ============================================================================ */
int cosmos_io_init(cosmos_io_manager* io) {
    if (!io) return -1;
    memset(io, 0, sizeof(cosmos_io_manager));
    
    // Allocate I/O Bitmaps (4KB each)
    io->io_bitmap_a = (uint8_t*)io_alloc_page(&io->io_bitmap_a_phys);
    io->io_bitmap_b = (uint8_t*)io_alloc_page(&io->io_bitmap_b_phys);
    
    if (!io->io_bitmap_a || !io->io_bitmap_b) {
        IO_LOG("ERROR: Could not allocate I/O bitmaps!");
        return -2;
    }
    
    // Default: Allow all I/O ports (Bit = 0)
    // We set bits only for ports we actively want to intercept.
    // (Except for potentially dangerous ports like host reboot, which may
    // later be blocked globally).
    memset(io->io_bitmap_a, 0, 4096);
    memset(io->io_bitmap_b, 0, 4096);
    
    IO_LOG("I/O Manager initialized.");
    return 0;
}

/* ============================================================================
 *  REGISTRATION
 * ============================================================================ */
int cosmos_io_register_port_trap(cosmos_io_manager* io, uint16_t start, uint16_t end, io_port_handler_t handler) {
    if (!io || !handler || io->num_port_traps >= MAX_IO_TRAPS) return -1;
    
    io_port_trap* t = &io->port_traps[io->num_port_traps++];
    t->port_start = start;
    t->port_end = end;
    t->handler = handler;
    
    // Set bits in I/O bitmap to receive VM-Exits for these ports
    for (uint32_t port = start; port <= end; port++) {
        uint8_t* bitmap = (port < 0x8000) ? io->io_bitmap_a : io->io_bitmap_b;
        uint32_t offset = (port < 0x8000) ? port : (port - 0x8000);
        
        bitmap[offset / 8] |= (1 << (offset % 8));
    }
    
    IO_LOG("Port-Trap registered: 0x%04X - 0x%04X", start, end);
    return 0;
}

int cosmos_io_register_mmio_trap(cosmos_io_manager* io, uint64_t start, uint64_t end, mmio_handler_t handler) {
    if (!io || !handler || io->num_mmio_traps >= MAX_MMIO_TRAPS) return -1;
    
    mmio_trap* t = &io->mmio_traps[io->num_mmio_traps++];
    t->addr_start = start;
    t->addr_end = end;
    t->handler = handler;
    
    // Note: The actual "trap" is set by clearing the Present bit
    // or Read/Write bit in the EPT for these addresses.
    // The caller (e.g. EPT Manager) must do this separately!
    
    IO_LOG("MMIO-Trap registered: 0x%llX - 0x%llX", 
           (unsigned long long)start, (unsigned long long)end);
    return 0;
}

/* ============================================================================
 *  EXIT HANDLERS
 * ============================================================================ */

// Called on EXIT_REASON_IO_INSTRUCTION (Exit 30)
void cosmos_io_handle_port_access(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    // Decode Exit Qualification (Intel Vol. 3, 27.2.1)
    uint64_t qual = 0;
    cosmos_vmread(VMCS_EXIT_QUALIFICATION, &qual);
    
    uint8_t size_minus_1 = qual & 7;
    uint8_t size = size_minus_1 + 1;
    
    uint8_t direction = (qual >> 3) & 1; // 0 = OUT, 1 = IN
    uint8_t is_string = (qual >> 4) & 1; // String instruction (INS/OUTS)
    uint8_t rep       = (qual >> 5) & 1; // REP prefixed
    
    uint16_t port = (qual >> 16) & 0xFFFF;
    
    io_access_type type = direction ? IO_READ : IO_WRITE;
    
    if (is_string || rep) {
        IO_LOG("WARNING: String I/O (INS/OUTS) not supported yet! Port=0x%04X", port);
        // We would need to update RAX/RSI/RDI/RCX here...
        return;
    }
    
    // Value for OUT is in RAX
    uint32_t value = 0;
    if (type == IO_WRITE) {
        if (size == 1) value = vcpu->guest_regs.rax & 0xFF;
        else if (size == 2) value = vcpu->guest_regs.rax & 0xFFFF;
        else if (size == 4) value = vcpu->guest_regs.rax & 0xFFFFFFFF;
    }
    
    // Find handler
    // TODO: Global IO Manager pointer in hv struct!
    // For this example we assume it is in hv.
    // We ignore handlers for now and only log:
    
    // IO_LOG("I/O %s Port=0x%04X Size=%u Val=0x%X", 
    //        type == IO_READ ? "IN " : "OUT", port, size, value);
    
    // Write value for IN to RAX
    if (type == IO_READ) {
        // By default return "hardware absent" (0xFF)
        value = 0xFFFFFFFF;
        
        if (size == 1) {
            vcpu->guest_regs.rax = (vcpu->guest_regs.rax & ~0xFFULL) | (value & 0xFF);
        } else if (size == 2) {
            vcpu->guest_regs.rax = (vcpu->guest_regs.rax & ~0xFFFFULL) | (value & 0xFFFF);
        } else if (size == 4) {
            vcpu->guest_regs.rax = (vcpu->guest_regs.rax & ~0xFFFFFFFFULL) | (value & 0xFFFFFFFF);
        }
    }
}

// Called on EXIT_REASON_EPT_VIOLATION (Exit 48)
void cosmos_io_handle_mmio_access(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, uint64_t guest_phys) {
    // EPT Violation for MMIO Emulation
    // We need to decode the instruction to determine if it is READ or WRITE
    // and access size. This requires an x86 instruction decoder.
    
    IO_LOG("MMIO Trap at GPA: 0x%llX (Instruction must be decoded!)", 
           (unsigned long long)guest_phys);
           
    // Currently no decoder available. Only logging.
}
