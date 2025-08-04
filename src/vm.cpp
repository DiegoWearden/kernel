#include "vm.h"
#include "libk.h"
#include "stdint.h"

uint64_t PGD[512] __attribute__((aligned(4096), section(".paging")));
uint64_t PUD[512] __attribute__((aligned(4096), section(".paging")));
uint64_t PMD[512] __attribute__((aligned(4096), section(".paging")));
uint64_t PMD_arm[512] __attribute__((aligned(4096), section(".paging")));

#define MAX_PTE_TABLES 128
#define MAX_PMD_TABLES 16
uint64_t PTE_tables[MAX_PTE_TABLES][512] __attribute__((aligned(4096), section(".paging")));
uint64_t PMD_extra[MAX_PMD_TABLES][512] __attribute__((aligned(4096), section(".paging")));
static int next_pte_table = 0;
static int next_pmd_table = 0;

#define PTE_VALID           (1ULL << 0)
#define PTE_TABLE           (1ULL << 1)
#define PTE_BLOCK           (0ULL << 1)
#define PTE_PAGE            (1ULL << 1)   // For L3 entries (same as TABLE but semantically different)
#define PTE_AF              (1ULL << 10)  // Access Flag
#define PTE_SHARED          (3ULL << 8)   // Inner Shareable
#define PTE_AP_RW           (0ULL << 6)   // Read/Write for EL1
#define PTE_AP_RO           (1ULL << 6)   // Read-only for EL1
#define PTE_UXN             (1ULL << 54)  // Execute Never (user)
#define PTE_PXN             (1ULL << 53)  // Execute Never (privileged)

#define PTE_ATTRINDX_NORMAL      (4ULL << 2)
#define PTE_ATTRINDX_DEVICE      (0ULL << 2)

#define PAGE_SIZE_4KB    0x1000
#define PAGE_SIZE_2MB    0x200000


static inline uint64_t create_table_descriptor(uint64_t next_level_table) {
    return (next_level_table & ~0xFFF) | PTE_VALID | PTE_TABLE;
}

static inline uint64_t create_block_descriptor(uint64_t phys_addr, uint64_t attrs) {
    return (phys_addr & ~0x1FFFFF) | PTE_VALID | PTE_BLOCK | PTE_AF | attrs;
}

static inline uint64_t create_page_descriptor(uint64_t phys_addr, uint64_t attrs) {
    return (phys_addr & ~0xFFF) | PTE_VALID | PTE_PAGE | PTE_AF | attrs;
}

static uint64_t* allocate_pte_table() {
    if (next_pte_table >= MAX_PTE_TABLES) {
        return nullptr;
    }
    
    uint64_t* table = PTE_tables[next_pte_table];
    next_pte_table++;
    
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
    
    return table;
}

static uint64_t* allocate_pmd_table() {
    if (next_pmd_table >= MAX_PMD_TABLES) {
        return nullptr;
    }
    
    uint64_t* table = PMD_extra[next_pmd_table];
    next_pmd_table++;
    
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
    
    return table;
}

// Helper function to determine memory attributes for a physical address        
static inline uint64_t get_memory_attributes(uint64_t phys_addr) {
    if (phys_addr >= 0x80000 && phys_addr < 0x40000000) {
        return PTE_SHARED | PTE_AP_RW | PTE_ATTRINDX_NORMAL;
    }
    
    if (phys_addr >= 0x40000000 && phys_addr < 0x40001000) {
        return PTE_AP_RW | PTE_UXN | PTE_PXN | PTE_ATTRINDX_DEVICE;
    }
    
    if (phys_addr >= 0x3C000000 && phys_addr < 0x3F000000) {
        return PTE_AP_RW | PTE_UXN | PTE_PXN | PTE_ATTRINDX_DEVICE;
    }
    
    if (phys_addr >= 0x3F000000 && phys_addr < 0x40000000) {
        return PTE_AP_RW | PTE_UXN | PTE_PXN | PTE_ATTRINDX_DEVICE;
    }
    
    return PTE_SHARED | PTE_AP_RW | PTE_ATTRINDX_NORMAL;
}

// Map a 2MB block
bool map_address_2mb(uint64_t virt_addr, uint64_t phys_addr, uint64_t attrs) {
    const uint64_t BLOCK_SIZE = 2 * 1024 * 1024;

    // Require 2MB alignment
    if ((virt_addr & (BLOCK_SIZE - 1)) || (phys_addr & (BLOCK_SIZE - 1))) {
        printf("Error: virt_addr or phys_addr is not 2MB aligned\n");
        return false;
    }


    uint64_t pgd_index = (virt_addr >> 39) & 0x1FF;
    uint64_t pud_index = (virt_addr >> 30) & 0x1FF;
    uint64_t pmd_index = (virt_addr >> 21) & 0x1FF;


    if (pgd_index != 0) {
        return false;
    }


    if (!(PGD[pgd_index] & PTE_VALID)) {
        PGD[pgd_index] = create_table_descriptor((uint64_t)PUD);
    }

    uint64_t* pud_table = PUD;
    uint64_t* pmd_table;


    if (!(pud_table[pud_index] & PTE_VALID)) {
        if (pud_index == 0) {
            pmd_table = PMD;
        } else if (pud_index == 1) {
            pmd_table = PMD_arm;
        } else {
            pmd_table = allocate_pmd_table();
            if (!pmd_table) {
                printf("Error: out of PMD tables\n");
                return false;
            }
        }
        pud_table[pud_index] = create_table_descriptor((uint64_t)pmd_table);
    } else {
        pmd_table = (uint64_t*)(pud_table[pud_index] & ~0xFFFULL);
    }


    uint64_t new_desc = create_block_descriptor(phys_addr, attrs);
    pmd_table[pmd_index] = new_desc;

    return true;
}


// Map a 4KB page
bool map_address_4kb(uint64_t virt_addr, uint64_t phys_addr, uint64_t attrs) {
    uint64_t pgd_index = (virt_addr >> 39) & 0x1FF;
    uint64_t pud_index = (virt_addr >> 30) & 0x1FF;
    uint64_t pmd_index = (virt_addr >> 21) & 0x1FF;
    uint64_t pte_index = (virt_addr >> 12) & 0x1FF;

 
    if (pgd_index != 0) {
        return false;
    }


    if (PGD[pgd_index] == 0) {
        PGD[pgd_index] = create_table_descriptor((uint64_t)PUD);
    }

    uint64_t* pud_table = PUD;


    if (pud_table[pud_index] == 0) {
        pud_table[pud_index] = create_table_descriptor((uint64_t)PMD);
    }

    uint64_t* pmd_table = (uint64_t*)(pud_table[pud_index] & ~0xFFFULL);

    if (pmd_table[pmd_index] & PTE_VALID && !(pmd_table[pmd_index] & PTE_TABLE)) {
        return false;
    }

    uint64_t* pte_table = nullptr;

    if (pmd_table[pmd_index] == 0) {
        pte_table = allocate_pte_table();
        if (!pte_table) {
            printf("Error: out of PTE tables\n");
            return false;
        }
        pmd_table[pmd_index] = create_table_descriptor((uint64_t)pte_table);
    } else {
        if (!(pmd_table[pmd_index] & PTE_TABLE)) {
            return false;
        }
        pte_table = (uint64_t*)(pmd_table[pmd_index] & ~0xFFFULL);
    }


    pte_table[pte_index] = create_page_descriptor(phys_addr, attrs);
    return true;
}


void enable_null_pointer_protection() {
    printf("=== Enabling Null Pointer Protection ===\n");

    uint64_t* pte_table = (uint64_t*)(PMD[0] & ~0xFFF);
    pte_table[0] = 0;
}

/**
 *
 * Sets up a 3-level page table hierarchy:
 *   PGD (Level 0) → PUD (Level 1) → PMD (Level 2) → Physical pages
 */
void create_page_tables() {
    for (int i = 0; i < 512; i++) {
        PGD[i] = 0;
        PUD[i] = 0;
        PMD[i] = 0;
        PMD_arm[i] = 0;
    }

    // Map the first 2MB of memory in 4kb pages
    for (uint64_t virt_addr = 0x0; virt_addr < 0x200000; virt_addr += PAGE_SIZE_4KB) {
        map_address_4kb(virt_addr, virt_addr, get_memory_attributes(virt_addr));
    }
    
    // Map low address space (0x0 - 0x40000000) - identity mapping
    for (uint64_t virt_addr = 0x0 + PAGE_SIZE_2MB; virt_addr < 0x40000000; virt_addr += PAGE_SIZE_2MB) {
        map_address_2mb(virt_addr, virt_addr, get_memory_attributes(virt_addr));
    }
    
    // Map high address space (0x40000000+) - device memory
    for (uint64_t virt_addr = 0x40000000; virt_addr < (uint64_t)0x40000000 + (PAGE_SIZE_2MB * 512); virt_addr += PAGE_SIZE_2MB) {
        map_address_2mb(virt_addr, virt_addr, get_memory_attributes(virt_addr));
    }


}

/**
 * used for setting up kernel memory for devices
 */
void patch_page_tables() {
    for (int i = 504; i < 512; i++) {
        PMD[i] = PMD[i] & (~0xFFF);
        PMD[i] = PMD[i] | DEVICE_LOWER_ATTRIBUTES;
    }
    // PUD[1] = PUD[1] & (~0xFFF);
    // PUD[1] = PUD[1] | 0x401;

    for (int i = 0; i < 8; i++) {
        PMD_arm[i] = PMD_arm[i] & (~0xFFF);
        PMD_arm[i] = PMD_arm[i] | DEVICE_LOWER_ATTRIBUTES;
    }
}

void check_address_mapping(uint64_t addr) {
    asm volatile("at s1e1w, %0" :: "r"(addr));
    asm volatile("isb");
    
    uint64_t par;
    asm volatile("mrs %0, par_el1" : "=r"(par));
    
    bool is_faulted = par & 1;  // bit 0 = 1 means translation faulted
    
    printf("Address 0x%016llx: %s (PAR_EL1=0x%016llx)\n", 
           addr, is_faulted ? "UNMAPPED" : "MAPPED", par);
}