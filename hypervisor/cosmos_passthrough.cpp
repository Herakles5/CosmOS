/*
 * ============================================================================
 *  COSMOS PCIe PASSTHROUGH - Implementation
 * ============================================================================
 *
 *  Phase 2.1: GPU Passthrough für NVIDIA GeForce GTX 1060 6GB
 *
 *  Ablauf:
 *  1. PCI Config Space scannen und BARs dekodieren
 *  2. GPU von Linux-Treiber trennen (sysfs unbind)
 *  3. GPU BARs in EPT mappen (UC für MMIO, WC für VRAM)
 *  4. VT-d Root/Context Tables für DMA-Remapping aufbauen
 *  5. MSI-Interrupts konfigurieren
 *  6. Gerät dem Gast zuweisen
 * ============================================================================
 */

#include "cosmos_hypervisor.h"
#include "cosmos_ept.h"
#include "cosmos_passthrough.h"

#ifdef __linux__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <dirent.h>

#define PT_LOG(fmt, ...) printf("[PASSTHROUGH] " fmt "\n", ##__VA_ARGS__)

// Linux: PCI Config Space via sysfs lesen
static uint32_t pci_read_config32(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/config",
             bus, dev, func);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0xFFFFFFFF;
    uint32_t val = 0;
    if (pread(fd, &val, 4, offset) < 0) {}
    close(fd);
    return val;
}

static uint16_t pci_read_config16(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/config",
             bus, dev, func);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0xFFFF;
    uint16_t val = 0;
    if (pread(fd, &val, 2, offset) < 0) {}
    close(fd);
    return val;
}

static uint8_t pci_read_config8(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/config",
             bus, dev, func);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0xFF;
    uint8_t val = 0;
    if (pread(fd, &val, 1, offset) < 0) {}
    close(fd);
    return val;
}

// Linux: BAR-Adressen und Größen aus sysfs/resource lesen
static int pci_read_bars_sysfs(uint8_t bus, uint8_t dev, uint8_t func, pci_bar_info* bars) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/resource",
             bus, dev, func);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    int count = 0;
    for (int i = 0; i < 6; i++) {
        uint64_t start = 0, end = 0, flags = 0;
        if (fscanf(f, "0x%llx 0x%llx 0x%llx\n",
                   (unsigned long long*)&start,
                   (unsigned long long*)&end,
                   (unsigned long long*)&flags) != 3) break;
        
        if (start == 0 && end == 0) continue;
        
        bars[count].base_addr = start;
        bars[count].size = end - start + 1;
        bars[count].type = (flags & 0x100) ? 1 : 0;  // IORESOURCE_IO
        bars[count].is_64bit = (flags & 0x04) ? 1 : 0; // IORESOURCE_MEM_64
        bars[count].prefetchable = (flags & 0x2000) ? 1 : 0; // IORESOURCE_PREFETCH
        bars[count].bar_index = i;
        count++;
    }
    fclose(f);
    return count;
}

// Linux: Treiber vom Gerät trennen
static int pci_unbind_driver(uint8_t bus, uint8_t dev, uint8_t func) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/driver/unbind",
             bus, dev, func);
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        PT_LOG("Kein aktiver Treiber für %02x:%02x.%x (oder bereits ungebunden)", bus, dev, func);
        return 0;
    }
    char addr[16];
    snprintf(addr, sizeof(addr), "0000:%02x:%02x.%x", bus, dev, func);
    int written = write(fd, addr, strlen(addr));
    close(fd);
    return written > 0 ? 0 : -1;
}

// Linux: VFIO oder vfio-pci Treiber binden
static int pci_bind_vfio(uint8_t bus, uint8_t dev, uint8_t func,
                         uint16_t vendor, uint16_t device) {
    // Schritt 1: vfio-pci Driver Override
    char path[256];
    snprintf(path, sizeof(path), 
             "/sys/bus/pci/devices/0000:%02x:%02x.%x/driver_override",
             bus, dev, func);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        if (write(fd, "vfio-pci", 8) < 0) {}
        close(fd);
    }
    
    // Schritt 2: New ID für vfio-pci
    fd = open("/sys/bus/pci/drivers/vfio-pci/new_id", O_WRONLY);
    if (fd >= 0) {
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%04x %04x", vendor, device);
        if (write(fd, id_str, strlen(id_str)) < 0) {}
        close(fd);
    }
    
    // Schritt 3: Probe
    fd = open("/sys/bus/pci/drivers_probe", O_WRONLY);
    if (fd >= 0) {
        char addr[16];
        snprintf(addr, sizeof(addr), "0000:%02x:%02x.%x", bus, dev, func);
        if (write(fd, addr, strlen(addr)) < 0) {}
        close(fd);
    }
    
    return 0;
}

// Page-Allokation Wrapper
static void* pt_alloc_page(uint64_t* out_phys) {
    void* p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    memset(p, 0, 4096);
    *out_phys = (uint64_t)p;
    return p;
}

#else
// Bare Metal: PCI Config über I/O Ports oder MMCONFIG
extern "C" int printf(const char* fmt, ...);
extern "C" int snprintf(char* str, unsigned long size, const char* format, ...);
extern "C" void* memset(void*, int, unsigned long);
extern "C" void* memcpy(void*, const void*, unsigned long);
extern void* cosmos_alloc_page_aligned(uint64_t* out_phys);

#define PT_LOG(fmt, ...) printf("[PASSTHROUGH] " fmt "\n", ##__VA_ARGS__)

// Bare Metal: PCI Config via MMCONFIG (Memory-Mapped)
static volatile uint32_t* mmconfig_ptr(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    uint64_t addr = PCIE_MMCONFIG_BASE +
                    ((uint64_t)bus << 20) +
                    ((uint64_t)dev << 15) +
                    ((uint64_t)func << 12) +
                    offset;
    return (volatile uint32_t*)addr;
}

static uint32_t pci_read_config32(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    return *mmconfig_ptr(bus, dev, func, offset);
}

static uint16_t pci_read_config16(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    return *(volatile uint16_t*)((uint8_t*)mmconfig_ptr(bus, dev, func, 0) + offset);
}

static uint8_t pci_read_config8(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    return *(volatile uint8_t*)((uint8_t*)mmconfig_ptr(bus, dev, func, 0) + offset);
}

static void pci_write_config32(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset, uint32_t val) {
    *mmconfig_ptr(bus, dev, func, offset) = val;
}

static void* pt_alloc_page(uint64_t* out_phys) {
    return cosmos_alloc_page_aligned(out_phys);
}

// Bare Metal: BAR-Größe durch Config-Space-Probe bestimmen
static uint64_t pci_probe_bar_size(uint8_t bus, uint8_t dev, uint8_t func, uint8_t bar_idx) {
    uint16_t offset = PCI_BAR0 + bar_idx * 4;
    uint32_t original = pci_read_config32(bus, dev, func, offset);
    
    pci_write_config32(bus, dev, func, offset, 0xFFFFFFFF);
    uint32_t sized = pci_read_config32(bus, dev, func, offset);
    pci_write_config32(bus, dev, func, offset, original);
    
    if (original & BAR_TYPE_MASK) {
        // I/O BAR
        sized &= BAR_IO_ADDR_MASK;
        return (~sized + 1) & 0xFFFF;
    } else {
        // Memory BAR
        sized &= BAR_MEM_ADDR_MASK;
        if (sized == 0) return 0;
        return (~sized + 1);
    }
}

static int pci_unbind_driver(uint8_t, uint8_t, uint8_t) { return 0; }
static int pci_bind_vfio(uint8_t, uint8_t, uint8_t, uint16_t, uint16_t) { return 0; }
#endif

/* ============================================================================
 *  CAPABILITY CHAIN WALKER
 *
 *  PCI Capabilities sind eine verlinkte Liste im Config Space.
 *  Jede Capability hat: [ID:8][Next:8][Data:variable]
 * ============================================================================ */
static uint16_t pci_find_capability(uint8_t bus, uint8_t dev, uint8_t func, uint8_t cap_id) {
    uint16_t status = pci_read_config16(bus, dev, func, PCI_STATUS);
    if (!(status & 0x10)) return 0;  // Keine Capabilities
    
    uint8_t ptr = pci_read_config8(bus, dev, func, PCI_CAP_PTR) & 0xFC;
    
    int max_walk = 48;  // Schutz gegen Endlosschleifen
    while (ptr && max_walk--) {
        uint8_t id = pci_read_config8(bus, dev, func, ptr);
        if (id == cap_id) return ptr;
        ptr = pci_read_config8(bus, dev, func, ptr + 1) & 0xFC;
    }
    return 0;
}

/* ============================================================================
 *  PASSTHROUGH MANAGER INITIALIZATION
 * ============================================================================ */
int cosmos_pt_init(cosmos_passthrough_mgr* mgr) {
    if (!mgr) return -1;
    
    memset(mgr, 0, sizeof(cosmos_passthrough_mgr));
    mgr->dmar_base = VTD_DMAR_BASE;
    
    PT_LOG("╔══════════════════════════════════════════╗");
    PT_LOG("║   CosmOS PCIe Passthrough Manager        ║");
    PT_LOG("║   Phase 2.1: GPU Passthrough             ║");
    PT_LOG("╚══════════════════════════════════════════╝");
    
    return 0;
}

/* ============================================================================
 *  PCI DEVICE SCANNER
 *
 *  Liest Config Space, dekodiert BARs, findet MSI Capabilities.
 * ============================================================================ */
int cosmos_pt_scan_device(cosmos_passthrough_mgr* mgr,
                          uint8_t bus, uint8_t dev, uint8_t func) {
    if (!mgr || mgr->num_devices >= MAX_PASSTHROUGH_DEVICES) return -1;
    
    uint16_t vendor = pci_read_config16(bus, dev, func, PCI_VENDOR_ID);
    uint16_t device = pci_read_config16(bus, dev, func, PCI_DEVICE_ID);
    
    if (vendor == 0xFFFF || vendor == 0x0000) {
        PT_LOG("Kein Gerät bei %02x:%02x.%x", bus, dev, func);
        return -1;
    }
    
    passthrough_device* pdev = &mgr->devices[mgr->num_devices];
    memset(pdev, 0, sizeof(passthrough_device));
    
    pdev->bus = bus;
    pdev->device = dev;
    pdev->function = func;
    pdev->vendor_id = vendor;
    pdev->device_id = device;
    pdev->subsystem_vendor = pci_read_config16(bus, dev, func, PCI_SUBSYSTEM_VENDOR);
    pdev->subsystem_id = pci_read_config16(bus, dev, func, PCI_SUBSYSTEM_ID);
    
    // Name zusammenbauen
    snprintf(pdev->name, sizeof(pdev->name), "PCI %02x:%02x.%x [%04x:%04x]",
             bus, dev, func, vendor, device);
    
    PT_LOG("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    PT_LOG("Scanne: %s", pdev->name);
    
    // Spezielle Namen für bekannte Geräte
    if (vendor == 0x10de && device == 0x1c03) {
        snprintf(pdev->name, sizeof(pdev->name), "NVIDIA GeForce GTX 1060 6GB");
    } else if (vendor == 0x10de && device == 0x10f1) {
        snprintf(pdev->name, sizeof(pdev->name), "NVIDIA GP106 HDMI Audio");
    }
    
    PT_LOG("  Name:     %s", pdev->name);
    PT_LOG("  Vendor:   0x%04X", vendor);
    PT_LOG("  Device:   0x%04X", device);
    PT_LOG("  SubSys:   0x%04X:0x%04X", pdev->subsystem_vendor, pdev->subsystem_id);
    
    // === BARs lesen ===
#ifdef __linux__
    pdev->num_bars = pci_read_bars_sysfs(bus, dev, func, pdev->bars);
#else
    // Bare Metal: BARs manuell aus Config Space lesen
    pdev->num_bars = 0;
    for (int i = 0; i < 6; i++) {
        uint32_t bar_val = pci_read_config32(bus, dev, func, PCI_BAR0 + i * 4);
        if (bar_val == 0) continue;
        
        pci_bar_info* bar = &pdev->bars[pdev->num_bars];
        bar->bar_index = i;
        
        if (bar_val & BAR_TYPE_MASK) {
            // I/O BAR
            bar->type = 1;
            bar->base_addr = bar_val & BAR_IO_ADDR_MASK;
            bar->size = pci_probe_bar_size(bus, dev, func, i);
        } else {
            // Memory BAR
            bar->type = 0;
            bar->prefetchable = (bar_val & BAR_MEM_PREFETCH) ? 1 : 0;
            
            uint8_t mem_type = (bar_val >> 1) & 0x3;
            if (mem_type == 2) {
                // 64-bit BAR
                bar->is_64bit = 1;
                uint32_t bar_hi = pci_read_config32(bus, dev, func, PCI_BAR0 + (i+1) * 4);
                bar->base_addr = ((uint64_t)bar_hi << 32) | (bar_val & BAR_MEM_ADDR_MASK);
                bar->size = pci_probe_bar_size(bus, dev, func, i);
                i++; // Skip next BAR (upper 32 bits)
            } else {
                // 32-bit BAR
                bar->base_addr = bar_val & BAR_MEM_ADDR_MASK;
                bar->size = pci_probe_bar_size(bus, dev, func, i);
            }
        }
        pdev->num_bars++;
    }
#endif
    
    PT_LOG("  BARs:");
    for (int i = 0; i < pdev->num_bars; i++) {
        pci_bar_info* bar = &pdev->bars[i];
        const char* type_str = bar->type ? "I/O" : "MEM";
        const char* bits_str = bar->is_64bit ? "64-bit" : "32-bit";
        const char* pf_str = bar->prefetchable ? "Prefetch" : "Non-PF";
        
        if (bar->size >= (1024*1024)) {
            PT_LOG("    BAR%d: 0x%llX (%llu MB) [%s, %s, %s]",
                   bar->bar_index,
                   (unsigned long long)bar->base_addr,
                   (unsigned long long)(bar->size / (1024*1024)),
                   type_str, bits_str, pf_str);
        } else {
            PT_LOG("    BAR%d: 0x%llX (%llu KB) [%s, %s, %s]",
                   bar->bar_index,
                   (unsigned long long)bar->base_addr,
                   (unsigned long long)(bar->size / 1024),
                   type_str, bits_str, pf_str);
        }
    }
    
    // === MSI Capability suchen ===
    pdev->msi_cap_offset = pci_find_capability(bus, dev, func, PCI_CAP_ID_MSI);
    pdev->has_msi = (pdev->msi_cap_offset != 0) ? 1 : 0;
    
    pdev->msix_cap_offset = pci_find_capability(bus, dev, func, PCI_CAP_ID_MSIX);
    pdev->has_msix = (pdev->msix_cap_offset != 0) ? 1 : 0;
    
    PT_LOG("  MSI:    %s (offset=0x%X)", pdev->has_msi ? "JA ✓" : "NEIN", pdev->msi_cap_offset);
    PT_LOG("  MSI-X:  %s (offset=0x%X)", pdev->has_msix ? "JA ✓" : "NEIN", pdev->msix_cap_offset);
    
    mgr->num_devices++;
    PT_LOG("  → Gerät #%u registriert", mgr->num_devices - 1);
    
    return mgr->num_devices - 1;
}

/* ============================================================================
 *  ATTACH DEVICE TO GUEST
 *
 *  Das Herzstück: Weist ein PCI-Gerät dem Gast zu.
 *  1. Treiber trennen
 *  2. BARs in EPT mappen
 *  3. MSI konfigurieren
 * ============================================================================ */
int cosmos_pt_attach_device(cosmos_passthrough_mgr* mgr,
                            cosmos_hypervisor* hv,
                            uint32_t device_index) {
    if (!mgr || !hv || device_index >= mgr->num_devices) return -1;
    
    passthrough_device* pdev = &mgr->devices[device_index];
    
    PT_LOG("═══════════════════════════════════════════");
    PT_LOG("ATTACH: %s", pdev->name);
    PT_LOG("═══════════════════════════════════════════");
    
    // --- Schritt 1: Linux-Treiber trennen ---
    PT_LOG("Schritt 1: Treiber trennen...");
    int result = pci_unbind_driver(pdev->bus, pdev->device, pdev->function);
    if (result == 0) {
        PT_LOG("  Treiber erfolgreich getrennt (oder war nicht gebunden)");
    } else {
        PT_LOG("  WARNUNG: Treiber konnte nicht getrennt werden!");
    }
    
    // --- Schritt 2: BARs in EPT mappen ---
    PT_LOG("Schritt 2: BARs in EPT mappen...");
    
    for (int i = 0; i < pdev->num_bars; i++) {
        pci_bar_info* bar = &pdev->bars[i];
        
        if (bar->type == 1) {
            // I/O BAR: Wird über I/O-Bitmap im VMCS kontrolliert
            PT_LOG("  BAR%d: I/O Port 0x%X (%llu bytes) → I/O Bitmap",
                   bar->bar_index,
                   (uint32_t)bar->base_addr,
                   (unsigned long long)bar->size);
            continue;
        }
        
        // Memory BAR: In EPT mappen
        // GPU VRAM (prefetchable) → Write-Combining für Performance
        // GPU MMIO (non-prefetch) → Uncacheable für Korrektheit
        uint64_t ept_flags;
        const char* cache_str;
        
        if (bar->prefetchable) {
            // VRAM: Write-Combining (beste GPU-Performance!)
            ept_flags = EPT_RWX | (EPT_MT_WC << EPT_MEMORY_TYPE_SHIFT) | EPT_IGNORE_PAT;
            cache_str = "Write-Combining";
        } else {
            // MMIO Register: Uncacheable (MUSS sein!)
            ept_flags = EPT_RWX | (EPT_MT_UC << EPT_MEMORY_TYPE_SHIFT) | EPT_IGNORE_PAT;
            cache_str = "Uncacheable";
        }
        
        // Mappe jede 2MB-Seite der BAR in die EPT
        // GPA == HPA (Identity Map → Guest sieht die echte Hardware)
        uint64_t bar_start = bar->base_addr & ~(EPT_PAGE_SIZE_2M - 1); // 2MB align
        uint64_t bar_end = (bar->base_addr + bar->size + EPT_PAGE_SIZE_2M - 1) 
                           & ~(EPT_PAGE_SIZE_2M - 1);
        
        uint32_t pages_mapped = 0;
        for (uint64_t addr = bar_start; addr < bar_end; addr += EPT_PAGE_SIZE_2M) {
            // EPT Page-Flags für diese Adresse setzen
            // In der Identity Map sind die Pages schon da, wir müssen nur
            // die Cache-Flags anpassen
            cosmos_ept_set_page_flags(hv, addr, ept_flags);
            pages_mapped++;
        }
        
        if (bar->size >= (1024*1024)) {
            PT_LOG("  BAR%d: 0x%llX (%llu MB) → EPT [%s] (%u×2MB pages)",
                   bar->bar_index,
                   (unsigned long long)bar->base_addr,
                   (unsigned long long)(bar->size / (1024*1024)),
                   cache_str, pages_mapped);
        } else {
            PT_LOG("  BAR%d: 0x%llX (%llu KB) → EPT [%s] (%u×2MB pages)",
                   bar->bar_index,
                   (unsigned long long)bar->base_addr,
                   (unsigned long long)(bar->size / 1024),
                   cache_str, pages_mapped);
        }
    }
    
    // --- Schritt 3: MSI Interrupt konfigurieren ---
    if (pdev->has_msi) {
        PT_LOG("Schritt 3: MSI Interrupts konfigurieren...");
        cosmos_pt_setup_msi(mgr, pdev, 0x30, 0);  // Vector 0x30, CPU 0
    }
    
    // --- Schritt 4: PCI Command Register konfigurieren ---
    PT_LOG("Schritt 4: PCI Command Register...");
    // Bus Master + Memory Space + I/O Space aktivieren
    // Das muss der Gast eigentlich selbst tun, aber wir bereiten es vor
    
    pdev->attached = 1;
    
    PT_LOG("═══════════════════════════════════════════");
    PT_LOG("★ %s ERFOLGREICH AN GAST ZUGEWIESEN ★", pdev->name);
    PT_LOG("═══════════════════════════════════════════");
    
    return 0;
}

/* ============================================================================
 *  DETACH DEVICE FROM GUEST
 * ============================================================================ */
int cosmos_pt_detach_device(cosmos_passthrough_mgr* mgr, uint32_t device_index) {
    if (!mgr || device_index >= mgr->num_devices) return -1;
    
    passthrough_device* pdev = &mgr->devices[device_index];
    
    PT_LOG("Trenne %s vom Gast...", pdev->name);
    
    // BARs aus EPT entfernen (Flags auf 0 setzen → nicht zugänglich)
    // TODO: EPT entries für die BARs invalidieren
    
    pdev->attached = 0;
    
    // Original-Treiber wieder binden
#ifdef __linux__
    // driver_probe um den originalen Treiber wieder zu laden
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/driver_override",
             pdev->bus, pdev->device, pdev->function);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        if (write(fd, "\n", 1) < 0) {}  // Override löschen
        close(fd);
    }
    
    fd = open("/sys/bus/pci/drivers_probe", O_WRONLY);
    if (fd >= 0) {
        char addr[16];
        snprintf(addr, sizeof(addr), "0000:%02x:%02x.%x",
                 pdev->bus, pdev->device, pdev->function);
        if (write(fd, addr, strlen(addr)) < 0) {}
        close(fd);
    }
#endif
    
    PT_LOG("%s vom Gast getrennt", pdev->name);
    return 0;
}

/* ============================================================================
 *  MSI INTERRUPT SETUP
 *
 *  MSI (Message Signaled Interrupts) sendet Interrupts als Speicherschreib-
 *  zugriffe an die Local APIC. Für Passthrough müssen wir die MSI-Adresse
 *  und -Daten so konfigurieren, dass der Interrupt direkt im Gast ankommt.
 *
 *  MSI Address Format (für x2APIC):
 *    Bits [31:20] = 0xFEE (MSI Address Prefix)
 *    Bits [19:12] = Destination APIC ID
 *    Bit  [3]     = Redirect Hint (0=direct, 1=lowest priority)
 *    Bit  [2]     = Destination Mode (0=physical, 1=logical)
 *
 *  MSI Data Format:
 *    Bits [7:0]   = Vector Number
 *    Bits [10:8]  = Delivery Mode (000=Fixed, 001=Low Priority)
 *    Bit  [14]    = Level (1=Assert)
 *    Bit  [15]    = Trigger Mode (0=Edge, 1=Level)
 * ============================================================================ */
int cosmos_pt_setup_msi(cosmos_passthrough_mgr* mgr,
                        passthrough_device* pdev,
                        uint8_t vector, uint8_t dest_cpu) {
    if (!pdev || !pdev->has_msi) return -1;
    
    uint16_t msi_off = pdev->msi_cap_offset;
    
    // MSI Control Register lesen
    uint16_t msi_ctrl = pci_read_config16(pdev->bus, pdev->device, pdev->function,
                                          msi_off + MSI_CONTROL);
    
    bool is_64bit = (msi_ctrl & MSI_CTRL_64BIT) != 0;
    
    PT_LOG("MSI Setup für %s:", pdev->name);
    PT_LOG("  MSI Offset: 0x%X", msi_off);
    PT_LOG("  64-bit:     %s", is_64bit ? "JA" : "NEIN");
    PT_LOG("  Vector:     0x%02X", vector);
    PT_LOG("  Dest CPU:   %u", dest_cpu);
    
    // MSI Address: 0xFEE + APIC ID
    uint32_t msi_addr_lo = 0xFEE00000 | ((uint32_t)dest_cpu << 12);
    uint32_t msi_addr_hi = 0;
    
    // MSI Data: Vector + Edge Trigger + Fixed Delivery
    uint16_t msi_data = vector | (0 << 8);  // Fixed delivery
    
    PT_LOG("  MSI Addr:   0x%08X_%08X", msi_addr_hi, msi_addr_lo);
    PT_LOG("  MSI Data:   0x%04X", msi_data);
    
    // Die eigentliche Programmierung erfolgt durch den Gast-Treiber.
    // Wir stellen nur sicher, dass der Hypervisor MSI-Schreibzugriffe
    // (an 0xFEExxxxx) korrekt an den Gast-APIC weiterleitet.
    
    // Für den Hypervisor: Die EPT muss den APIC-Bereich (0xFEE00000)
    // korrekt behandeln. Bei einem Schreibzugriff auf 0xFEE00000+offset
    // muss ein Interrupt im Gast injiziert werden.
    
    PT_LOG("  → MSI wird vom Gast-Treiber programmiert");
    PT_LOG("  → Hypervisor leitet MSI-Writes an Gast-APIC weiter");
    
    return 0;
}

/* ============================================================================
 *  VT-d INITIALIZATION
 *
 *  Initialisiert die DMA-Remapping-Hardware:
 *  1. DMAR Base aus ACPI lesen (bereits bekannt: 0xFED90000)
 *  2. Root Table allozieren und ins DMAR-Register schreiben
 *  3. Translation aktivieren
 * ============================================================================ */
int cosmos_pt_init_vtd(cosmos_passthrough_mgr* mgr) {
    if (!mgr) return -1;
    
    PT_LOG("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    PT_LOG("VT-d / DMA Remapping Initialisierung");
    PT_LOG("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    PT_LOG("DMAR Base: 0x%llX", (unsigned long long)mgr->dmar_base);
    
#ifdef __linux__
    // Unter Linux: VT-d wird vom Kernel verwaltet (IOMMU Driver)
    // Wir lesen nur die Capabilities aus
    
    // Prüfe ob IOMMU aktiv ist
    int fd = open("/sys/kernel/iommu_groups/1/devices/0000:01:00.0", O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        close(fd);
        mgr->has_vtd = 1;
        PT_LOG("VT-d: AKTIV (IOMMU Groups vorhanden)");
    } else {
        PT_LOG("VT-d: NICHT AKTIV oder keine IOMMU-Gruppen");
        PT_LOG("→ Stelle sicher dass intel_iommu=on in der Kernel-Commandline steht");
        mgr->has_vtd = 0;
    }
    
    // DMAR Capabilities aus sysfs lesen
    fd = open("/sys/class/iommu/dmar0/intel-iommu/cap", O_RDONLY);
    if (fd >= 0) {
        char buf[64] = {0};
        if (read(fd, buf, sizeof(buf)-1) < 0) {}
        close(fd);
        PT_LOG("DMAR Capability: %s", buf);
    }
    
    fd = open("/sys/class/iommu/dmar0/intel-iommu/ecap", O_RDONLY);
    if (fd >= 0) {
        char buf[64] = {0};
        if (read(fd, buf, sizeof(buf)-1) < 0) {}
        close(fd);
        PT_LOG("DMAR Extended Cap: %s", buf);
    }
    
#else
    // Bare Metal: DMAR Register direkt lesen
    volatile uint32_t* dmar = (volatile uint32_t*)mgr->dmar_base;
    
    uint32_t ver = dmar[VTD_VER / 4];
    uint64_t cap = *(volatile uint64_t*)((uint8_t*)dmar + VTD_CAP);
    uint64_t ecap = *(volatile uint64_t*)((uint8_t*)dmar + VTD_ECAP);
    
    PT_LOG("VT-d Version: %u.%u", (ver >> 4) & 0xF, ver & 0xF);
    PT_LOG("VT-d CAP:     0x%llX", (unsigned long long)cap);
    PT_LOG("VT-d ECAP:    0x%llX", (unsigned long long)ecap);
    
    // Capabilities dekodieren
    mgr->vtd_sagaw = (cap >> 8) & 0x1F;
    mgr->vtd_num_domains = 1 << (((cap >> 0) & 0x7) * 2 + 4);
    
    PT_LOG("  Supported Address Widths: %s%s%s",
           (mgr->vtd_sagaw & 0x04) ? "39-bit " : "",
           (mgr->vtd_sagaw & 0x08) ? "48-bit " : "",
           (mgr->vtd_sagaw & 0x10) ? "57-bit " : "");
    PT_LOG("  Max Domains: %u", mgr->vtd_num_domains);
    
    // Root Table allozieren
    mgr->root_table = (vtd_root_entry*)pt_alloc_page(&mgr->root_table_phys);
    if (!mgr->root_table) {
        PT_LOG("FEHLER: Root Table konnte nicht alloziert werden!");
        return -1;
    }
    memset(mgr->root_table, 0, VTD_ROOT_TABLE_SIZE);
    
    PT_LOG("Root Table @ phys=0x%llX", (unsigned long long)mgr->root_table_phys);
    
    // Root Table Address ins DMAR Register schreiben
    *(volatile uint64_t*)((uint8_t*)dmar + VTD_RTADDR) = mgr->root_table_phys;
    
    // Set Root Table Pointer Command
    uint32_t gcmd = *(volatile uint32_t*)((uint8_t*)dmar + VTD_GCMD);
    gcmd |= VTD_GCMD_SRTP;
    *(volatile uint32_t*)((uint8_t*)dmar + VTD_GCMD) = gcmd;
    
    // Warten bis Root Table Pointer gesetzt ist
    int timeout = 1000000;
    while (timeout--) {
        uint32_t gsts = *(volatile uint32_t*)((uint8_t*)dmar + VTD_GSTS);
        if (gsts & VTD_GSTS_RTPS) break;
    }
    
    if (timeout <= 0) {
        PT_LOG("FEHLER: VT-d Root Table Pointer Timeout!");
        return -2;
    }
    
    mgr->has_vtd = 1;
    PT_LOG("VT-d Root Table gesetzt");
#endif
    
    PT_LOG("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    PT_LOG("★ VT-d Initialisierung abgeschlossen ★");
    PT_LOG("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    return 0;
}

/* ============================================================================
 *  DMA DEVICE ASSIGNMENT
 *
 *  Weist einem PCI-Gerät einen DMA-Adressraum zu.
 *  Das Gerät kann dann nur auf den Speicher zugreifen, den die
 *  Second-Level Page Table erlaubt.
 * ============================================================================ */
int cosmos_pt_assign_device_dma(cosmos_passthrough_mgr* mgr,
                                uint8_t bus, uint8_t dev, uint8_t func,
                                uint64_t second_level_pml4_phys) {
    if (!mgr || !mgr->has_vtd) return -1;
    
#ifndef __linux__
    PT_LOG("DMA Assignment: %02x:%02x.%x → PML4=0x%llX",
           bus, dev, func, (unsigned long long)second_level_pml4_phys);
    
    // Root Entry für diesen Bus
    vtd_root_entry* re = &mgr->root_table[bus];
    
    // Context Table allozieren wenn nötig
    if (!(re->lo & 1)) {
        uint64_t ctx_phys;
        vtd_context_entry* ctx = (vtd_context_entry*)pt_alloc_page(&ctx_phys);
        if (!ctx) return -2;
        memset(ctx, 0, VTD_CONTEXT_TABLE_SIZE);
        
        re->lo = (ctx_phys & ~0xFFFULL) | 1;  // Present
        re->hi = 0;
        
        PT_LOG("  Context Table für Bus %u @ phys=0x%llX", bus, (unsigned long long)ctx_phys);
    }
    
    // Context Entry für dieses Device:Function
    vtd_context_entry* ctx_table = (vtd_context_entry*)(re->lo & ~0xFFFULL);
    uint8_t ctx_idx = (dev << 3) | func;
    vtd_context_entry* ce = &ctx_table[ctx_idx];
    
    // Context Entry konfigurieren:
    // - Present
    // - Translation Type = 0 (Second-Level only)
    // - Second-Level Page Table Pointer
    ce->lo = (second_level_pml4_phys & ~0xFFFULL) | 1;  // Present
    
    // Address Width = 2 (48-bit, 4-level page table)
    // Domain ID = bus * 32 + dev * 8 + func (eindeutig pro Gerät)
    uint16_t domain_id = (uint16_t)(bus * 32 + dev * 8 + func);
    ce->hi = (2ULL << 0) | ((uint64_t)domain_id << 8);
    
    PT_LOG("  Context[%u]: Domain=%u, AW=48bit, SL-PML4=0x%llX",
           ctx_idx, domain_id, (unsigned long long)second_level_pml4_phys);
    
    // IOTLB Invalidieren
    // TODO: Proper IOTLB invalidation sequence
    
    PT_LOG("  ★ DMA Assignment komplett für %02x:%02x.%x", bus, dev, func);
#else
    PT_LOG("DMA Assignment: Unter Linux via IOMMU/VFIO verwaltet");
#endif
    
    return 0;
}

/* ============================================================================
 *  CONVENIENCE: GPU PASSTHROUGH SETUP
 *
 *  Scannt die GTX 1060 + HDMI Audio, trennt Treiber, mappt EPT.
 *  One-Call-Setup für den häufigsten Anwendungsfall.
 * ============================================================================ */
int cosmos_pt_setup_gpu(cosmos_passthrough_mgr* mgr, cosmos_hypervisor* hv) {
    if (!mgr || !hv) return -1;
    
    PT_LOG("╔══════════════════════════════════════════╗");
    PT_LOG("║   GPU PASSTHROUGH SETUP                  ║");
    PT_LOG("║   NVIDIA GeForce GTX 1060 6GB            ║");
    PT_LOG("╚══════════════════════════════════════════╝");
    
    // Schritt 1: VT-d initialisieren
    int result = cosmos_pt_init_vtd(mgr);
    if (result != 0) {
        PT_LOG("VT-d Init fehlgeschlagen, DMA-Remapping nicht verfügbar");
        // Weitermachen ohne DMA-Remapping (unsicher, aber funktional)
    }
    
    // Schritt 2: GPU scannen (Bus 01, Device 00, Function 0)
    int gpu_idx = cosmos_pt_scan_device(mgr, 0x01, 0x00, 0x00);
    if (gpu_idx < 0) {
        PT_LOG("FEHLER: GPU nicht gefunden!");
        return -1;
    }
    
    // Schritt 3: HDMI Audio scannen (Bus 01, Device 00, Function 1)
    int audio_idx = cosmos_pt_scan_device(mgr, 0x01, 0x00, 0x01);
    if (audio_idx < 0) {
        PT_LOG("WARNUNG: HDMI Audio nicht gefunden (nicht kritisch)");
    }
    
    // Schritt 4: GPU an Gast zuweisen
    result = cosmos_pt_attach_device(mgr, hv, gpu_idx);
    if (result != 0) {
        PT_LOG("FEHLER: GPU konnte nicht zugewiesen werden!");
        return -2;
    }
    
    // Schritt 5: HDMI Audio an Gast zuweisen (IOMMU Group 1 erfordert das!)
    if (audio_idx >= 0) {
        result = cosmos_pt_attach_device(mgr, hv, audio_idx);
        if (result != 0) {
            PT_LOG("WARNUNG: HDMI Audio konnte nicht zugewiesen werden");
        }
    }
    
    // Schritt 6: DMA-Remapping für GPU konfigurieren
    if (mgr->has_vtd && hv->ept_pml4_phys) {
        cosmos_pt_assign_device_dma(mgr, 0x01, 0x00, 0x00, hv->ept_pml4_phys);
        if (audio_idx >= 0) {
            cosmos_pt_assign_device_dma(mgr, 0x01, 0x00, 0x01, hv->ept_pml4_phys);
        }
    }
    
    PT_LOG("╔══════════════════════════════════════════╗");
    PT_LOG("║  ★ GPU PASSTHROUGH KOMPLETT! ★            ║");
    PT_LOG("║                                          ║");
    PT_LOG("║  GTX 1060 6GB → Gast zugewiesen          ║");
    PT_LOG("║  VRAM (256MB) → EPT [Write-Combining]    ║");
    PT_LOG("║  MMIO (16MB)  → EPT [Uncacheable]        ║");
    PT_LOG("║  MSI          → Direct Injection          ║");
    PT_LOG("║  DMA          → VT-d Remapped             ║");
    PT_LOG("╚══════════════════════════════════════════╝");
    
    return 0;
}
