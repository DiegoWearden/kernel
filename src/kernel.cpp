#include "uart.h"
#include "utils.h"
#include "printf.h"
#include "percpu.h"
#include "stdint.h"
#include "entry.h"
#include "core.h"

struct Stack {
    static constexpr int BYTES = 4096;
    uint64_t bytes[BYTES] __attribute__ ((aligned(16)));
};

PerCPU<Stack> stacks;

static bool smpInitDone = false;

void kernel_init();

extern "C" uint64_t pickKernelStack(void) {
    return (uint64_t) &stacks.forCPU(smpInitDone ? getCoreID() : 0).bytes[Stack::BYTES];
}


extern "C" void secondary_kernel_init(){
    kernel_init();
}

extern "C" void primary_kernel_init() {
        uart_init();
        uart_puts("uart initalized!!!\n\r");
        init_printf(nullptr, uart_putc_wrapper);
        printf("printf initialized!!!\n");
        // Initialize MMU page tables
        // Initialize heap
        smpInitDone = true;
        wake_up_cores();
        kernel_init();
}

void kernel_init(){
    // will get race conditions here, in order to create locks, we will need the mmu enables to run atomic instruction, which is the next part
    printf("Hi, I'm core %d\n", getCoreID());
    printf("in EL: %d\n", get_el());
}

