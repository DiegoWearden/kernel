#include "vm.h"
#include "arm/sysregs.h"

//---------------------------------------------------------
// Global: init_mmu
// Initializes the MMU (loads page table base registers, TCR, MAIR, etc.).
//---------------------------------------------------------
.global init_mmu
init_mmu:
    dsb     sy
    // Set the base address of the page tables into ttbr1_el1 and ttbr0_el1.
    adr     x0, __paging_start
    orr     x0, x0, #1
    msr     ttbr1_el1, x0
    msr     ttbr0_el1, x0
    isb

    // Set the Translation Control Register.
    ldr     x0, =(TCR_VALUE)
    msr     tcr_el1, x0
    isb

    // Set the Memory Attribute Indirection Register.
    ldr     x0, =(MAIR_VALUE)
    msr     mair_el1, x0

    // Adjust stack pointer and frame pointer for virtual addressing.
    ldr     x4, =(VA_START)
    add     sp, sp, x4
    add     x29, x29, x4
    add     x30, x30, x4

    // Invalidate TLB and caches.
    tlbi    vmalle1is
    dsb     sy
    isb

    // Enable the MMU and data cache.
    mrs     x0, sctlr_el1
    ldr     x4, =(SCTLR_MMU_ENABLED)
    orr     x0, x0, x4
    msr     sctlr_el1, x0

    tlbi    vmalle1is
    dsb     sy
    isb

    ret