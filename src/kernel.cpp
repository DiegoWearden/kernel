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

