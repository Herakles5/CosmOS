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
    
    // I/O Bitmaps allozieren (jeweils 4KB)
    io->io_bitmap_a = (uint8_t*)io_alloc_page(&io->io_bitmap_a_phys);
    io->io_bitmap_b = (uint8_t*)io_alloc_page(&io->io_bitmap_b_phys);
    
    if (!io->io_bitmap_a || !io->io_bitmap_b) {
        IO_LOG("FEHLER: Konnte I/O Bitmaps nicht allozieren!");
        return -2;
    }
    
    // Standard: Alle I/O Ports erlauben (Bit = 0)
    // Wir setzen Bits nur für Ports, die wir aktiv abfangen wollen.
    // (Ausgenommen ein paar gefährliche Ports wie z.B. Host-Reboot, die man
    // später evtl. global sperren sollte).
    memset(io->io_bitmap_a, 0, 4096);
    memset(io->io_bitmap_b, 0, 4096);
    
    IO_LOG("I/O Manager initialisiert.");
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
    
    // Bits in der I/O Bitmap setzen, damit wir VM-Exits für diese Ports bekommen
    for (uint32_t port = start; port <= end; port++) {
        uint8_t* bitmap = (port < 0x8000) ? io->io_bitmap_a : io->io_bitmap_b;
        uint32_t offset = (port < 0x8000) ? port : (port - 0x8000);
        
        bitmap[offset / 8] |= (1 << (offset % 8));
    }
    
    IO_LOG("Port-Trap registriert: 0x%04X - 0x%04X", start, end);
    return 0;
}

int cosmos_io_register_mmio_trap(cosmos_io_manager* io, uint64_t start, uint64_t end, mmio_handler_t handler) {
    if (!io || !handler || io->num_mmio_traps >= MAX_MMIO_TRAPS) return -1;
    
    mmio_trap* t = &io->mmio_traps[io->num_mmio_traps++];
    t->addr_start = start;
    t->addr_end = end;
    t->handler = handler;
    
    // Hinweis: Die eigentliche "Falle" wird durch das Löschen des Present-Bits
    // oder Read/Write-Bits in der EPT für diese Adressen gestellt.
    // Das muss der Aufrufer (z.B. EPT Manager) separat tun!
    
    IO_LOG("MMIO-Trap registriert: 0x%llX - 0x%llX", 
           (unsigned long long)start, (unsigned long long)end);
    return 0;
}

/* ============================================================================
 *  EXIT HANDLERS
 * ============================================================================ */

// Wird aufgerufen bei EXIT_REASON_IO_INSTRUCTION (Exit 30)
void cosmos_io_handle_port_access(cosmos_hypervisor* hv, cosmos_vcpu* vcpu) {
    // Exit Qualification dekodieren (Intel Vol. 3, 27.2.1)
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
        IO_LOG("WARNUNG: String I/O (INS/OUTS) wird noch nicht unterstützt! Port=0x%04X", port);
        // Wir müssen hier eigentlich RAX/RSI/RDI/RCX updaten...
        return;
    }
    
    // Wert für OUT liegt in RAX
    uint32_t value = 0;
    if (type == IO_WRITE) {
        if (size == 1) value = vcpu->guest_regs.rax & 0xFF;
        else if (size == 2) value = vcpu->guest_regs.rax & 0xFFFF;
        else if (size == 4) value = vcpu->guest_regs.rax & 0xFFFFFFFF;
    }
    
    // Handler suchen
    // TODO: Globaler IO Manager Pointer im hv struct!
    // Für dieses Beispiel gehen wir davon aus, dass wir ihn im hv haben.
    // Wir ignorieren Handler erstmal und loggen nur:
    
    // IO_LOG("I/O %s Port=0x%04X Size=%u Val=0x%X", 
    //        type == IO_READ ? "IN " : "OUT", port, size, value);
    
    // Wert für IN nach RAX schreiben
    if (type == IO_READ) {
        // Standardmäßig lesen wir "Hardware fehlt" (0xFF)
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

// Wird aufgerufen bei EXIT_REASON_EPT_VIOLATION (Exit 48)
void cosmos_io_handle_mmio_access(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, uint64_t guest_phys) {
    // EPT Violation für MMIO Emulation
    // Wir müssen die Instruktion dekodieren, um herauszufinden ob es ein READ oder WRITE ist
    // und wie groß der Zugriff ist. Das erfordert einen x86 Instruction Decoder.
    
    IO_LOG("MMIO Trap an GPA: 0x%llX (Instruktion muss dekodiert werden!)", 
           (unsigned long long)guest_phys);
           
    // Im Moment haben wir keinen Decoder. Wir loggen nur.
}
