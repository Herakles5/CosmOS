/*
 * ============================================================================
 *  COSMOS EPT - Extended Page Tables
 * ============================================================================
 *
 *  Phase 1.4: Hardware-Speichervirtualisierung
 *
 *  EPT erlaubt es dem Gast, seine eigenen Page Tables zu verwalten,
 *  während der Hypervisor kontrolliert, welche physischen Speicherseiten
 *  der Gast tatsächlich sehen und benutzen darf.
 *
 *  Struktur (4-Level Paging, identisch zu x86_64):
 *    PML4 → PDPT → PD → PT → Physical Page
 *
 *  Für Stealth: Wir mappen den gesamten physischen Speicher 1:1 (Identity Map),
 *  damit der Gast keinen Performance-Verlust durch EPT-Misses bemerkt.
 * ============================================================================
 */

#ifndef COSMOS_EPT_H
#define COSMOS_EPT_H

#include <stdint.h>

/* ============================================================================
 *  EPT ENTRY FLAGS
 * ============================================================================ */

#define EPT_READ            (1ULL << 0)
#define EPT_WRITE           (1ULL << 1)
#define EPT_EXECUTE         (1ULL << 2)
#define EPT_MEMORY_TYPE_SHIFT   3
#define EPT_MEMORY_TYPE_MASK    (7ULL << EPT_MEMORY_TYPE_SHIFT)
#define EPT_IGNORE_PAT      (1ULL << 6)
#define EPT_LARGE_PAGE       (1ULL << 7)   // 2MB oder 1GB Page
#define EPT_ACCESSED         (1ULL << 8)
#define EPT_DIRTY            (1ULL << 9)
#define EPT_USER_EXECUTE     (1ULL << 10)  // Execute für User-Mode (Mode-Based Execute)

// Memory Types (in EPT Entry Bits [5:3])
#define EPT_MT_UC           0   // Uncacheable
#define EPT_MT_WC           1   // Write Combining
#define EPT_MT_WT           4   // Write Through
#define EPT_MT_WP           5   // Write Protected
#define EPT_MT_WB           6   // Write Back (Standard für RAM)

// EPT Vollzugriff (Read + Write + Execute, Write-Back)
#define EPT_RWX             (EPT_READ | EPT_WRITE | EPT_EXECUTE)
#define EPT_RWX_WB          (EPT_RWX | (EPT_MT_WB << EPT_MEMORY_TYPE_SHIFT))

// EPTP (Extended Page Table Pointer) Flags
#define EPTP_MT_WB          (6ULL << 0)     // Memory Type = Write-Back
#define EPTP_PAGE_WALK_4    (3ULL << 3)     // 4-Level Page Walk (Standard)
#define EPTP_AD_ENABLE      (1ULL << 6)     // Accessed/Dirty Bits aktivieren

// Adress-Maske für EPT Entries (Bits [51:12] = physische Adresse)
#define EPT_ADDR_MASK       0x000FFFFFFFFFF000ULL

/* ============================================================================
 *  EPT PAGING STRUCTURES
 * ============================================================================ */

// Alle EPT-Tabellen sind 4KB groß und enthalten 512 Einträge à 8 Bytes
#define EPT_ENTRIES_PER_TABLE   512
#define EPT_TABLE_SIZE          (EPT_ENTRIES_PER_TABLE * sizeof(uint64_t))

// Page Sizes
#define EPT_PAGE_SIZE_4K    0x1000ULL
#define EPT_PAGE_SIZE_2M    0x200000ULL
#define EPT_PAGE_SIZE_1G    0x40000000ULL

// EPT PML4 Entry → zeigt auf PDPT
// EPT PDPT Entry → zeigt auf PD (oder 1GB Page)
// EPT PD Entry   → zeigt auf PT (oder 2MB Page)
// EPT PT Entry   → zeigt auf 4KB Page

/* ============================================================================
 *  EPT TABLE STRUCTURES (für Bare Metal)
 * ============================================================================ */

struct __attribute__((aligned(4096))) ept_pml4_table {
    uint64_t entries[EPT_ENTRIES_PER_TABLE];
};

struct __attribute__((aligned(4096))) ept_pdpt_table {
    uint64_t entries[EPT_ENTRIES_PER_TABLE];
};

struct __attribute__((aligned(4096))) ept_pd_table {
    uint64_t entries[EPT_ENTRIES_PER_TABLE];
};

struct __attribute__((aligned(4096))) ept_pt_table {
    uint64_t entries[EPT_ENTRIES_PER_TABLE];
};

/* ============================================================================
 *  EPT ADDRESS DECOMPOSITION
 *
 *  Guest Physical Address (GPA) wird in 4 Indices zerlegt:
 *
 *  Bits [47:39] → PML4 Index (9 bits, 0-511)
 *  Bits [38:30] → PDPT Index (9 bits, 0-511)
 *  Bits [29:21] → PD Index   (9 bits, 0-511)
 *  Bits [20:12] → PT Index   (9 bits, 0-511)
 *  Bits [11:0]  → Page Offset (12 bits)
 * ============================================================================ */

static inline uint64_t ept_pml4_index(uint64_t gpa) {
    return (gpa >> 39) & 0x1FF;
}

static inline uint64_t ept_pdpt_index(uint64_t gpa) {
    return (gpa >> 30) & 0x1FF;
}

static inline uint64_t ept_pd_index(uint64_t gpa) {
    return (gpa >> 21) & 0x1FF;
}

static inline uint64_t ept_pt_index(uint64_t gpa) {
    return (gpa >> 12) & 0x1FF;
}

/* ============================================================================
 *  EPT HELPER FUNCTIONS
 * ============================================================================ */

// Erstelle einen EPT Entry der auf eine Tabelle zeigt
static inline uint64_t ept_make_table_entry(uint64_t table_phys_addr) {
    return (table_phys_addr & EPT_ADDR_MASK) | EPT_RWX;
}

// Erstelle einen EPT Entry für eine 4KB Page
static inline uint64_t ept_make_4k_entry(uint64_t page_phys_addr, uint64_t flags) {
    return (page_phys_addr & EPT_ADDR_MASK) | flags;
}

// Erstelle einen EPT Entry für eine 2MB Large Page
static inline uint64_t ept_make_2m_entry(uint64_t page_phys_addr, uint64_t flags) {
    return (page_phys_addr & 0x000FFFFFFFE00000ULL) | EPT_LARGE_PAGE | flags;
}

// Erstelle einen EPT Entry für eine 1GB Huge Page
static inline uint64_t ept_make_1g_entry(uint64_t page_phys_addr, uint64_t flags) {
    return (page_phys_addr & 0x000FFFFFC0000000ULL) | EPT_LARGE_PAGE | flags;
}

// Erstelle den EPTP (Extended Page Table Pointer) für den VMCS
static inline uint64_t ept_make_eptp(uint64_t pml4_phys_addr) {
    return (pml4_phys_addr & EPT_ADDR_MASK) | EPTP_MT_WB | EPTP_PAGE_WALK_4 | EPTP_AD_ENABLE;
}

/* ============================================================================
 *  EPT IDENTITY MAP BUILDER
 *
 *  Erstellt eine 1:1-Abbildung: GPA == HPA (Guest Physical = Host Physical)
 *  Benutzt 2MB Large Pages für Performance (weniger EPT-Einträge, weniger TLB-Misses)
 *
 *  Für einen i7-7700 mit 16/32GB RAM brauchen wir:
 *    1 PML4 → 1 PDPT → 16 PDs (für 32GB) → 512 * 16 = 8192 2MB-Pages
 *
 *  Speicherbedarf: ~80KB (1 PML4 + 1 PDPT + 16 PDs × 4KB)
 * ============================================================================ */

// Maximaler physischer Speicher den wir mappen (64GB sollte reichen)
#define EPT_MAX_PHYS_MEM    (64ULL * 1024 * 1024 * 1024)

// EPT State
struct cosmos_ept_state {
    ept_pml4_table* pml4;
    uint64_t        pml4_phys;
    
    ept_pdpt_table* pdpt;
    uint64_t        pdpt_phys;
    
    // Wir brauchen bis zu 64 PD-Tabellen (für 64GB, eine pro 1GB Bereich)
    ept_pd_table*   pd[64];
    uint64_t        pd_phys[64];
    
    uint64_t        eptp;           // Fertiger EPTP-Wert für den VMCS
    uint64_t        mapped_size;    // Wie viel Speicher gemapped ist
};
struct cosmos_hypervisor;
struct cosmos_vcpu;

int cosmos_hv_setup_ept(cosmos_hypervisor* hv);
int cosmos_hv_handle_ept_violation(cosmos_hypervisor* hv, cosmos_vcpu* vcpu);
int cosmos_ept_set_page_flags(cosmos_hypervisor* hv, uint64_t guest_phys, uint64_t new_flags);
int cosmos_ept_hide_memory(cosmos_hypervisor* hv, uint64_t phys_start, uint64_t size);
int cosmos_ept_hide_page(cosmos_hypervisor* hv, uint64_t target_phys, uint64_t dummy_phys);
int cosmos_ept_hide_hv_memory(cosmos_hypervisor* hv);

#endif // COSMOS_EPT_H
