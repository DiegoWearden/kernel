#include "uart.h"
#include "utils.h"
#include "printf.h"
#include "percpu.h"
#include "stdint.h"
#include "entry.h"
#include "core.h"
#include "vm.h"
#include "atomic.h"
#include "dcache.h"

struct Stack {
    static constexpr int BYTES = 4096;
    uint64_t bytes[BYTES] __attribute__ ((aligned(16)));
};

PerCPU<Stack> stacks;

static bool smpInitDone = false;
static bool null_protection_enabled = false;


void kernel_init();

SpinLock lock;

extern "C" uint64_t pickKernelStack(void) {
    return (uint64_t) &stacks.forCPU(smpInitDone ? getCoreID() : 0).bytes[Stack::BYTES];
}

// Returns true if 'va' has a valid translation (i.e., is mapped)
bool va_is_mapped(uint64_t va) {
    asm volatile("at S1E1R, %0" :: "r"(va));         // translate EL1 read
    uint64_t par;
    asm volatile("mrs %0, par_el1" : "=r"(par));
    return !(par & 1); // bit0 == 1 => fault (unmapped); 0 => valid
}

// Dump contiguous regions in [start, end) with page granularity
void dump_null_region_mapping(uint64_t start, uint64_t end, uint64_t step) {
    printf("Mapping around NULL (0x%llx - 0x%llx) step 0x%llx:\n", start, end, step);
    bool last_mapped = va_is_mapped(start);
    printf("  0x%08llx: %s\n", start, last_mapped ? "mapped" : "unmapped");

    for (uint64_t va = start + step; va < end; va += step) {
        bool mapped = va_is_mapped(va);
        if (mapped != last_mapped) {
            printf("  boundary at 0x%08llx now %s\n", va, mapped ? "mapped" : "unmapped");
            last_mapped = mapped;
        }
    }
}

// More verbose: prints each VA in [start, end) with its mapping status.
void dump_null_region_mapping_explicit(uint64_t start, uint64_t end, uint64_t step) {
    printf("Explicit mapping around NULL (0x%llx - 0x%llx):\n", start, end);
    for (uint64_t va = start; va < end; va += step) {
        asm volatile("at S1E1R, %0" :: "r"(va)); // Translate VA
        uint64_t par;
        asm volatile("mrs %0, par_el1" : "=r"(par));
        bool mapped = !(par & 1); // bit0==1 means fault/unmapped
        printf("  0x%08llx: %s (PAR=0x%016llx)\n", va, mapped ? "mapped" : "unmapped", par);
    }
}



extern "C" void secondary_kernel_init(){
    while(!null_protection_enabled);
    init_mmu();
    kernel_init();
}

extern "C" void primary_kernel_init() {
    create_page_tables();
    patch_page_tables();
    init_mmu();
    uart_init();
    init_printf(nullptr, uart_putc_wrapper);
    printf("printf initialized!!!\n");
    
        // Initialize heap
    smpInitDone = true;
    clean_dcache_line(&smpInitDone);
    wake_up_cores();
    enable_null_pointer_protection();
    null_protection_enabled = true;
    clean_dcache_line(&null_protection_enabled);
    kernel_init();
}

void kernel_init(){
    lock.lock();
    printf("Hi, I'm core %d\n", getCoreID());
    printf("in EL: %d\n", get_el());
    printf("Stack Pointer: %llx\n", get_sp());
    lock.unlock();
    if(getCoreID() == 0){
        uint64_t* null_ptr = (uint64_t*)0x0;
        *null_ptr = 0xDEADBEEF;
    }
    if(getCoreID() == 1){
        uint64_t* null_ptr = (uint64_t*)0xf0;
        *null_ptr = 0xDEADBEEF;
    }
    if(getCoreID() == 2){
        uint64_t* null_ptr = (uint64_t*)0xe8;
        *null_ptr = 0xDEADBEEF;
    }
    if(getCoreID() == 3){
        uint64_t* null_ptr = (uint64_t*)0xe0;
        *null_ptr = 0xDEADBEEF;
    }
    while(true) {
        asm volatile("wfe");
    }
}

