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
#include "test.h"
#include "heap.h"
#include "timer.h"
#include "sched.h"

struct Stack {
    static constexpr int BYTES = 4096;
    uint64_t bytes[BYTES] __attribute__ ((aligned(16)));
};

PerCPU<Stack> stacks;

static bool smpInitDone = false;
static bool null_protection_enabled = false;

void kernel_init();

extern void register_all_tests();

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
    
    heap_init();
    printf("Heap allocator initialized!\n");

    sched_init();

    
    smpInitDone = true;
    clean_dcache_line(&smpInitDone);
    wake_up_cores();
    enable_null_pointer_protection();
    null_protection_enabled = true;
    clean_dcache_line(&null_protection_enabled);

    // Start core 0 as well
    kernel_init();
}

void kernel_init(){

    // demo threads
    auto worker = [](void* a){
        (void)a;
        uint64_t last = 0;
        while (1) {
            uint64_t t = timer_ticks();
            if (t - last >= 100) { // ~1s if 10ms tick
                printf("heartbeat core=%lld t=%llu\n", getCoreID(), t);
                last = t;
            }
            sched_yield(); // be polite
        }
    };
    if(getCoreID() == 0) {
    thread_create(worker, (void*)(uint64_t)0, 0);
    }
    if(getCoreID() == 1) {
        thread_create(worker, (void*)(uint64_t)1, 1);
    }
    if(getCoreID() == 2) {
        thread_create(worker, (void*)(uint64_t)2, 2);
    }
    if(getCoreID() == 3) {
        thread_create(worker, (void*)(uint64_t)3, 3);
    }
    asm volatile("msr daifclr, #2");
    timer_init_qA7(10000);
    sched_start_cpu();
    // unused now for tests
    while(true) asm volatile("wfe");
}

