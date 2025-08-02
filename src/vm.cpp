#include "vm.h"
#include "libk.h"
#include "stdint.h"

// L0-L2 Page Tables (existing)
uint64_t PGD[512] __attribute__((aligned(4096), section(".paging")));
uint64_t PUD[512] __attribute__((aligned(4096), section(".paging")));
uint64_t PMD[512] __attribute__((aligned(4096), section(".paging")));
uint64_t PMD_arm[512] __attribute__((aligned(4096), section(".paging")));

// L3 Page Tables (NEW) - We'll need multiple PTE tables
// Each PMD entry can point to a PTE table that maps 512 * 4KB = 2MB
#define MAX_PTE_TABLES 128  // Support up to 128 PTE tables (256MB of fine-grained mappings)
#define MAX_PMD_TABLES 16  // Support additional PMD tables for arbitrary mappings
uint64_t PTE_tables[MAX_PTE_TABLES][512] __attribute__((aligned(4096), section(".paging")));
uint64_t PMD_extra[MAX_PMD_TABLES][512] __attribute__((aligned(4096), section(".paging")));
static int next_pte_table = 0;  // Track which PTE table to allocate next
static int next_pmd_table = 0;  // Track which PMD table to allocate next

// Page table entry flags
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

// Memory type attributes
#define PTE_ATTRINDX_NORMAL      (4ULL << 2)  // Normal memory (index 4)
#define PTE_ATTRINDX_DEVICE      (0ULL << 2)  // Device memory (index 0)

// Page size constants
#define PAGE_SIZE_4KB    0x1000      // 4KB
#define PAGE_SIZE_2MB    0x200000    // 2MB

// Helper function to create a table descriptor
static inline uint64_t create_table_descriptor(uint64_t next_level_table) {
    return (next_level_table & ~0xFFF) | PTE_VALID | PTE_TABLE;
}

// Helper function to create a block descriptor (2MB)
static inline uint64_t create_block_descriptor(uint64_t phys_addr, uint64_t attrs) {
    return (phys_addr & ~0x1FFFFF) | PTE_VALID | PTE_BLOCK | PTE_AF | attrs;
}

// Helper function to create a page descriptor (4KB)
static inline uint64_t create_page_descriptor(uint64_t phys_addr, uint64_t attrs) {
    return (phys_addr & ~0xFFF) | PTE_VALID | PTE_PAGE | PTE_AF | attrs;
}

// Allocate a new PTE table
static uint64_t* allocate_pte_table() {
    if (next_pte_table >= MAX_PTE_TABLES) {
        return nullptr;  // Out of PTE tables
    }
    
    uint64_t* table = PTE_tables[next_pte_table];
    next_pte_table++;
    
    // Clear the table
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
    
    return table;
}

// Allocate a new PMD table
static uint64_t* allocate_pmd_table() {
    if (next_pmd_table >= MAX_PMD_TABLES) {
        return nullptr;  // Out of PMD tables
    }
    
    uint64_t* table = PMD_extra[next_pmd_table];
    next_pmd_table++;
    
    // Clear the table
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
    
    return table;
}

// Helper function to determine memory attributes for a physical address
static inline uint64_t get_memory_attributes(uint64_t phys_addr) {
    // Kernel region (0x80000): normal memory with atomic support
    if (phys_addr >= 0x80000 && phys_addr < 0x40000000) {
        return PTE_SHARED | PTE_AP_RW | PTE_ATTRINDX_NORMAL;
    }
    
    // Mailbox region (0x40000000): device memory
    if (phys_addr >= 0x40000000 && phys_addr < 0x40001000) {
        return PTE_AP_RW | PTE_UXN | PTE_PXN | PTE_ATTRINDX_DEVICE;
    }
    
    // Framebuffer region (0x3C000000-0x3F000000): device memory
    if (phys_addr >= 0x3C000000 && phys_addr < 0x3F000000) {
        return PTE_AP_RW | PTE_UXN | PTE_PXN | PTE_ATTRINDX_DEVICE;
    }
    
    // Peripheral region (0x3F000000-0x40000000): device memory
    if (phys_addr >= 0x3F000000 && phys_addr < 0x40000000) {
        return PTE_AP_RW | PTE_UXN | PTE_PXN | PTE_ATTRINDX_DEVICE;
    }
    
    // Default: normal memory with atomic support
    return PTE_SHARED | PTE_AP_RW | PTE_ATTRINDX_NORMAL;
}

/**
 * Map a virtual address to a physical address with custom attributes
 * Now supports both 4KB pages and 2MB blocks!
 * 
 * @param virt_addr Virtual address to map
 * @param phys_addr Physical address to map to
 * @param size Size of the mapping (4KB or 2MB)
 * @param attrs Memory attributes (if 0, will auto-detect based on phys_addr)
 * @return true if mapping was successful, false otherwise
 */
bool map_address(uint64_t virt_addr, uint64_t phys_addr, uint64_t size, uint64_t attrs) {
    // Support both 4KB pages and 2MB blocks
    if (size != PAGE_SIZE_4KB && size != PAGE_SIZE_2MB) {
        return false;  // Only 4KB and 2MB mappings supported
    }
    
    // Calculate page table indices
    uint64_t pgd_index = (virt_addr >> 39) & 0x1FF;
    uint64_t pud_index = (virt_addr >> 30) & 0x1FF;
    uint64_t pmd_index = (virt_addr >> 21) & 0x1FF;
    uint64_t pte_index = (virt_addr >> 12) & 0x1FF;  // For 4KB pages
    
    // Ensure PGD entry exists
    if (PGD[pgd_index] == 0) {
        // For our current setup, use PUD for pgd_index 0
        if (pgd_index == 0) {
            PGD[pgd_index] = create_table_descriptor((uint64_t)PUD);
        } else {
            // For now, return false for addresses outside our current layout
            return false;
        }
    }
    
    // Get PUD table (for now, only support pgd_index 0)
    if (pgd_index != 0) {
        return false;  // Only support low address space for now
    }
    
    uint64_t* pud_table = PUD;
    
    // Ensure PUD entry exists
    if (pud_table[pud_index] == 0) {
        // Determine which PMD table to use
        uint64_t* pmd_table = nullptr;
        
        // Use existing tables for known ranges
        if (pud_index == 0) {
            pmd_table = PMD;
        } else if (pud_index == 1) {
            pmd_table = PMD_arm;
        } else {
            // Allocate a new PMD table for other ranges
            pmd_table = allocate_pmd_table();
            if (pmd_table == nullptr) {
                return false;  // Out of PMD tables
            }
        }
        
        pud_table[pud_index] = create_table_descriptor((uint64_t)pmd_table);
    }
    
    // Get PMD table
    uint64_t pmd_table_addr = pud_table[pud_index] & ~0xFFF;
    uint64_t* pmd_table = (uint64_t*)pmd_table_addr;
    
    // Use auto-detected attributes if none provided
    if (attrs == 0) {
        attrs = get_memory_attributes(phys_addr);
    }
    
    if (size == PAGE_SIZE_2MB) {
        // Create 2MB block mapping (existing behavior)
        pmd_table[pmd_index] = create_block_descriptor(phys_addr, attrs);
    } else {
        // Create 4KB page mapping (NEW!)
        
        // Check if PMD entry already exists and is a block (can't mix blocks and tables)
        if (pmd_table[pmd_index] != 0 && !(pmd_table[pmd_index] & PTE_TABLE)) {
            return false;  // PMD entry is already a block, can't make it a table
        }
        
        // Ensure PTE table exists
        if (pmd_table[pmd_index] == 0) {
            // Allocate a new PTE table
            uint64_t* pte_table = allocate_pte_table();
            if (pte_table == nullptr) {
                return false;  // Out of PTE tables
            }
            pmd_table[pmd_index] = create_table_descriptor((uint64_t)pte_table);
        }
        
        // Get PTE table and create 4KB page mapping
        uint64_t pte_table_addr = pmd_table[pmd_index] & ~0xFFF;
        uint64_t* pte_table = (uint64_t*)pte_table_addr;
        
        pte_table[pte_index] = create_page_descriptor(phys_addr, attrs);
    }
    
    return true;
}

/**
 * Map a range of virtual addresses to physical addresses
 * Intelligently uses 2MB blocks when possible, 4KB pages for fine-grained control
 * 
 * @param virt_start Starting virtual address
 * @param phys_start Starting physical address
 * @param size Total size to map
 * @param attrs Memory attributes (if 0, will auto-detect)
 * @param force_4kb If true, forces 4KB pages even when 2MB would work
 * @return true if all mappings were successful
 */
bool map_range(uint64_t virt_start, uint64_t phys_start, uint64_t size, uint64_t attrs, bool force_4kb) {
    uint64_t virt_addr = virt_start;
    uint64_t phys_addr = phys_start;
    uint64_t remaining = size;
    
    while (remaining > 0) {
        // Check if we can use a 2MB block (more efficient)
        bool can_use_2mb = !force_4kb && 
                          remaining >= PAGE_SIZE_2MB &&
                          (virt_addr & (PAGE_SIZE_2MB - 1)) == 0 &&  // 2MB aligned virtual
                          (phys_addr & (PAGE_SIZE_2MB - 1)) == 0;    // 2MB aligned physical
        
        if (can_use_2mb) {
            // Use 2MB block
            if (!map_address(virt_addr, phys_addr, PAGE_SIZE_2MB, attrs)) {
                return false;
            }
            virt_addr += PAGE_SIZE_2MB;
            phys_addr += PAGE_SIZE_2MB;
            remaining -= PAGE_SIZE_2MB;
        } else {
            // Use 4KB page
            if (remaining < PAGE_SIZE_4KB) {
                return false;  // Can't map less than 4KB
            }
            if (!map_address(virt_addr, phys_addr, PAGE_SIZE_4KB, attrs)) {
                return false;
            }
            virt_addr += PAGE_SIZE_4KB;
            phys_addr += PAGE_SIZE_4KB;
            remaining -= PAGE_SIZE_4KB;
        }
    }
    
    return true;
}

/**
 * Convenience wrapper for map_range with default 4KB preference
 */
bool map_range(uint64_t virt_start, uint64_t phys_start, uint64_t size, uint64_t attrs) {
    return map_range(virt_start, phys_start, size, attrs, false);  // Allow 2MB optimization
}

/**
 * Map a range while excluding dangerous null addresses but preserving mailbox registers
 * This helps catch null pointer dereferences while keeping system functionality
 */
bool map_range_skip_null(uint64_t virt_start, uint64_t phys_start, uint64_t size, uint64_t attrs, bool force_4kb) {
    uint64_t virt_addr = virt_start;
    uint64_t phys_addr = phys_start;
    uint64_t remaining = size;
    
    while (remaining > 0) {
        // Skip the dangerous null region (0x0-0x7F) but allow mailbox registers (0x80+)
        // Mailbox registers are at 0xe0, 0xe8, 0xf0 and must remain accessible
        if (virt_addr < 0x80) {
            // Skip mapping the dangerous null region (first 128 bytes)
            uint64_t skip_size = 0x80 - virt_addr;
            if (skip_size > remaining) skip_size = remaining;
            virt_addr += skip_size;
            phys_addr += skip_size;
            remaining -= skip_size;
            continue;
        }
        
        // Check if we can use a 2MB block (more efficient)
        bool can_use_2mb = !force_4kb && 
                          remaining >= PAGE_SIZE_2MB &&
                          (virt_addr & (PAGE_SIZE_2MB - 1)) == 0 &&  // 2MB aligned virtual
                          (phys_addr & (PAGE_SIZE_2MB - 1)) == 0;    // 2MB aligned physical
        
        if (can_use_2mb) {
            // Use 2MB block
            if (!map_address(virt_addr, phys_addr, PAGE_SIZE_2MB, attrs)) {
                return false;
            }
            virt_addr += PAGE_SIZE_2MB;
            phys_addr += PAGE_SIZE_2MB;
            remaining -= PAGE_SIZE_2MB;
        } else {
            // Use 4KB page
            if (remaining < PAGE_SIZE_4KB) {
                return false;  // Can't map less than 4KB
            }
            if (!map_address(virt_addr, phys_addr, PAGE_SIZE_4KB, attrs)) {
                return false;
            }
            virt_addr += PAGE_SIZE_4KB;
            phys_addr += PAGE_SIZE_4KB;
            remaining -= PAGE_SIZE_4KB;
        }
    }
    
    return true;
}

/**
 * Enable null pointer protection by unmapping the first 4KB page
 * Call this AFTER all cores have booted and mailbox registers are no longer needed
 */
void enable_null_pointer_protection() {
    printf("=== Enabling Null Pointer Protection ===\n");
    
    if (PMD[0] & PTE_TABLE) {
        uint64_t pte_table_addr = PMD[0] & ~0xFFF;
        uint64_t* pte_table = (uint64_t*)pte_table_addr;
        
        printf("Before: PTE[0] = 0x%016llx\n", pte_table[0]);
        printf("Unmapping page 0 (0x0000-0x0FFF) for null pointer protection...\n");
        
        pte_table[0] = 0;  // Clear the PTE entry for page 0
        
        printf("After: PTE[0] = 0x%016llx\n", pte_table[0]);
        
        // Stronger cache and TLB invalidation for hardware compatibility
        asm volatile("dc civac, %0" :: "r"(pte_table) : "memory");
        asm volatile("dc civac, %0" :: "r"(&pte_table[0]) : "memory");
        asm volatile("dsb sy");
        asm volatile("tlbi vmalle1is");
        asm volatile("dsb sy");
        asm volatile("isb");
        
        printf("Null pointer protection enabled!\n");
        printf("   - Address 0x0000-0x0FFF: UNMAPPED (will cause page fault)\n");
        printf("   - Address 0x1000+: Still accessible\n");
        
        // Verify the change took effect
        printf("Verification: PTE[0] = 0x%016llx\n", pte_table[0]);
        
    } else {
        printf("Cannot enable null protection: PMD[0] is not a PTE table\n");
        printf("PMD[0] = 0x%016llx\n", PMD[0]);
    }
    printf("=========================================\n\n");
}

/**
 * Print information about null page protection
 */
void print_null_page_info() {
    printf("=== Null Page Protection Details ===\n");
    printf("Protection: Delayed until after core boot\n");
    printf("Range: 0x0000-0x0FFF (first 4KB page)\n");
    printf("Effect: Null pointer dereferences cause page faults\n");
    printf("Implementation: Unmap page 0 after cores are running\n");
    printf("=========================================\n\n");
}

/**
 * Demo function showing how easy it is to map memory regions
 * Now with 4KB page support and null page protection!
 */
void demo_mapping_functions() {
    // Example 1: Map a single 2MB region (block mapping)
    // Map virtual address 0x10000000 to physical address 0x80000
    map_address(0x10000000, 0x80000, PAGE_SIZE_2MB);
    
    // Example 2: Map a single 4KB page (fine-grained mapping)
    // Map virtual address 0x20000000 to physical address 0x100000
    map_address(0x20000000, 0x100000, PAGE_SIZE_4KB);
    
    // Example 3: Map a range that will use optimal block sizes
    // Map 8MB starting at virtual 0x30000000 to physical 0x200000
    // This will automatically use 2MB blocks when aligned
    map_range(0x30000000, 0x200000, 0x800000);
    
    // Example 4: Force 4KB pages for fine-grained control
    // Map 2MB using only 4KB pages (512 individual page mappings)
    map_range(0x40000000, 0x300000, PAGE_SIZE_2MB, 0, true);  // force_4kb = true
    
    // Example 5: Map device memory with custom attributes (4KB)
    // Map UART device at virtual 0x50000000 to physical 0x3F201000
    map_address(0x50000000, 0x3F201000, PAGE_SIZE_4KB, PTE_AP_RW | PTE_ATTRINDX_DEVICE);
    
    // Example 6: Map unaligned region (will use 4KB pages automatically)
    // Starting address is not 2MB aligned, so 4KB pages will be used
    map_range(0x60001000, 0x400000, 0x10000);  // 64KB starting at unaligned address
    
    // Example 7: Map normal memory with auto-detected attributes (4KB)
    // The function will automatically detect this is normal memory
    map_address(0x70000000, 0x500000, PAGE_SIZE_4KB);
}

/**
 * Create page tables in C++ - much more readable than assembly!
 * Sets up a 3-level page table hierarchy:
 *   PGD (Level 0) → PUD (Level 1) → PMD (Level 2) → Physical pages
 */
void create_page_tables_cpp() {
    // Clear all page tables first
    for (int i = 0; i < 512; i++) {
        PGD[i] = 0;
        PUD[i] = 0;
        PMD[i] = 0;
        PMD_arm[i] = 0;
    }
    
    // ===== LEVEL 0: PGD (Page Global Directory) =====
    
    // PGD[0] points to PUD
    PGD[0] = create_table_descriptor((uint64_t)PUD);
    
    // ===== LEVEL 1: PUD (Page Upper Directory) =====
    
    // PUD[0] points to PMD (for low addresses < 0x40000000)
    PUD[0] = create_table_descriptor((uint64_t)PMD);
    
    // PUD[1] points to PMD_arm (for high addresses >= 0x40000000)
    PUD[1] = create_table_descriptor((uint64_t)PMD_arm);
    
    // ===== MAP MEMORY REGIONS =====
    
    // Map low address space (0x0 - 0x40000000) - identity mapping
    // Use 4KB pages for first 256MB (128 PTE tables), 2MB blocks for the rest
    // IMPORTANT: Skip mapping the null region (0x0-0x7F) to catch null pointer dereferences
    for (int i = 0; i < 512; i++) {
        uint64_t virt_addr = (uint64_t)i << 21;  // 2MB regions
        uint64_t phys_addr = virt_addr;  // Identity mapping
        
        if (i < 128) {
            // Use 4KB pages for first 256MB (can demonstrate fine-grained control)
            if (i == 0) {
                // For now, map first 2MB region normally during boot
                // Null protection will be enabled later after cores are up
                map_range(virt_addr, phys_addr, PAGE_SIZE_2MB, 0, true);  // force_4kb = true
            } else {
                // Normal mapping for other regions
                map_address(virt_addr, phys_addr, PAGE_SIZE_2MB);  // force_4kb = true
            }
        } else {
            // Use 2MB blocks for remaining 768MB (more efficient)
            map_address(virt_addr, phys_addr, PAGE_SIZE_2MB);
        }
    }
    
    // Map high address space (0x40000000+) - device memory
    for (int i = 0; i < 512; i++) {
        uint64_t virt_addr = 0x40000000 + ((uint64_t)i << 21);  // 2MB blocks starting at 0x40000000
        uint64_t phys_addr = 0x40000000 + ((uint64_t)i << 21);  // Identity mapping for device memory
        map_address(virt_addr, phys_addr, PAGE_SIZE_2MB, PTE_AP_RW | PTE_ATTRINDX_DEVICE);
    }
    
    // Map each 2MB block in the upper address space (device memory)
    for (int i = 0; i < 512; i++) {
        uint64_t phys_addr = 0x40000000 + ((uint64_t)i << 21);  // 2MB blocks starting at 0x40000000
        uint64_t attrs = PTE_AP_RW | PTE_ATTRINDX_DEVICE;  // All device memory
        PMD_arm[i] = create_block_descriptor(phys_addr, attrs);
    }

}

/**
 * Print page table statistics and usage information
 */
void print_page_table_stats() {
    int pte_tables_used = next_pte_table;
    int pte_tables_available = MAX_PTE_TABLES - next_pte_table;
    
    printf("=== PAGE TABLE STATISTICS ===\n");
    printf("L3 (PTE) Tables Used: %d/%d\n", pte_tables_used, MAX_PTE_TABLES);
    printf("L3 (PTE) Tables Available: %d\n", pte_tables_available);
    printf("Fine-grained (4KB) mapping capacity: %d MB\n", pte_tables_available * 2);
    printf("Current 4KB page allocations: %d pages (%d KB)\n", 
           pte_tables_used * 512, pte_tables_used * 512 * 4);
    printf("=============================\n");
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

// Add a function to check if address is mapped using translation registers
void check_address_mapping(uint64_t addr) {
    // Use AT S1E1W (Address Translate Stage 1 EL1 Write) to check mapping
    asm volatile("at s1e1w, %0" :: "r"(addr));
    asm volatile("isb");
    
    uint64_t par;
    asm volatile("mrs %0, par_el1" : "=r"(par));
    
    bool is_faulted = par & 1;  // bit 0 = 1 means translation faulted
    
    printf("Address 0x%016llx: %s (PAR_EL1=0x%016llx)\n", 
           addr, is_faulted ? "UNMAPPED" : "MAPPED", par);
}