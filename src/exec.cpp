#include "uart.h"
#include "printf.h"
#include "stdint.h"
#include "atomic.h"
#include "utils.h"
#include "sched.h"

SpinLock exc_lock;

// Function to decode ESR_EL1 exception class
const char* get_exception_class_name(uint32_t ec) {
    switch(ec) {
        case 0x00: return "Unknown";
        case 0x01: return "Trapped WFI/WFE";
        case 0x03: return "Trapped MCR/MRC (CP15)";
        case 0x04: return "Trapped MCRR/MRRC (CP15)";
        case 0x05: return "Trapped MCR/MRC (CP14)";
        case 0x06: return "Trapped LDC/STC (CP14)";
        case 0x07: return "Trapped FP/SIMD";
        case 0x0C: return "Trapped MRRC (CP14)";
        case 0x0E: return "Illegal Execution";
        case 0x11: return "SVC from AArch32";
        case 0x12: return "HVC from AArch32";
        case 0x13: return "SMC from AArch32";
        case 0x15: return "SVC from AArch64";
        case 0x16: return "HVC from AArch64";
        case 0x17: return "SMC from AArch64";
        case 0x18: return "Trapped MSR/MRS/System";
        case 0x19: return "Trapped SVE";
        case 0x1F: return "Implementation Defined";
        case 0x20: return "Instruction Abort (lower EL)";
        case 0x21: return "Instruction Abort (same EL)";
        case 0x22: return "PC Alignment Fault";
        case 0x24: return "Data Abort (lower EL)";
        case 0x25: return "Data Abort (same EL)";
        case 0x26: return "SP Alignment Fault";
        case 0x28: return "FP Exception (AArch32)";
        case 0x2C: return "FP Exception (AArch64)";
        case 0x2F: return "SError";
        case 0x30: return "Breakpoint (lower EL)";
        case 0x31: return "Breakpoint (same EL)";
        case 0x32: return "Software Step (lower EL)";
        case 0x33: return "Software Step (same EL)";
        case 0x34: return "Watchpoint (lower EL)";
        case 0x35: return "Watchpoint (same EL)";
        case 0x38: return "BKPT (AArch32)";
        case 0x3A: return "Vector Catch (AArch32)";
        case 0x3C: return "BRK (AArch64)";
        default: return "Reserved/Unknown";
    }
}

extern "C" void exc_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long far)
{
    exc_lock.lock();
    
    // Decode ESR_EL1
    uint32_t ec = (esr >> 26) & 0x3F;  // Exception Class (bits 31:26)
    uint32_t il = (esr >> 25) & 0x1;   // Instruction Length (bit 25)
    uint32_t iss = esr & 0x1FFFFFF;    // Instruction Specific Syndrome (bits 24:0)
    
    printf("\n=== EXCEPTION CAUGHT ===\n");
    printf("Core   : %d\n", getCoreID());
    printf("Type   : %lu\n", type);
    printf("ESR_EL1: 0x%lx (Exception Syndrome Register)\n", esr);
    printf("  - Exception Class (EC): 0x%02x (%s)\n", ec, get_exception_class_name(ec));
    printf("  - Instruction Length (IL): %d (%s)\n", il, il ? "32-bit" : "16-bit");
    printf("  - ISS (Instruction Specific): 0x%07x\n", iss);
    printf("ELR_EL1: 0x%lx (Exception Link Register)\n", elr);
    printf("SPSR_EL1: 0x%lx (Saved Program Status Register)\n", spsr);
    printf("FAR_EL1: 0x%lx (Fault Address Register)\n", far);
    
    // Special handling for data/instruction aborts
    if (ec == 0x24 || ec == 0x25 || ec == 0x20 || ec == 0x21) {
        uint32_t dfsc = iss & 0x3F;  // Data/Instruction Fault Status Code
        printf("  - Fault Status Code: 0x%02x\n", dfsc);
        
        // Common fault types
        switch(dfsc) {
            case 0x04: printf("    Translation fault (level 0)\n"); break;
            case 0x05: printf("    Translation fault (level 1)\n"); break;
            case 0x06: printf("    Translation fault (level 2)\n"); break;
            case 0x07: printf("    Translation fault (level 3)\n"); break;
            case 0x0D: printf("    Permission fault (level 1)\n"); break;
            case 0x0E: printf("    Permission fault (level 2)\n"); break;
            case 0x0F: printf("    Permission fault (level 3)\n"); break;
            default: printf("    Other fault type\n"); break;
        }
    }
    
    exc_lock.unlock();  
    while(1);
}

extern "C" void page_fault_handler(unsigned long elr, unsigned long spsr, unsigned long far)
{
    printf("\n=== PAGE FAULT ===\n");
    printf("Core   : %d\n", getCoreID());
    printf("Fault Address (FAR_EL1): 0x%lx\n", far);
    printf("Instruction (ELR_EL1): 0x%lx\n", elr);
    printf("Status (SPSR_EL1)    : 0x%lx\n", spsr);
    while(1);
}

extern "C" void syscall_handler(unsigned long sp)
{
    // For now, just print a message.
    printf("Syscall received on Core %d. SP: 0x%lx\n", getCoreID(), sp);
}

extern "C" void timer_irq_handler();

static volatile int irq_print_budget = 4;

extern "C" void handle_irq(unsigned long sp)
{
    // Timer interrupt handler will ack; then drive scheduler tick
    timer_irq_handler();
    sched_tick(sp);
}
