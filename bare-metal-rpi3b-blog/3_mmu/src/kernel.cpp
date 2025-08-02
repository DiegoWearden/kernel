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

void kernel_init();

SpinLock lock;

extern "C" uint64_t pickKernelStack(void) {
    return (uint64_t) &stacks.forCPU(smpInitDone ? getCoreID() : 0).bytes[Stack::BYTES];
}


extern "C" void secondary_kernel_init(){
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
    kernel_init();
}

void kernel_init(){
    lock.lock();
    printf("Hi, I'm core %d\n", getCoreID());
    printf("in EL: %d\n", get_el());
    printf("Stack Pointer: %llx\n", get_sp());
    lock.unlock();
    if(getCoreID() == 3){
        __asm__ volatile("brk #0");
    }
}

