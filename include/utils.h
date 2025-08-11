#ifndef _BOOT_H
#define _BOOT_H

extern "C" void delay(unsigned long);
extern "C" void put32(volatile unsigned int*, unsigned int);
extern "C" unsigned int get32(volatile unsigned int*);
extern "C" unsigned long getCoreID();
extern "C" unsigned long get_el();
extern "C" unsigned long get_sp();

extern int onHypervisor;

#include "stdint.h"
#include "printf.h"
#define ALIGN_PTR_DOWN_16(p) ((uint64_t*)((uintptr_t)(p) & ~(uintptr_t)15))
#define ALIGN_PTR_UP_16(p)   ((uint64_t*)((((uintptr_t)(p) + 15) & ~(uintptr_t)15)))

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        panic("ASSERT FAILED: %s (file %s, line %d) - %s\n", #cond, __FILE__, __LINE__, (msg)); \
    } \
} while (0)

#endif /*_BOOT_H */
