#include "uart.h"
#include "printf.h"
#include "stdint.h"

/**
 * common exception handler
 */

 extern "C" void exc_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long far)
 {
    printf("\n=== Exception Handler Triggered ===\n");
    printf("Type   : %lu\n", type);
    printf("ESR_EL1: 0x%lx\n", esr);
    printf("ELR_EL1: 0x%lx\n", elr);
    printf("SPSR_EL1: 0x%lx\n", spsr);
    printf("FAR_EL1: 0x%lx\n", far);
            
     // no return from exception for now
     while(1);
 }
