#ifndef CONTEXT_H
#define CONTEXT_H
#include "stdint.h"

// ARM callee saved registers
struct CpuContext{
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64_t x29; //frame pointer
    uint64_t x30; //link register
    uint64_t sp; //stack pointer
};
#endif