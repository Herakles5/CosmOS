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
    // TODO: Implement virtio-blk logic here
    (void)hv;
    (void)vcpu;
    (void)port;
    (void)type;
    (void)size;
    (void)value;
    
    // Basic logic (will be fully extended in subsequent phases)
    VIRTIO_LOG("Access to VirtIO port 0x%04X (Type: %s, Size: %u)", 
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
    
    // VirtIO Block Features: We support LBA capacity (sector count) and block size
    dev->host_features = (1 << VIRTIO_BLK_F_BLK_SIZE);
    
    // PCI Configuration (fake values for our virtual PCI device)
    dev->pci_bus = 0;
    dev->pci_dev = 4; // Virtual device #4
    dev->pci_func = 0;
    
    // Assign free I/O port range to device
    dev->io_base = 0xC000;
    
    VIRTIO_LOG("VirtIO Block Device initialized.");
    VIRTIO_LOG("  Disk Size: %llu MB", (unsigned long long)(size_bytes / (1024*1024)));
    VIRTIO_LOG("  I/O Base:  0x%04X", dev->io_base);
    
    return 0;
}

int cosmos_virtio_blk_register_traps(cosmos_virtio_blk* dev, cosmos_io_manager* io_mgr) {
    if (!dev || !io_mgr) return -1;
    
    // Legacy VirtIO uses 32 bytes I/O ports
    // Register trap from io_base to io_base + 31
    int res = cosmos_io_register_port_trap(io_mgr, dev->io_base, dev->io_base + 0x1F, virtio_io_handler);
    if (res != 0) {
        VIRTIO_LOG("ERROR: Could not register I/O traps!");
        return res;
    }
    
    VIRTIO_LOG("I/O traps registered for VirtIO (0x%04X - 0x%04X).", 
               dev->io_base, dev->io_base + 0x1F);
    return 0;
}
