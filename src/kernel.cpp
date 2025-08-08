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

struct Stack {
    static constexpr int BYTES = 4096;
    uint64_t bytes[BYTES] __attribute__ ((aligned(16)));
};

PerCPU<Stack> stacks;

static bool allowStackInit = false;
static bool smpInitDone = false;

void kernel_init();

extern void register_all_tests();

SpinLock lock;

extern "C" uint64_t pickKernelStack(void) {
    return (uint64_t) &stacks.forCPU(allowStackInit ? getCoreID() : 0).bytes[Stack::BYTES];
}

extern "C" void secondary_kernel_init(){
    while(!smpInitDone);
    init_mmu();
    kernel_init();
}

extern "C" void primary_kernel_init() {
    create_page_tables();
    patch_page_tables();

    // Wake up secondary cores via spin tables before enabling MMU
    allowStackInit = true;
    clean_dcache_line(&allowStackInit);
    wake_up_cores();

    init_mmu();

    uart_init();
    init_printf(nullptr, uart_putc_wrapper);
    printf("printf initialized!!!\n");

    heap_init();
    printf("Heap allocator initialized!\n");

    // Signal secondaries it's safe to enable MMU on their side
    smpInitDone = true;
    clean_dcache_line(&smpInitDone);

    kernel_init();
}

void kernel_init(){
    uint64_t core_id = getCoreID();
    lock.lock();    
    printf("\n=== CORE %lld: STARTING MULTI-CORE KERNEL TESTS ===\n", core_id);
    printf("Each core will run the same comprehensive test suite...\n");
    printf("Note: Memory protection tests may cause expected page faults\n\n");
    
    register_all_tests();
    printf("=== CORE %lld: RUNNING TESTS ===\n", core_id);
    
    TestFramework::run_all_tests();

    printf("\n=== CORE %lld: ALL TESTS COMPLETED ===\n", core_id);
    printf("Core %lld entering idle state.\n\n", core_id);
    lock.unlock();
    

    while(true) {
        asm volatile("wfe");
    }
}
