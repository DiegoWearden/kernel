#include "heap.h"
#include "printf.h"
#include "atomic.h"
#include "stdint.h"

extern char _end[];
extern char __heap_start[];
extern char __heap_end[];

static char* heap_start = nullptr;
static char* heap_end = nullptr;
static Atomic<char*> heap_current(nullptr);

void heap_init() {
    heap_start = __heap_start;
    heap_end = __heap_end;
    heap_current.set(heap_start);
    
    size_t heap_size = heap_end - heap_start;
    
    printf("Heap initialized: start=0x%llx, end=0x%llx, size=%zu MB\n", 
           (uint64_t)heap_start, (uint64_t)heap_end, heap_size / (1024 * 1024));
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    
    size_t aligned_size = (size + 15) & ~15;
    
    while (true) {
        char* current = heap_current.get();
        char* new_current = current + aligned_size;
        
        if (new_current > heap_end) {
            printf("kmalloc: Out of memory! Requested %zu bytes\n", size);
            return nullptr;
        }
        
        char* old_current = heap_current.exchange(new_current);
        if (old_current == current) {
            for (size_t i = 0; i < aligned_size; i++) {
                current[i] = 0;
            }
            return current;
        }
        
    }
}

size_t get_heap_used() {
    char* current = heap_current.get();
    return current - heap_start;
}

size_t get_heap_free() {
    char* current = heap_current.get();
    return heap_end - current;
}

void* get_heap_start() {
    return heap_start;
}

void* get_heap_current() {
    return heap_current.get();
}

void* get_heap_end() {
    return heap_end;
}

// C++ operator new/delete implementations
void* operator new(unsigned long size) {
    return kmalloc(size);
}

void* operator new[](unsigned long size) {
    return kmalloc(size);
}

void operator delete(void* ptr) {
    (void)ptr;
}

void operator delete[](void* ptr) {
    (void)ptr;
}

void operator delete(void* ptr, unsigned long size) {
    (void)ptr;
    (void)size;
}

void operator delete[](void* ptr, unsigned long size) {
    (void)ptr;
    (void)size;
}
