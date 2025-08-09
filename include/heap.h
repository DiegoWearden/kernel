#ifndef _HEAP_H
#define _HEAP_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void heap_init();

// Simple bump allocator - allocate memory
void* kmalloc(size_t size);

// Deallocate previously allocated memory
void kfree(void* ptr);

// Optionally expand the heap by at least min_bytes; returns number of bytes added
size_t heap_expand(size_t min_bytes);

// Get current heap usage statistics
size_t get_heap_used();
size_t get_heap_free();

// Get heap boundaries for debugging
void* get_heap_start();
void* get_heap_current();
void* get_heap_end();

#ifdef __cplusplus
}

// C++ operator overloads
void* operator new(unsigned long size);
void* operator new[](unsigned long size);
void operator delete(void* ptr);
void operator delete[](void* ptr);
void operator delete(void* ptr, unsigned long size);
void operator delete[](void* ptr, unsigned long size);

// Placement new (already defined by compiler, but declared for completeness)
inline void* operator new(unsigned long, void* ptr) { return ptr; }
inline void* operator new[](unsigned long, void* ptr) { return ptr; }
inline void operator delete(void*, void*) { }
inline void operator delete[](void*, void*) { }

#endif

#endif /* _HEAP_H */
