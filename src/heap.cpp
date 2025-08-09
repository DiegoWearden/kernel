#include "heap.h"
#include "printf.h"
#include "atomic.h"
#include "stdint.h"

extern char _end[];
extern char __heap_start[];
extern char __heap_end[];



static constexpr size_t HEAP_ALIGN = 16;
static constexpr size_t ALLOCATED_FLAG = 1ULL; // LSB marks allocation status

    struct BlockHeader {
        size_t size_and_flags;   // total block size (header..footer), LSB=1 if allocated
    BlockHeader* prev_free;  // free-list prev (valid only when block is free)
    BlockHeader* next_free;  // free-list next (valid only when block is free)
};

static inline size_t align_up(size_t value, size_t align) {
    return (value + (align - 1)) & ~(align - 1);
}

static inline size_t header_aligned_size() {
    return align_up(sizeof(BlockHeader), HEAP_ALIGN);
}

static inline size_t footer_size() {
    return sizeof(size_t);
}

static inline size_t block_total_size(const BlockHeader* h) {
    return h->size_and_flags & ~ALLOCATED_FLAG;
}

static inline bool block_is_allocated(const BlockHeader* h) {
    return (h->size_and_flags & ALLOCATED_FLAG) != 0;
}

static inline void set_block_allocated(BlockHeader* h, bool allocated) {
    if (allocated) {
        h->size_and_flags |= ALLOCATED_FLAG;
    } else {
        h->size_and_flags &= ~ALLOCATED_FLAG;
    }
}

static inline size_t* block_footer_ptr(BlockHeader* h) {
    return (size_t*)((char*)h + block_total_size(h) - footer_size());
}

static inline void write_footer(BlockHeader* h) {
    *block_footer_ptr(h) = h->size_and_flags;
}

static inline BlockHeader* block_from_payload(void* payload) {
    return (BlockHeader*)((char*)payload - header_aligned_size());
}

static inline void* payload_from_block(BlockHeader* h) {
    return (char*)h + header_aligned_size();
}

static inline BlockHeader* next_block(BlockHeader* h, char* heap_end) {
    char* next = (char*)h + block_total_size(h);
    if (next >= heap_end) return nullptr;
    return (BlockHeader*)next;
}

static inline BlockHeader* prev_block(BlockHeader* h, char* heap_start) {
    if ((char*)h == heap_start) return nullptr;
    size_t prev_size_and_flags = *(size_t*)((char*)h - footer_size());
    size_t prev_size = prev_size_and_flags & ~ALLOCATED_FLAG;
    return (BlockHeader*)((char*)h - prev_size);
}

static char* heap_start = nullptr;
static char* heap_end = nullptr;
static Atomic<char*> heap_current(nullptr);

static BlockHeader* free_head = nullptr; // head of free-list
static SpinLock heap_lock;
static size_t heap_used_bytes = 0;

static inline void free_list_insert(BlockHeader* b) {
    b->prev_free = nullptr;
    b->next_free = free_head;
    if (free_head) free_head->prev_free = b;
    free_head = b;
}

static inline void free_list_remove(BlockHeader* b) {
    if (b->prev_free) b->prev_free->next_free = b->next_free;
    else free_head = b->next_free;
    if (b->next_free) b->next_free->prev_free = b->prev_free;
    b->prev_free = b->next_free = nullptr;
}

static BlockHeader* coalesce(BlockHeader* b) {
    // Merge with next if free
    BlockHeader* n = next_block(b, heap_end);
    if (n && !block_is_allocated(n)) {
        free_list_remove(n);
        size_t new_size = block_total_size(b) + block_total_size(n);
        b->size_and_flags = (new_size | 0); // keep free
        write_footer(b);
    }
    // Merge with prev if free
    BlockHeader* p = prev_block(b, heap_start);
    if (p && !block_is_allocated(p)) {
        free_list_remove(p);
        size_t new_size = block_total_size(p) + block_total_size(b);
        p->size_and_flags = (new_size | 0);
        write_footer(p);
        b = p;
    }
    return b;
}

void heap_init() {
    heap_start = __heap_start;
    heap_end = __heap_end;
    heap_current.set(heap_start);
    heap_used_bytes = 0;

    size_t region_size = (size_t)(heap_end - heap_start);
    size_t total_size_aligned = align_up(region_size, HEAP_ALIGN);

    if (total_size_aligned < header_aligned_size() + footer_size() + HEAP_ALIGN) {
        panic("heap_init: Heap too small");
    }

    // Create a single large free block spanning the entire heap region.
    BlockHeader* b = (BlockHeader*)heap_start;
    b->size_and_flags = (total_size_aligned & ~ALLOCATED_FLAG);
    b->prev_free = b->next_free = nullptr;
    write_footer(b);
    free_head = b;

    printf("Heap initialized: start=0x%llx, end=0x%llx, size=%zu MB\n",
           (uint64_t)heap_start, (uint64_t)heap_end, region_size / (1024 * 1024));
}

//TODO: Implement heap expansion, requires VM mapping + physical page allocator.
size_t heap_expand(size_t /*min_bytes*/) {
    
    return 0;
}

void* kmalloc(size_t size) {
    if (size == 0) return nullptr;

    size_t payload_size = align_up(size, HEAP_ALIGN);
    size_t need = header_aligned_size() + payload_size + footer_size();
    need = align_up(need, HEAP_ALIGN);

    LockGuard<SpinLock> g(heap_lock);

    // First-fit search
    BlockHeader* cur = free_head;
    while (cur && block_total_size(cur) < need) {
        cur = cur->next_free;
    }

    if (!cur) {
        // Try to grow (stub)
        if (heap_expand(need) == 0) {
            panic("kmalloc: Out of heap memory! Requested %zu bytes\n", size);
            return nullptr;
        }
        // Retry after expansion
        cur = free_head;
        while (cur && block_total_size(cur) < need) cur = cur->next_free;
        if (!cur) {
            panic("kmalloc: Out of heap memory after expand! Requested %zu bytes\n", size);
            return nullptr;
        }
    }

    // Remove chosen block from free list
    free_list_remove(cur);

    size_t cur_size = block_total_size(cur);
    size_t remain = (cur_size > need) ? (cur_size - need) : 0;

    if (remain >= (header_aligned_size() + footer_size() + HEAP_ALIGN)) {
        // Split: allocated part at front, remainder becomes a new free block
        BlockHeader* alloc = cur;
        alloc->size_and_flags = (need | ALLOCATED_FLAG);
        write_footer(alloc);

        BlockHeader* rest = (BlockHeader*)((char*)alloc + need);
        rest->size_and_flags = (remain & ~ALLOCATED_FLAG);
        rest->prev_free = rest->next_free = nullptr;
        write_footer(rest);
        free_list_insert(rest);

        heap_used_bytes += need;
        void* payload = payload_from_block(alloc);
        // Zero-initialize
        for (size_t i = 0; i < payload_size; i++) {
            ((char*)payload)[i] = 0;
        }
        return payload;
    } else {
        // Use entire block
        cur->size_and_flags = (cur_size | ALLOCATED_FLAG);
        write_footer(cur);
        heap_used_bytes += cur_size;
        void* payload = payload_from_block(cur);
        for (size_t i = 0; i < payload_size; i++) {
            ((char*)payload)[i] = 0;
        }
        return payload;
    }
}

void kfree(void* ptr) {
    if (!ptr) return;

    LockGuard<SpinLock> g(heap_lock);

    BlockHeader* b = block_from_payload(ptr);
    if (!block_is_allocated(b)) {
        panic("kfree: double free or invalid pointer %llx\n", (uint64_t)ptr);
        return;
    }

    size_t freed = block_total_size(b);

    set_block_allocated(b, false);
    write_footer(b);

    // Merge with neighbors if possible, then insert into free list
    BlockHeader* merged = coalesce(b);
    free_list_insert(merged);

    if (heap_used_bytes >= freed) heap_used_bytes -= freed; else heap_used_bytes = 0;
}

size_t get_heap_used() {
    LockGuard<SpinLock> g(heap_lock);
    return heap_used_bytes;
}

size_t get_heap_free() {
    LockGuard<SpinLock> g(heap_lock);
    size_t total = (size_t)(heap_end - heap_start);
    return (total >= heap_used_bytes) ? (total - heap_used_bytes) : 0;
}

void* get_heap_start() { return heap_start; }
void* get_heap_current() { return heap_current.get(); }
void* get_heap_end() { return heap_end; }

void* operator new(unsigned long size) { return kmalloc(size); }
void* operator new[](unsigned long size) { return kmalloc(size); }

void operator delete(void* ptr) { kfree(ptr); }
void operator delete[](void* ptr) { kfree(ptr); }
void operator delete(void* ptr, unsigned long /*size*/) { kfree(ptr); }
void operator delete[](void* ptr, unsigned long /*size*/) { kfree(ptr); }
