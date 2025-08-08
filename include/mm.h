#include "peripherals/base.h"
#ifndef _MM_H
#define _MM_H

#define PAGE_SHIFT 12
#define TABLE_SHIFT 9
#define SECTION_SHIFT (PAGE_SHIFT + TABLE_SHIFT)
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define SECTION_SIZE (1 << SECTION_SHIFT)
#define LOW_MEMORY (2 * SECTION_SIZE)
#define HIGH_MEMORY PBASE
#define PAGING_MEMORY (HIGH_MEMORY - LOW_MEMORY)
#define PAGING_PAGES (PAGING_MEMORY / PAGE_SIZE)
#ifndef __ASSEMBLER__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void memzero(unsigned long src, unsigned long n);

// Minimal physical page allocator API
void palloc_init();
uint64_t palloc_page();
void pfree_page(uint64_t phys_addr);

#ifdef __cplusplus
}
#endif

#endif
#endif /*_MM_H */