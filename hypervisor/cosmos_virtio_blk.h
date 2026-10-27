/*
 * ============================================================================
 *  COSMOS VIRTIO BLOCK EMULATOR
 * ============================================================================
 *
 *  Phase 2.4: Virtuelle Festplatte (VirtIO-Blk)
 *
 *  Emuliert eine Festplatte nach dem VirtIO-Standard über den virtuellen PCI-Bus.
 *  OVMF bringt native Treiber für VirtIO-Blockgeräte mit, was das Booten
 *  von Windows extrem schnell und einfach macht.
 * ============================================================================
 */

#ifndef COSMOS_VIRTIO_BLK_H
#define COSMOS_VIRTIO_BLK_H

#include <stdint.h>
#include "cosmos_hypervisor.h"
#include "cosmos_io_trap.h"

// VirtIO PCI Vendor & Device IDs
#define VIRTIO_VENDOR_ID        0x1AF4
#define VIRTIO_BLK_DEVICE_ID    0x1001  // Legacy VirtIO Block Device
#define VIRTIO_BLK_SUBSYS_ID    0x0002

// VirtIO Block Feature Bits
#define VIRTIO_BLK_F_SIZE_MAX   1
#define VIRTIO_BLK_F_SEG_MAX    2
#define VIRTIO_BLK_F_GEOMETRY   4
#define VIRTIO_BLK_F_RO         5
#define VIRTIO_BLK_F_BLK_SIZE   6
#define VIRTIO_BLK_F_FLUSH      9
#define VIRTIO_BLK_F_TOPOLOGY   10

// Virtqueue Größen
#define VIRTIO_BLK_QUEUE_SIZE   128

// Virtqueue Strukturen (vring)
struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_BLK_QUEUE_SIZE];
    uint16_t used_event;
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem ring[VIRTIO_BLK_QUEUE_SIZE];
    uint16_t avail_event;
} __attribute__((packed));

// Block Request Headers
#define VIRTIO_BLK_T_IN           0
#define VIRTIO_BLK_T_OUT          1
#define VIRTIO_BLK_T_FLUSH        4

#define VIRTIO_BLK_S_OK           0
#define VIRTIO_BLK_S_IOERR        1
#define VIRTIO_BLK_S_UNSUPP       2

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

// Device State
struct cosmos_virtio_blk {
    // Backend File/Disk (Pointer zum RAM-Disk oder echten Dateihandle)
    uint8_t*  disk_image;
    uint64_t  disk_size_bytes;
    
    // VirtIO Status
    uint8_t   status;
    uint32_t  guest_features;
    uint32_t  host_features;
    
    // Queue State
    uint16_t  queue_select;
    uint32_t  queue_pfn; // Physical Page Number des vrings
    uint16_t  last_avail_idx;
    
    // Vring Pointers (werden berechnet wenn queue_pfn gesetzt wird)
    vring_desc*  desc;
    vring_avail* avail;
    vring_used*  used;
    
    // Virtuelle PCI Konfiguration
    uint8_t   pci_bus;
    uint8_t   pci_dev;
    uint8_t   pci_func;
    uint16_t  io_base;      // Port I/O Base (z.B. 0xC000)
    uint64_t  mmio_base;    // MMIO Base (z.B. 0xFEB00000)
};

// API
int cosmos_virtio_blk_init(cosmos_virtio_blk* dev, uint8_t* disk_buffer, uint64_t size_bytes);
int cosmos_virtio_blk_register_traps(cosmos_virtio_blk* dev, cosmos_io_manager* io_mgr);

#endif // COSMOS_VIRTIO_BLK_H
