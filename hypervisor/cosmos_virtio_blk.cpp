/*
 * ============================================================================
 *  COSMOS VIRTIO BLOCK EMULATOR - Implementation
 * ============================================================================
 */

#include "cosmos_virtio_blk.h"

#ifdef __linux__
#include <stdio.h>
#include <string.h>
#define VIRTIO_LOG(fmt, ...) printf("[VIRTIO] " fmt "\n", ##__VA_ARGS__)
#else
extern "C" int printf(const char* fmt, ...);
extern "C" void* memset(void*, int, unsigned long);
#define VIRTIO_LOG(fmt, ...) printf("[VIRTIO] " fmt "\n", ##__VA_ARGS__)
#endif

// VirtIO Register Offsets (Legacy)
#define VIRTIO_PCI_HOST_FEATURES        0x00
#define VIRTIO_PCI_GUEST_FEATURES       0x04
#define VIRTIO_PCI_QUEUE_PFN            0x08
#define VIRTIO_PCI_QUEUE_NUM            0x0C
#define VIRTIO_PCI_QUEUE_SEL            0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY         0x10
#define VIRTIO_PCI_STATUS               0x12
#define VIRTIO_PCI_ISR                  0x13
#define VIRTIO_PCI_CONFIG               0x14

/* ============================================================================
 *  TRAP HANDLER
 * ============================================================================ */
static void virtio_io_handler(cosmos_hypervisor* hv, cosmos_vcpu* vcpu, 
                              uint16_t port, io_access_type type, 
                              uint8_t size, uint32_t* value) {
    //TODO: Implement virtio-blk logic here
    (void)hv;
    (void)vcpu;
    (void)port;
    (void)type;
    (void)size;
    (void)value;
    
    // Einfache Logik (wird in späteren Phasen komplett ausgebaut)
    VIRTIO_LOG("Zugriff auf VirtIO Port 0x%04X (Typ: %s, Size: %u)", 
               port, type == IO_READ ? "IN" : "OUT", size);
}

/* ============================================================================
 *  INIT & REGISTRATION
 * ============================================================================ */
int cosmos_virtio_blk_init(cosmos_virtio_blk* dev, uint8_t* disk_buffer, uint64_t size_bytes) {
    if (!dev) return -1;
    
    memset(dev, 0, sizeof(cosmos_virtio_blk));
    dev->disk_image = disk_buffer;
    dev->disk_size_bytes = size_bytes;
    
    // VirtIO Block Features: Wir unterstützen LBA Capacity (Sektorenanzahl) und Blockgröße
    dev->host_features = (1 << VIRTIO_BLK_F_BLK_SIZE);
    
    // PCI-Konfiguration (Fake-Werte für unser virtuelles PCI-Gerät)
    dev->pci_bus = 0;
    dev->pci_dev = 4; // Virtuelles Gerät #4
    dev->pci_func = 0;
    
    // Weisen dem Gerät einen freien I/O Port-Bereich zu
    dev->io_base = 0xC000;
    
    VIRTIO_LOG("VirtIO Block Device initialisiert.");
    VIRTIO_LOG("  Disk Size: %llu MB", (unsigned long long)(size_bytes / (1024*1024)));
    VIRTIO_LOG("  I/O Base:  0x%04X", dev->io_base);
    
    return 0;
}

int cosmos_virtio_blk_register_traps(cosmos_virtio_blk* dev, cosmos_io_manager* io_mgr) {
    if (!dev || !io_mgr) return -1;
    
    // Legacy VirtIO nutzt 32 Bytes I/O Ports
    // Wir registrieren einen Trap von io_base bis io_base + 31
    int res = cosmos_io_register_port_trap(io_mgr, dev->io_base, dev->io_base + 0x1F, virtio_io_handler);
    if (res != 0) {
        VIRTIO_LOG("FEHLER: Konnte I/O Traps nicht registrieren!");
        return res;
    }
    
    VIRTIO_LOG("I/O Traps für VirtIO registriert (0x%04X - 0x%04X).", 
               dev->io_base, dev->io_base + 0x1F);
    return 0;
}
